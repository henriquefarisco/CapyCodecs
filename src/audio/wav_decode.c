#include "capy_audio.h"
#include "vorbis_decode.h"

struct capy_wav_view {
  struct capy_audio_metadata metadata;
  const uint8_t *samples;
};

static void wav_pcm_reset(struct capy_audio_pcm *pcm) {
  if (!pcm) {
    return;
  }
  pcm->metadata.container = CAPY_AUDIO_CONTAINER_UNKNOWN;
  pcm->metadata.sample_format = CAPY_AUDIO_SAMPLE_UNKNOWN;
  pcm->metadata.sample_rate = 0;
  pcm->metadata.channels = 0;
  pcm->metadata.bits_per_sample = 0;
  pcm->metadata.block_align = 0;
  pcm->metadata.frame_count = 0;
  pcm->metadata.pcm_bytes = 0;
  pcm->samples = 0;
  pcm->allocator.alloc = 0;
  pcm->allocator.free = 0;
  pcm->allocator.user_data = 0;
}

static uint16_t wav_u16le(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t wav_u32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int wav_fourcc(const uint8_t *p, char a, char b, char c, char d) {
  return p[0] == (uint8_t)a && p[1] == (uint8_t)b &&
         p[2] == (uint8_t)c && p[3] == (uint8_t)d;
}

static int wav_limits_valid(const struct capy_audio_limits *limits) {
  return limits && limits->max_input_bytes > 0u &&
         limits->max_output_bytes > 0u && limits->max_frames > 0u &&
         limits->max_sample_rate > 0u && limits->max_channels > 0u &&
         limits->max_chunks > 0u;
}

static int wav_parse(const uint8_t *data, size_t size,
                     const struct capy_audio_limits *limits,
                     struct capy_wav_view *out) {
  size_t riff_end;
  size_t offset;
  const uint8_t *sample_data = 0;
  size_t sample_bytes = 0;
  uint32_t sample_rate = 0;
  uint32_t byte_rate = 0;
  uint16_t channels = 0;
  uint16_t block_align = 0;
  uint16_t bits_per_sample = 0;
  uint16_t format_tag = 0;
  uint32_t chunks = 0;
  int saw_fmt = 0;
  int saw_data = 0;

  if (!data || !out || !wav_limits_valid(limits)) {
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }
  out->samples = 0;
  out->metadata.container = CAPY_AUDIO_CONTAINER_UNKNOWN;
  out->metadata.sample_format = CAPY_AUDIO_SAMPLE_UNKNOWN;
  out->metadata.sample_rate = 0;
  out->metadata.channels = 0;
  out->metadata.bits_per_sample = 0;
  out->metadata.block_align = 0;
  out->metadata.frame_count = 0;
  out->metadata.pcm_bytes = 0;
  if (size > limits->max_input_bytes) {
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  }
  if (size < 12u) {
    return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  }
  if (!wav_fourcc(data, 'R', 'I', 'F', 'F') ||
      !wav_fourcc(data + 8u, 'W', 'A', 'V', 'E')) {
    return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  }
  riff_end = (size_t)wav_u32le(data + 4u);
  if (riff_end > SIZE_MAX - 8u) {
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  }
  riff_end += 8u;
  if (riff_end < 12u) {
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  if (riff_end > size) {
    return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  }

  offset = 12u;
  while (offset < riff_end) {
    uint32_t chunk_size_u32;
    size_t chunk_size;
    size_t payload;
    size_t next;
    if (++chunks > limits->max_chunks) {
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    }
    if (riff_end - offset < 8u) {
      return CAPY_AUDIO_ERR_TRUNCATED_DATA;
    }
    chunk_size_u32 = wav_u32le(data + offset + 4u);
    chunk_size = (size_t)chunk_size_u32;
    payload = offset + 8u;
    if (chunk_size > riff_end - payload) {
      return CAPY_AUDIO_ERR_TRUNCATED_DATA;
    }
    next = payload + chunk_size;
    if ((chunk_size_u32 & 1u) != 0u) {
      if (next == riff_end) {
        return CAPY_AUDIO_ERR_TRUNCATED_DATA;
      }
      ++next;
    }

    if (wav_fourcc(data + offset, 'f', 'm', 't', ' ')) {
      if (saw_fmt) {
        return CAPY_AUDIO_ERR_CORRUPT_DATA;
      }
      if (chunk_size < 16u) {
        return CAPY_AUDIO_ERR_TRUNCATED_DATA;
      }
      format_tag = wav_u16le(data + payload);
      channels = wav_u16le(data + payload + 2u);
      sample_rate = wav_u32le(data + payload + 4u);
      byte_rate = wav_u32le(data + payload + 8u);
      block_align = wav_u16le(data + payload + 12u);
      bits_per_sample = wav_u16le(data + payload + 14u);
      saw_fmt = 1;
    } else if (wav_fourcc(data + offset, 'd', 'a', 't', 'a')) {
      if (saw_data) {
        return CAPY_AUDIO_ERR_CORRUPT_DATA;
      }
      sample_data = data + payload;
      sample_bytes = chunk_size;
      saw_data = 1;
    }
    offset = next;
  }
  if (offset != riff_end || !saw_fmt || !saw_data) {
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  if (channels == 0u || sample_rate == 0u) {
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  if (channels > limits->max_channels ||
      sample_rate > limits->max_sample_rate) {
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  }
  if (format_tag == 1u) {
    if (bits_per_sample == 16u) {
      out->metadata.sample_format = CAPY_AUDIO_SAMPLE_S16_LE;
    } else if (bits_per_sample == 24u) {
      out->metadata.sample_format = CAPY_AUDIO_SAMPLE_S24_LE_PACKED;
    } else if (bits_per_sample == 32u) {
      out->metadata.sample_format = CAPY_AUDIO_SAMPLE_S32_LE;
    } else {
      return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
    }
  } else if (format_tag == 3u && bits_per_sample == 32u) {
    out->metadata.sample_format = CAPY_AUDIO_SAMPLE_F32_LE;
  } else {
    return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  }
  {
    uint32_t bytes_per_sample = (uint32_t)bits_per_sample / 8u;
    uint32_t expected_align;
    uint64_t expected_rate;
    if (bytes_per_sample == 0u ||
        (uint32_t)channels > UINT32_MAX / bytes_per_sample) {
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    }
    expected_align = (uint32_t)channels * bytes_per_sample;
    expected_rate = (uint64_t)sample_rate * expected_align;
    if (expected_align > UINT16_MAX || block_align != expected_align ||
        expected_rate > UINT32_MAX || byte_rate != (uint32_t)expected_rate) {
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    }
  }
  if (sample_bytes == 0u) {
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  if (sample_bytes > limits->max_output_bytes ||
      (block_align != 0u && sample_bytes % block_align != 0u)) {
    return sample_bytes > limits->max_output_bytes
               ? CAPY_AUDIO_ERR_RESOURCE_LIMIT
               : CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  out->metadata.frame_count = sample_bytes / block_align;
  if (out->metadata.frame_count > limits->max_frames) {
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  }
  out->metadata.container = CAPY_AUDIO_CONTAINER_WAV;
  out->metadata.sample_rate = sample_rate;
  out->metadata.channels = channels;
  out->metadata.bits_per_sample = bits_per_sample;
  out->metadata.block_align = block_align;
  out->metadata.pcm_bytes = sample_bytes;
  out->samples = sample_data;
  return CAPY_AUDIO_OK;
}

int capy_audio_query_memory(const uint8_t *data, size_t size,
                            const struct capy_audio_limits *limits,
                            struct capy_audio_metadata *out_metadata) {
  struct capy_audio_limits defaults;
  struct capy_wav_view view;
  int rc;
  if (!out_metadata) {
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }
  out_metadata->container = CAPY_AUDIO_CONTAINER_UNKNOWN;
  out_metadata->sample_format = CAPY_AUDIO_SAMPLE_UNKNOWN;
  out_metadata->sample_rate = 0;
  out_metadata->channels = 0;
  out_metadata->bits_per_sample = 0;
  out_metadata->block_align = 0;
  out_metadata->frame_count = 0;
  out_metadata->pcm_bytes = 0;
  if (!limits) {
    capy_audio_default_limits(&defaults);
    limits = &defaults;
  }
  if (data && size >= 4u && wav_fourcc(data, 'O', 'g', 'g', 'S'))
    return capy_vorbis_query_memory(data, size, limits, out_metadata);
  rc = wav_parse(data, size, limits, &view);
  if (rc == CAPY_AUDIO_OK) {
    *out_metadata = view.metadata;
  }
  return rc;
}

int capy_audio_decode_memory_limited(
    const uint8_t *data, size_t size,
    const struct capy_audio_allocator *allocator,
    const struct capy_audio_limits *limits, struct capy_audio_pcm *out) {
  struct capy_audio_limits defaults;
  struct capy_wav_view view;
  uint8_t *samples;
  int rc;
  if (!out) {
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }
  wav_pcm_reset(out);
  if (!allocator || !allocator->alloc || !allocator->free) {
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }
  if (!limits) {
    capy_audio_default_limits(&defaults);
    limits = &defaults;
  }
  if (data && size >= 4u && wav_fourcc(data, 'O', 'g', 'g', 'S')) {
    const struct capy_vorbis_decode_budget budget = {16u << 20, limits->max_chunks, 16000000};
    return capy_vorbis_decode_memory(data, size, allocator, limits, &budget, out);
  }
  rc = wav_parse(data, size, limits, &view);
  if (rc != CAPY_AUDIO_OK) {
    return rc;
  }
  if (view.metadata.pcm_bytes == 0u) {
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  samples = (uint8_t *)allocator->alloc(view.metadata.pcm_bytes,
                                        allocator->user_data);
  if (!samples) {
    return CAPY_AUDIO_ERR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < view.metadata.pcm_bytes; ++i) {
    samples[i] = view.samples[i];
  }
  out->metadata = view.metadata;
  out->samples = samples;
  out->allocator = *allocator;
  return CAPY_AUDIO_OK;
}

int capy_audio_decode_memory(const uint8_t *data, size_t size,
                             const struct capy_audio_allocator *allocator,
                             struct capy_audio_pcm *out) {
  struct capy_audio_limits limits;
  capy_audio_default_limits(&limits);
  return capy_audio_decode_memory_limited(data, size, allocator, &limits, out);
}

#include "capy_audio.h"

static void capy_audio_metadata_reset(struct capy_audio_metadata *metadata) {
  if (!metadata) {
    return;
  }
  metadata->container = CAPY_AUDIO_CONTAINER_UNKNOWN;
  metadata->sample_format = CAPY_AUDIO_SAMPLE_UNKNOWN;
  metadata->sample_rate = 0;
  metadata->channels = 0;
  metadata->bits_per_sample = 0;
  metadata->block_align = 0;
  metadata->frame_count = 0;
  metadata->pcm_bytes = 0;
}

static void capy_audio_pcm_reset(struct capy_audio_pcm *pcm) {
  if (!pcm) {
    return;
  }
  capy_audio_metadata_reset(&pcm->metadata);
  pcm->samples = 0;
  pcm->allocator.alloc = 0;
  pcm->allocator.free = 0;
  pcm->allocator.user_data = 0;
}

uint32_t capy_audio_abi_version(void) { return CAPY_AUDIO_ABI_VERSION; }

uint32_t capy_audio_codec_features(void) {
  return CAPY_AUDIO_FEATURE_WAV_PCM_DECODE |
         CAPY_AUDIO_FEATURE_INTERLEAVED_OUTPUT |
         CAPY_AUDIO_FEATURE_ALLOCATOR_INJECTION |
         CAPY_AUDIO_FEATURE_PER_CALL_LIMITS | CAPY_AUDIO_FEATURE_METADATA |
         CAPY_AUDIO_FEATURE_OGG_VORBIS_DECODE;
}

void capy_audio_default_limits(struct capy_audio_limits *limits) {
  if (!limits) {
    return;
  }
  limits->max_input_bytes = CAPY_AUDIO_MAX_OUTPUT_BYTES + 1024u * 1024u;
  limits->max_output_bytes = CAPY_AUDIO_MAX_OUTPUT_BYTES;
  limits->max_frames = (uint64_t)CAPY_AUDIO_MAX_SAMPLE_RATE * 600u;
  limits->max_sample_rate = CAPY_AUDIO_MAX_SAMPLE_RATE;
  limits->max_channels = CAPY_AUDIO_MAX_CHANNELS;
  limits->max_chunks = CAPY_AUDIO_MAX_CHUNKS;
}

int capy_audio_detect_memory(const uint8_t *data, size_t size,
                             enum capy_audio_container *out_container) {
  if (!data || !out_container) {
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }
  *out_container = CAPY_AUDIO_CONTAINER_UNKNOWN;
  if (size >= 4u && data[0] == 'O' && data[1] == 'g' && data[2] == 'g' && data[3] == 'S') {
    /* Container detection only; query/decode validates Vorbis and page CRCs. */
    *out_container = CAPY_AUDIO_CONTAINER_OGG_VORBIS;
    return CAPY_AUDIO_OK;
  }
  if (size < 12u) {
    return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  }
  if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F' ||
      data[8] != 'W' || data[9] != 'A' || data[10] != 'V' ||
      data[11] != 'E') {
    return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  }
  *out_container = CAPY_AUDIO_CONTAINER_WAV;
  return CAPY_AUDIO_OK;
}

const char *capy_audio_strerror(enum capy_audio_error error) {
  switch (error) {
  case CAPY_AUDIO_OK:
    return "no error";
  case CAPY_AUDIO_ERR_INVALID_ARGUMENT:
    return "invalid argument";
  case CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT:
    return "unsupported audio format";
  case CAPY_AUDIO_ERR_CORRUPT_DATA:
    return "corrupt audio data";
  case CAPY_AUDIO_ERR_TRUNCATED_DATA:
    return "truncated audio data";
  case CAPY_AUDIO_ERR_OUT_OF_MEMORY:
    return "out of memory";
  case CAPY_AUDIO_ERR_RESOURCE_LIMIT:
    return "audio exceeds resource limit";
  }
  return "unknown audio error";
}

const char *capy_audio_sample_format_name(enum capy_audio_sample_format format) {
  switch (format) {
  case CAPY_AUDIO_SAMPLE_UNKNOWN:
    return "unknown";
  case CAPY_AUDIO_SAMPLE_S16_LE:
    return "s16le";
  case CAPY_AUDIO_SAMPLE_S24_LE_PACKED:
    return "s24le-packed";
  case CAPY_AUDIO_SAMPLE_S32_LE:
    return "s32le";
  case CAPY_AUDIO_SAMPLE_F32_LE:
    return "f32le";
  }
  return "unknown";
}

void capy_audio_pcm_free(struct capy_audio_pcm *pcm) {
  if (!pcm) {
    return;
  }
  if (pcm->samples && pcm->allocator.free) {
    pcm->allocator.free(pcm->samples, pcm->allocator.user_data);
  }
  capy_audio_pcm_reset(pcm);
}

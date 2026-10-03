#include "vorbis_decode.h"
#include "vorbis_headers.h"
#include "ogg_reader.h"

/* Metadata query validates Ogg framing/CRC, header signatures and identification,
 * not entropy/setup support. Only decode establishes PCM decodability. */
int capy_vorbis_query_memory(const uint8_t *data, size_t size,
    const struct capy_audio_limits *limits, struct capy_audio_metadata *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_audio_metadata){0};
  if (!data || !limits || !limits->max_input_bytes || !limits->max_output_bytes ||
      !limits->max_frames || !limits->max_channels || !limits->max_sample_rate ||
      !limits->max_chunks) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (size > CAPY_AUDIO_MAX_OUTPUT_BYTES) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  struct capy_ogg_reader ogg;
  struct capy_vorbis_identification id = {0};
  uint8_t prefix[30];
  size_t bytes = 0;
  uint32_t packets = 0;
  uint64_t granule = UINT64_MAX;
  int rc = capy_ogg_reader_init(&ogg, data, size, limits->max_input_bytes,
                               1u << 20, limits->max_chunks);
  if (rc) return rc;
  while ((rc = capy_ogg_reader_next_prefix(&ogg, prefix, sizeof(prefix), &bytes)) == 1) {
    if (packets >= 65539u || (packets >= 3u && packets - 3u >= limits->max_chunks))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    if (packets < 3u) {
      unsigned type = packets * 2u + 1u;
      if (bytes < 7u || prefix[0] != type || prefix[1] != 'v' || prefix[2] != 'o' ||
          prefix[3] != 'r' || prefix[4] != 'b' || prefix[5] != 'i' || prefix[6] != 's')
        return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
      if (packets == 0u) {
        if (bytes != sizeof(prefix)) return CAPY_AUDIO_ERR_CORRUPT_DATA;
        rc = capy_vorbis_parse_identification(prefix, bytes, limits, &id);
        if (rc) return rc;
      }
    } else if (ogg.packet_has_granule) {
      if (granule != UINT64_MAX && ogg.packet_granule < granule)
        return CAPY_AUDIO_ERR_CORRUPT_DATA;
      granule = ogg.packet_granule;
    }
    ++packets;
  }
  if (rc < 0) return rc;
  if (packets < 4u || granule == UINT64_MAX || !granule || ogg.page_granule != granule)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (granule > limits->max_frames || granule > limits->max_output_bytes / (id.channels*2u) ||
      granule > CAPY_AUDIO_MAX_OUTPUT_BYTES / (id.channels*2u))
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  *out = (struct capy_audio_metadata){.container = CAPY_AUDIO_CONTAINER_OGG_VORBIS,
    .sample_format = CAPY_AUDIO_SAMPLE_S16_LE, .sample_rate = id.sample_rate,
    .channels = id.channels, .bits_per_sample = 16, .block_align = id.channels*2u,
    .frame_count = granule, .pcm_bytes = (size_t)granule * id.channels*2u};
  return 0;
}

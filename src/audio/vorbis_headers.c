#include "vorbis_headers.h"

static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
         (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int header(const uint8_t *p, size_t size,
                  const struct capy_audio_limits *limits, unsigned type) {
  if (!p || !limits || !limits->max_input_bytes)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (size > limits->max_input_bytes) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (size < 7u) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  if (p[0] != type || p[1] != 'v' || p[2] != 'o' || p[3] != 'r' ||
      p[4] != 'b' || p[5] != 'i' || p[6] != 's')
    return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  return 0;
}

int capy_vorbis_parse_identification(const uint8_t *p, size_t size,
    const struct capy_audio_limits *limits,
    struct capy_vorbis_identification *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_identification){0};
  int rc = header(p, size, limits, 1u);
  if (rc) return rc;
  if (!limits->max_channels || !limits->max_sample_rate)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (size < 30u) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  if (le32(p + 7u) != 0u) return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  unsigned small = p[28] & 15u, large = p[28] >> 4;
  uint32_t rate = le32(p + 12u);
  if (!p[11] || !rate || small < 6u || large > 13u || small > large ||
      !(p[29] & 1u)) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (p[11] > limits->max_channels || p[11] > CAPY_AUDIO_MAX_CHANNELS ||
      rate > limits->max_sample_rate || rate > CAPY_AUDIO_MAX_SAMPLE_RATE)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  /* Bitrate hints are signed, optional and irrelevant to allocation. */
  out->sample_rate = rate;
  out->channels = p[11];
  out->blocksize_small = (uint16_t)(1u << small);
  out->blocksize_large = (uint16_t)(1u << large);
  return 0;
}

int capy_vorbis_parse_comments(const uint8_t *p, size_t size,
    const struct capy_audio_limits *limits, struct capy_vorbis_comments *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_comments){0};
  int rc = header(p, size, limits, 3u);
  if (rc) return rc;
  if (!limits->max_chunks) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  size_t offset = 7u, total = 0;
  if (size - offset < 4u) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  uint32_t vendor = le32(p + offset);
  offset += 4u;
  if (vendor > size - offset) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  offset += vendor;
  if (size - offset < 4u) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  uint32_t count = le32(p + offset);
  offset += 4u;
  if (count > limits->max_chunks) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  for (uint32_t i = 0; i < count; ++i) {
    if (size - offset < 4u) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
    uint32_t length = le32(p + offset);
    offset += 4u;
    if (length > size - offset) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
    offset += length;
    total += length; /* Disjoint ranges, sum cannot exceed bounded packet. */
  }
  if (offset == size) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  if (!(p[offset] & 1u)) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  out->vendor_bytes = vendor;
  out->comment_count = count;
  out->comment_bytes = total;
  return 0;
}

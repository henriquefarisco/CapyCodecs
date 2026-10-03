#include "vorbis_headers.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t ident[] = {
  1,'v','o','r','b','i','s',0,0,0,0,2,0x80,0xbb,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0xb8,1
};
static const uint8_t comments[] = {
  3,'v','o','r','b','i','s',3,0,0,0,'a','b','c',2,0,0,0,
  3,0,0,0,'A','=','B',0,0,0,0,1
};
int main(void) {
  struct capy_audio_limits limits;
  struct capy_vorbis_identification id;
  struct capy_vorbis_comments tags;
  uint8_t data[128];
  capy_audio_default_limits(&limits);
  assert(capy_vorbis_parse_identification(ident, sizeof(ident), &limits, &id) == 0);
  assert(id.sample_rate == 48000 && id.channels == 2 &&
         id.blocksize_small == 256 && id.blocksize_large == 2048);
  assert(capy_vorbis_parse_comments(comments, sizeof(comments), &limits, &tags) == 0);
  assert(tags.vendor_bytes == 3 && tags.comment_count == 2 && tags.comment_bytes == 3);
  for (size_t i = 0; i < sizeof(ident); ++i) {
    assert(capy_vorbis_parse_identification(ident, i, &limits, &id) < 0);
    assert(!id.sample_rate && !id.channels && !id.blocksize_large);
  }
  for (size_t i = 0; i < sizeof(comments); ++i) {
    assert(capy_vorbis_parse_comments(comments, i, &limits, &tags) < 0);
    assert(!tags.vendor_bytes && !tags.comment_count && !tags.comment_bytes);
  }
  memcpy(data, ident, sizeof(ident));
  for (unsigned small = 0; small < 16; ++small)
    for (unsigned large = 0; large < 16; ++large) {
      data[28] = (uint8_t)(small | (large << 4));
      int rc = capy_vorbis_parse_identification(data, sizeof(ident), &limits, &id);
      assert((rc == 0) == (small >= 6 && large <= 13 && small <= large));
    }
  memcpy(data, ident, sizeof(ident));
  data[7] = 1;
  assert(capy_vorbis_parse_identification(data, sizeof(ident), &limits, &id) == CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT);
  data[7] = 0; data[11] = 0;
  assert(capy_vorbis_parse_identification(data, sizeof(ident), &limits, &id) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  data[11] = 2; data[29] = 0;
  assert(capy_vorbis_parse_identification(data, sizeof(ident), &limits, &id) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  limits.max_channels = 1;
  assert(capy_vorbis_parse_identification(ident, sizeof(ident), &limits, &id) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  limits.max_channels = 8; limits.max_sample_rate = 44100;
  assert(capy_vorbis_parse_identification(ident, sizeof(ident), &limits, &id) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  capy_audio_default_limits(&limits);
  limits.max_chunks = 1;
  assert(capy_vorbis_parse_comments(comments, sizeof(comments), &limits, &tags) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  capy_audio_default_limits(&limits);
  const size_t offsets[] = {7,14,18,25};
  for (size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
    memcpy(data, comments, sizeof(comments));
    memset(data + offsets[i], 255, 4);
    assert(capy_vorbis_parse_comments(data, sizeof(comments), &limits, &tags) < 0);
    assert(!tags.comment_count);
  }
  memcpy(data, comments, sizeof(comments)); data[sizeof(comments)-1] = 0;
  assert(capy_vorbis_parse_comments(data, sizeof(comments), &limits, &tags) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  assert(capy_vorbis_parse_comments(0, 0, &limits, &tags) == CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  limits.max_input_bytes = sizeof(comments) - 1;
  assert(capy_vorbis_parse_comments(comments, sizeof(comments), &limits, &tags) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  capy_audio_default_limits(&limits);
  uint32_t seed = 0x68429031;
  for (unsigned trial = 0; trial < 10000; ++trial) {
    memcpy(data, ident, sizeof(ident));
    seed = seed * 1664525u + 1013904223u;
    data[seed % sizeof(ident)] ^= (uint8_t)(1u + (seed >> 24));
    int rc = capy_vorbis_parse_identification(data, sizeof(ident), &limits, &id);
    if (rc) assert(!id.sample_rate && !id.channels);
    memcpy(data, comments, sizeof(comments));
    data[seed % sizeof(comments)] ^= (uint8_t)(1u + (seed >> 24));
    rc = capy_vorbis_parse_comments(data, sizeof(comments), &limits, &tags);
    if (rc) assert(!tags.comment_bytes && !tags.vendor_bytes);
  }
  puts("[vorbis-headers] identification/comments/limits/mutations passed");
  return 0;
}

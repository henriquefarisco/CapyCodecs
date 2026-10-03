#ifndef CAPY_VORBIS_DECODE_H
#define CAPY_VORBIS_DECODE_H
#include "capy_audio.h"

/* Internal integration implementation. No I/O or global allocator. Scratch and
 * entropy work are bounded independently of the public output limits. */
struct capy_vorbis_decode_budget {
  size_t max_work_bytes;
  uint32_t max_packets;
  uint64_t max_vectors;
};
int capy_vorbis_decode_memory(const uint8_t *data, size_t size,
    const struct capy_audio_allocator *allocator,
    const struct capy_audio_limits *limits,
    const struct capy_vorbis_decode_budget *budget, struct capy_audio_pcm *out);
int capy_vorbis_query_memory(const uint8_t *data, size_t size,
    const struct capy_audio_limits *limits, struct capy_audio_metadata *out);
#endif

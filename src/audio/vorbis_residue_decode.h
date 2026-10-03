#ifndef CAPY_VORBIS_RESIDUE_DECODE_H
#define CAPY_VORBIS_RESIDUE_DECODE_H

#include "vorbis_residue.h"
#include "vorbis_setup.h"

#define CAPY_VORBIS_RESIDUE_CLASS_SLOTS \
  (CAPY_AUDIO_MAX_CHANNELS * CAPY_VORBIS_FLOOR1_BINS)

struct capy_vorbis_residue_decode_result {
  uint32_t partitions, vectors;
  uint8_t exhausted;
};

/* Private complete residue 0/1/2 pass scheduler. `out` is channel-major with
 * `channels * bins` floats. `skip[channel] != 0` suppresses that channel for
 * types 0/1; type 2 decodes the interleaved bundle when any channel is active.
 * Caller supplies a same-sized transactional output scratch plus one partition
 * scratch (two for type 2), and classification scratch. Audio exhaustion is a
 * successful partial decode. Hard failure leaves output unchanged. */
int capy_vorbis_residue_decode(
    const struct capy_vorbis_residue_config *config,
    const struct capy_vorbis_book *books,
    const struct capy_vorbis_huffman *trees, size_t book_count,
    const uint8_t *setup, size_t setup_size, struct capy_vorbis_bits *audio,
    unsigned channels, const uint8_t *skip, size_t bins,
    uint32_t max_vectors, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity,
    uint8_t *classes, size_t class_capacity,
    struct capy_vorbis_residue_decode_result *result);

#endif

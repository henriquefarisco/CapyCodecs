#ifndef CAPY_VORBIS_RESIDUE_H
#define CAPY_VORBIS_RESIDUE_H
#include "vorbis_huffman.h"
#include "vorbis_vq.h"
#define CAPY_VORBIS_RESIDUE_SCALARS 32768u
struct capy_vorbis_residue_result { uint32_t vectors; uint8_t exhausted; };
/* Private additive decode of ONE partition, type 0 (strided) or 1 (sequential).
 * Type 1 clips a final vector to count; type 0 leaves its division remainder
 * unchanged. count=1..32768, max_vectors is a work budget. Both decoded vectors
 * and resulting sums must remain within positive finite max_abs.
 * Caller supplies count floats of output and scratch, initially finite output,
 * a validated book/tree and its immutable setup packet. All regions/descriptors
 * must not overlap; scratch contents are unspecified on return.
 * Hard error: output unchanged, result cleared, consumed bits not rolled back.
 * Audio packet EOF: success, exhausted=1, complete vectors so far committed.
 * Setup truncation is a hard error, NEVER audio exhaustion. Sticky audio-reader
 * truncation is retained. Requires the same FP environment as vorbis_vq.
 * No allocation, I/O, pass/classification/channel orchestration or PCM output. */
int capy_vorbis_residue_partition(unsigned type,
    const struct capy_vorbis_book *book, const struct capy_vorbis_huffman *tree,
    const uint8_t *setup, size_t setup_size, struct capy_vorbis_bits *audio,
    size_t count, uint32_t max_vectors, float max_abs,
    float *out, size_t capacity, float *scratch, size_t scratch_capacity,
    struct capy_vorbis_residue_result *result);
#endif

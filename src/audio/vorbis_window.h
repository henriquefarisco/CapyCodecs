#ifndef CAPY_VORBIS_WINDOW_H
#define CAPY_VORBIS_WINDOW_H

#include "vorbis_packet.h"

#define CAPY_VORBIS_BLOCK_SAMPLES 8192u

/* Apply the Vorbis window to one IMDCT block. Input/output/scratch each contain
 * block_size floats and must not overlap. Output is unchanged on hard error.
 * The implementation is freestanding and does not call libm. */
int capy_vorbis_window_apply(const struct capy_vorbis_packet_window *window,
    const float *input, size_t input_count, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity);

/* Add the cached right half of a previous windowed block to the left half of
 * the current windowed block. Returns prev/4 + current/4 finished samples.
 * Input blocks are not modified; output is atomically committed via scratch. */
int capy_vorbis_overlap_add(const float *previous, size_t previous_size,
    const float *current, size_t current_size, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity,
    size_t *produced);

#endif

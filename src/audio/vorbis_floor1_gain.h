#ifndef CAPY_VORBIS_FLOOR1_GAIN_H
#define CAPY_VORBIS_FLOOR1_GAIN_H

#include "vorbis_floor1.h"

/* Private floor1 inverse-dB substitution and floor/residue dot product.
 * `indices` is the 0..255 curve emitted by capy_vorbis_floor1_curve.
 * `spectrum` is updated atomically through caller-owned scratch. All three
 * regions must be non-overlapping. No allocation, libm, I/O or PCM output. */
int capy_vorbis_floor1_apply_gain(
    const uint8_t *indices, size_t bins, float max_abs, float *spectrum,
    size_t spectrum_capacity, float *scratch, size_t scratch_capacity);

#endif

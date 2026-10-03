#ifndef CAPY_VORBIS_MDCT_H
#define CAPY_VORBIS_MDCT_H

#include "vorbis_window.h"

struct capy_vorbis_mdct_plan {
  unsigned n, log2n;
  float *trig;
  int32_t *bitrev;
  size_t trig_count, bitrev_count;
};

/* Build one legal power-of-two inverse-MDCT plan in nonoverlapping caller
 * storage. Required capacities are n+n/4 floats and n/4 int32 values. */
int capy_vorbis_mdct_plan_init(unsigned n, float *trig, size_t trig_capacity,
    int32_t *bitrev, size_t bitrev_capacity,
    struct capy_vorbis_mdct_plan *out);

/* Transform n/2 spectral floats into n time-domain floats. Plan/input/output/
 * scratch regions must not overlap. Scratch is the in-place work area. Output
 * is unchanged on failure. No heap, libm, I/O or callback. */
int capy_vorbis_mdct_backward(const struct capy_vorbis_mdct_plan *plan,
    const float *input, size_t input_count, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity);

#endif

#include "vorbis_coupling.h"

#include <float.h>

#ifdef __FAST_MATH__
#error "Vorbis coupling requires finite checks; disable fast-math"
#endif

static int bounded(float value, float max_abs) {
  return value <= max_abs && value >= -max_abs;
}

static int regions_overlap(const void *left, size_t left_size,
                           const void *right, size_t right_size) {
  uintptr_t left_at = (uintptr_t)left;
  uintptr_t right_at = (uintptr_t)right;
  if (left_at > UINTPTR_MAX - left_size || right_at > UINTPTR_MAX - right_size)
    return 1;
  return left_at < right_at + right_size && right_at < left_at + left_size;
}

int capy_vorbis_inverse_coupling(
    float *vectors, size_t vector_capacity, size_t channels, size_t stride,
    size_t bins, const uint8_t *magnitude, const uint8_t *angle, size_t steps,
    float max_abs, float *scratch, size_t scratch_capacity) {
  size_t used;

  if (!vectors || !channels || channels > CAPY_VORBIS_COUPLING_CHANNELS ||
      !bins || !stride || stride < bins || !(max_abs > 0.0f) ||
      max_abs > FLT_MAX)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (bins > CAPY_VORBIS_SPECTRAL_BINS ||
      (channels - 1u) > (SIZE_MAX - bins) / stride)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  used = (channels - 1u) * stride + bins;
  if (vector_capacity < used || channels > SIZE_MAX / bins)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (!steps)
    return CAPY_AUDIO_OK;
  if (!magnitude || !angle || !scratch)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (steps > CAPY_VORBIS_COUPLING_STEPS ||
      scratch_capacity < channels * bins)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (used > SIZE_MAX / sizeof(*vectors) ||
      channels * bins > SIZE_MAX / sizeof(*scratch))
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (regions_overlap(vectors, used * sizeof(*vectors), scratch,
                      channels * bins * sizeof(*scratch)) ||
      regions_overlap(vectors, used * sizeof(*vectors), magnitude, steps) ||
      regions_overlap(vectors, used * sizeof(*vectors), angle, steps) ||
      regions_overlap(scratch, channels * bins * sizeof(*scratch), magnitude,
                      steps) ||
      regions_overlap(scratch, channels * bins * sizeof(*scratch), angle,
                      steps) ||
      regions_overlap(magnitude, steps, angle, steps))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;

  for (size_t step = 0; step < steps; ++step) {
    if (magnitude[step] >= channels || angle[step] >= channels ||
        magnitude[step] == angle[step])
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  for (size_t channel = 0; channel < channels; ++channel) {
    for (size_t bin = 0; bin < bins; ++bin) {
      float value = vectors[channel * stride + bin];
      if (!bounded(value, max_abs))
        return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
      scratch[channel * bins + bin] = value;
    }
  }

  for (size_t step = steps; step-- > 0;) {
    float *mag = scratch + (size_t)magnitude[step] * bins;
    float *ang = scratch + (size_t)angle[step] * bins;
    for (size_t bin = 0; bin < bins; ++bin) {
      float m = mag[bin];
      float a = ang[bin];
      double new_m;
      double new_a;
      if (m > 0.0f) {
        if (a > 0.0f) {
          new_m = m;
          new_a = (double)m - a;
        } else {
          new_a = m;
          new_m = (double)m + a;
        }
      } else if (a > 0.0f) {
        new_m = m;
        new_a = (double)m + a;
      } else {
        new_a = m;
        new_m = (double)m - a;
      }
      if (!(new_m <= max_abs && new_m >= -(double)max_abs &&
            new_a <= max_abs && new_a >= -(double)max_abs))
        return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
      mag[bin] = (float)new_m;
      ang[bin] = (float)new_a;
    }
  }

  for (size_t channel = 0; channel < channels; ++channel)
    for (size_t bin = 0; bin < bins; ++bin)
      vectors[channel * stride + bin] = scratch[channel * bins + bin];
  return CAPY_AUDIO_OK;
}

#include "vorbis_window.h"

#include <float.h>

#ifdef __FAST_MATH__
#error "Vorbis windowing requires finite checks; disable fast-math"
#endif

#define CAPY_PI_OVER_TWO 1.57079632679489661923132169163975144

static int valid_block(size_t value) {
  return value >= 64u && value <= CAPY_VORBIS_BLOCK_SAMPLES &&
         !(value & (value - 1u));
}

/* Taylor on [0, pi/2], evaluated in binary64. The first omitted term is below
 * 5e-15 on this interval; final storage is binary32. */
static double sin_half_pi(double x) {
  double x2 = x * x;
  double p = 1.0 / 355687428096000.0;
  p = -1.0 / 1307674368000.0 + x2 * p;
  p = 1.0 / 6227020800.0 + x2 * p;
  p = -1.0 / 39916800.0 + x2 * p;
  p = 1.0 / 362880.0 + x2 * p;
  p = -1.0 / 5040.0 + x2 * p;
  p = 1.0 / 120.0 + x2 * p;
  p = -1.0 / 6.0 + x2 * p;
  return x * (1.0 + x2 * p);
}

static double slope(size_t index, size_t count) {
  double phase = ((double)index + 0.5) * CAPY_PI_OVER_TWO / (double)count;
  double inner = sin_half_pi(phase);
  return sin_half_pi(CAPY_PI_OVER_TWO * inner * inner);
}

int capy_vorbis_window_apply(const struct capy_vorbis_packet_window *window,
    const float *input, size_t input_count, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity) {
  if (!window || !input || !out || !scratch ||
      !(max_abs > 0.0f && max_abs <= FLT_MAX))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  size_t n = window->block_size;
  if (!valid_block(n) || input_count < n || out_capacity < n ||
      scratch_capacity < n || window->left_start > window->left_end ||
      window->left_end > window->right_start ||
      window->right_start > window->right_end || window->right_end > n ||
      window->left_start == window->left_end ||
      window->right_start == window->right_end)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  size_t left_n = window->left_end - window->left_start;
  size_t right_n = window->right_end - window->right_start;
  for (size_t i = 0; i < n; ++i) {
    if (!(input[i] <= max_abs && input[i] >= -max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    double coefficient;
    if (i < window->left_start || i >= window->right_end) coefficient = 0.0;
    else if (i < window->left_end)
      coefficient = slope(i - window->left_start, left_n);
    else if (i < window->right_start) coefficient = 1.0;
    else coefficient = slope(window->right_end - i - 1u, right_n);
    double product = (double)input[i] * coefficient;
    if (!(product <= max_abs && product >= -(double)max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    scratch[i] = (float)product;
  }
  for (size_t i = 0; i < n; ++i) out[i] = scratch[i];
  return CAPY_AUDIO_OK;
}

int capy_vorbis_overlap_add(const float *previous, size_t previous_size,
    const float *current, size_t current_size, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity,
    size_t *produced) {
  if (!produced) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *produced = 0;
  if (!previous || !current || !out || !scratch ||
      !valid_block(previous_size) || !valid_block(current_size) ||
      !(max_abs > 0.0f && max_abs <= FLT_MAX))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  size_t count = previous_size / 4u + current_size / 4u;
  if (out_capacity < count || scratch_capacity < count)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  for (size_t i = 0; i < previous_size; ++i)
    if (!(previous[i] <= max_abs && previous[i] >= -max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  for (size_t i = 0; i < current_size; ++i)
    if (!(current[i] <= max_abs && current[i] >= -max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;

  for (size_t i = 0; i < count; ++i) {
    size_t previous_index = previous_size / 2u + i;
    int64_t current_index = (int64_t)(current_size / 4u) -
                            (int64_t)(previous_size / 4u) + (int64_t)i;
    double value = 0.0;
    if (previous_index < previous_size) value += previous[previous_index];
    if (current_index >= 0 && (size_t)current_index < current_size / 2u)
      value += current[(size_t)current_index];
    if (!(value <= max_abs && value >= -(double)max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    scratch[i] = (float)value;
  }
  for (size_t i = 0; i < count; ++i) out[i] = scratch[i];
  *produced = count;
  return CAPY_AUDIO_OK;
}

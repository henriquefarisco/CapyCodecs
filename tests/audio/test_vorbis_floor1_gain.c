#include "vorbis_floor1_gain.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void same(const float *left, const float *right, size_t count) {
  for (size_t i = 0; i < count; ++i)
    assert(memcmp(&left[i], &right[i], sizeof(left[i])) == 0);
}

int main(void) {
  const uint8_t indices[] = {0, 128, 254, 255};
  float spectrum[] = {1.0f, -2.0f, 4.0f, -8.0f, 77.0f};
  float scratch[4];
  assert(capy_vorbis_floor1_apply_gain(indices, 4, 10.0f, spectrum, 4,
                                       scratch, 4) == 0);
  assert(spectrum[0] == 1.0649863e-07F);
  assert(spectrum[1] == -2.0F * 0.00033677814F);
  assert(spectrum[2] == 4.0F * 0.9389798F);
  assert(spectrum[3] == -8.0F);
  assert(spectrum[4] == 77.0F);

  float unchanged[] = {1.0f, 2.0f, NAN, 4.0f};
  float original[4];
  memcpy(original, unchanged, sizeof(original));
  assert(capy_vorbis_floor1_apply_gain(indices, 4, 10.0f, unchanged, 4,
                                       scratch, 4) ==
         CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  same(unchanged, original, 4);
  unchanged[2] = 3.0f;
  memcpy(original, unchanged, sizeof(original));
  assert(capy_vorbis_floor1_apply_gain(indices, 4, 10.0f, unchanged, 4,
                                       unchanged, 4) ==
         CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  same(unchanged, original, 4);
  assert(capy_vorbis_floor1_apply_gain(indices, 4, 10.0f, unchanged, 3,
                                       scratch, 4) ==
         CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_vorbis_floor1_apply_gain(indices, 0, 10.0f, unchanged, 4,
                                       scratch, 4) ==
         CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_floor1_apply_gain(indices, 4, NAN, unchanged, 4,
                                       scratch, 4) ==
         CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  static uint8_t maximum_indices[CAPY_VORBIS_FLOOR1_BINS];
  static float maximum[CAPY_VORBIS_FLOOR1_BINS + 1];
  static float maximum_scratch[CAPY_VORBIS_FLOOR1_BINS];
  for (size_t i = 0; i < CAPY_VORBIS_FLOOR1_BINS; ++i) {
    maximum_indices[i] = (uint8_t)(i & 255u);
    maximum[i] = 1.0f;
  }
  maximum[CAPY_VORBIS_FLOOR1_BINS] = 123.0f;
  assert(capy_vorbis_floor1_apply_gain(
             maximum_indices, CAPY_VORBIS_FLOOR1_BINS, 1.0f, maximum,
             CAPY_VORBIS_FLOOR1_BINS, maximum_scratch,
             CAPY_VORBIS_FLOOR1_BINS) == 0);
  for (size_t i = 1; i < 256; ++i)
    assert(maximum[i] > maximum[i - 1]);
  for (size_t i = 0; i < CAPY_VORBIS_FLOOR1_BINS; ++i)
    assert(maximum[i] > 0.0f && maximum[i] <= 1.0f);
  assert(maximum[CAPY_VORBIS_FLOOR1_BINS] == 123.0f);

  puts("[vorbis-floor1-gain] exact table points, atomic failure and maximum bounds passed");
  return 0;
}

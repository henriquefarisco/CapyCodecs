#include "vorbis_coupling.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void expect_same(const float *left, const float *right, size_t count) {
  for (size_t i = 0; i < count; ++i)
    assert(memcmp(&left[i], &right[i], sizeof(left[i])) == 0);
}

int main(void) {
  const uint8_t magnitude[] = {0};
  const uint8_t angle[] = {1};
  float vectors[12] = {
      2.0f, 2.0f, -2.0f, -2.0f, 0.0f, 91.0f,
      1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 92.0f,
  };
  float scratch[10];
  assert(capy_vorbis_inverse_coupling(vectors, 12, 2, 6, 5, magnitude,
                                      angle, 1, 100.0f, scratch, 10) == 0);
  const float expected[12] = {
      2.0f, 1.0f, -2.0f, -1.0f, 0.0f, 91.0f,
      1.0f, 2.0f, -1.0f, -2.0f, 0.0f, 92.0f,
  };
  expect_same(vectors, expected, 12);

  /* Pair order is normative and reversed during inverse coupling. */
  float ordered[] = {4.0f, 1.0f, -2.0f};
  float ordered_scratch[3];
  const uint8_t ordered_magnitude[] = {0, 1};
  const uint8_t ordered_angle[] = {1, 2};
  assert(capy_vorbis_inverse_coupling(
             ordered, 3, 3, 1, 1, ordered_magnitude, ordered_angle, 2,
             10.0f, ordered_scratch, 3) == 0);
  assert(ordered[0] == 3.0f && ordered[1] == 4.0f && ordered[2] == 1.0f);

  float unchanged[] = {1.0f, 2.0f, 3.0f, 4.0f};
  float original[4];
  float small_scratch[4];
  memcpy(original, unchanged, sizeof(original));
  const uint8_t bad_angle[] = {0};
  assert(capy_vorbis_inverse_coupling(
             unchanged, 4, 2, 2, 2, magnitude, bad_angle, 1, 10.0f,
             small_scratch, 4) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  expect_same(unchanged, original, 4);
  assert(capy_vorbis_inverse_coupling(
             unchanged, 4, 2, 2, 2, magnitude, angle, 1, 10.0f,
             small_scratch, 3) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  expect_same(unchanged, original, 4);
  unchanged[3] = NAN;
  memcpy(original, unchanged, sizeof(original));
  assert(capy_vorbis_inverse_coupling(
             unchanged, 4, 2, 2, 2, magnitude, angle, 1, 10.0f,
             small_scratch, 4) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  expect_same(unchanged, original, 4);
  unchanged[3] = 4.0f;
  assert(capy_vorbis_inverse_coupling(
             unchanged, 4, 2, 2, 2, magnitude, angle, 1, 10.0f,
             unchanged, 4) == CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_inverse_coupling(
             unchanged, 4, 2, 2, 2, magnitude, angle, 257, 10.0f,
             small_scratch, 4) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_vorbis_inverse_coupling(
             unchanged, 4, 2, 4097, 4097, magnitude, angle, 1, 10.0f,
             small_scratch, 4) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);

  /* No-coupling mappings are a bounded no-op and need no descriptor/scratch. */
  memcpy(original, unchanged, sizeof(original));
  assert(capy_vorbis_inverse_coupling(unchanged, 4, 2, 2, 2, NULL, NULL,
                                      0, 10.0f, NULL, 0) == 0);
  expect_same(unchanged, original, 4);

  /* Deterministic sign/reference sweep plus unchanged padding sentinels. */
  uint32_t seed = 0x5f3759dfu;
  for (unsigned trial = 0; trial < 10000; ++trial) {
    float input[9];
    float work[6];
    float reference[9];
    for (size_t i = 0; i < 9; ++i) {
      seed = seed * 1664525u + 1013904223u;
      input[i] = (float)((int32_t)(seed % 2001u) - 1000) / 100.0f;
    }
    input[2] = 111.0f;
    input[5] = 112.0f;
    input[8] = 113.0f;
    memcpy(reference, input, sizeof(reference));
    float m = reference[0];
    float a = reference[3];
    if (m > 0.0f) {
      if (a > 0.0f)
        reference[3] = m - a;
      else {
        reference[0] = m + a;
        reference[3] = m;
      }
    } else if (a > 0.0f)
      reference[3] = m + a;
    else {
      reference[0] = m - a;
      reference[3] = m;
    }
    m = reference[1];
    a = reference[4];
    if (m > 0.0f) {
      if (a > 0.0f)
        reference[4] = m - a;
      else {
        reference[1] = m + a;
        reference[4] = m;
      }
    } else if (a > 0.0f)
      reference[4] = m + a;
    else {
      reference[1] = m - a;
      reference[4] = m;
    }
    assert(capy_vorbis_inverse_coupling(input, 9, 3, 3, 2, magnitude,
                                        angle, 1, 20.0f, work, 6) == 0);
    expect_same(input, reference, 9);
  }

  static float maximum[CAPY_VORBIS_COUPLING_CHANNELS *
                       CAPY_VORBIS_SPECTRAL_BINS];
  static float maximum_scratch[CAPY_VORBIS_COUPLING_CHANNELS *
                               CAPY_VORBIS_SPECTRAL_BINS];
  const uint8_t maximum_magnitude[] = {0};
  const uint8_t maximum_angle[] = {7};
  maximum[0] = 1.0f;
  maximum[7u * CAPY_VORBIS_SPECTRAL_BINS] = -0.25f;
  assert(capy_vorbis_inverse_coupling(
             maximum, sizeof(maximum) / sizeof(maximum[0]),
             CAPY_VORBIS_COUPLING_CHANNELS, CAPY_VORBIS_SPECTRAL_BINS,
             CAPY_VORBIS_SPECTRAL_BINS, maximum_magnitude, maximum_angle, 1,
             2.0f, maximum_scratch,
             sizeof(maximum_scratch) / sizeof(maximum_scratch[0])) == 0);
  assert(maximum[0] == 0.75f &&
         maximum[7u * CAPY_VORBIS_SPECTRAL_BINS] == 1.0f);

  puts("[vorbis-coupling] reverse order, sign quadrants, bounds and atomic failures passed");
  return 0;
}

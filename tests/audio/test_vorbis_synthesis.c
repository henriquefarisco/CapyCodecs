#include "vorbis_synthesis.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  enum { CHANNELS = 2, N = 64, BINS = 32 };
  struct capy_vorbis_setup_summary summary = {0};
  struct capy_vorbis_setup_workspace setup = {0};
  struct capy_vorbis_packet_window window = {0};
  struct capy_vorbis_mdct_plan mdct = {0};
  struct capy_vorbis_floor1_values floors[CHANNELS] = {0};
  struct capy_vorbis_synthesis_state state = {0};
  struct capy_vorbis_synthesis_result result = {0};
  float trig[N + N / 4], spectrum[CHANNELS * BINS] = {0};
  int32_t bitrev[N / 4];
  float previous[CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES] = {0};
  float pcm[CHANNELS * N] = {0}, pcm_scratch[CHANNELS * N] = {0};
  float work[CHANNELS * N + 3 * N] = {0};
  uint8_t curve[BINS];
  uint16_t x[2] = {0, BINS};

  summary.floors = summary.mappings = 1;
  setup.floor_type[0] = 1;
  setup.mappings[0].submaps = 1;
  setup.mappings[0].floor[0] = 0;
  assert(capy_vorbis_floor1_prepare(x, 2, 1, &setup.floor1[0].plan) == 0);
  assert(capy_vorbis_mdct_plan_init(N, trig, N + N / 4, bitrev, N / 4,
                                    &mdct) == 0);
  window.block_size = N;
  window.left_end = N / 2;
  window.right_start = N / 2;
  window.right_end = N;

  /* An absent-floor packet is silent but still primes overlap state. */
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
      floors, CHANNELS, 1024.0f, spectrum, CHANNELS * BINS,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * N, pcm_scratch, CHANNELS * N,
      work, CHANNELS * N + 3 * N, curve, BINS, &state, &result) == 0);
  assert(state.primed && state.channels == CHANNELS &&
         state.previous_size == N && result.frames == 0);

  floors[0].present = floors[1].present = 1;
  floors[0].y[0] = floors[0].y[1] = 255;
  floors[1].y[0] = floors[1].y[1] = 255;
  spectrum[0] = 1.0f;
  spectrum[BINS] = -0.5f;
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
      floors, CHANNELS, 1024.0f, spectrum, CHANNELS * BINS,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * N, pcm_scratch, CHANNELS * N,
      work, CHANNELS * N + 3 * N, curve, BINS, &state, &result) == 0);
  assert(result.frames == N / 2);
  int nonzero = 0;
  for (size_t i = 0; i < result.frames * CHANNELS; ++i)
    if (pcm[i] != 0.0f) nonzero = 1;
  assert(nonzero);

  /* Packet exhaustion zeros every channel, regardless of other floor state. */
  floors[0].exhausted = 1;
  floors[0].present = 0;
  memset(spectrum, 0, sizeof(spectrum));
  spectrum[BINS] = 10.0f;
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
      floors, CHANNELS, 1024.0f, spectrum, CHANNELS * BINS,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * N, pcm_scratch, CHANNELS * N,
      work, CHANNELS * N + 3 * N, curve, BINS, &state, &result) == 0);
  for (size_t i = 0; i < BINS; ++i) assert(spectrum[BINS + i] == 0.0f);

  /* Unsupported floor0 fails before mutating persistent decoder state. */
  struct capy_vorbis_synthesis_state saved_state = state;
  float saved_previous[CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES];
  memcpy(saved_previous, previous, sizeof(previous));
  setup.floor_type[0] = 0;
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
      floors, CHANNELS, 1024.0f, spectrum, CHANNELS * BINS,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * N, pcm_scratch, CHANNELS * N,
      work, CHANNELS * N + 3 * N, curve, BINS, &state, &result) ==
      CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT);
  assert(!memcmp(&state, &saved_state, sizeof(state)));
  assert(!memcmp(previous, saved_previous, sizeof(previous)));

  setup.floor_type[0] = 1;
  state.channels = 1;
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
      floors, CHANNELS, 1024.0f, spectrum, CHANNELS * BINS,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * N, pcm_scratch, CHANNELS * N,
      work, CHANNELS * N + 3 * N, curve, BINS, &state, &result) ==
      CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  puts("[vorbis-synthesis] floor/coupling/imdct/window/overlap lifecycle passed");
  return 0;
}

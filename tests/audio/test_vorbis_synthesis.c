#include "vorbis_synthesis.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_transition(unsigned channels, size_t before, size_t after) {
  struct capy_vorbis_setup_summary summary = {0};
  struct capy_vorbis_setup_workspace setup = {0};
  struct capy_vorbis_floor1_values floors[CAPY_AUDIO_MAX_CHANNELS] = {0};
  struct capy_vorbis_synthesis_state state = {0}, saved_state;
  struct capy_vorbis_synthesis_result result;
  size_t largest = before > after ? before : after;
  size_t frames = before / 4u + after / 4u, samples = frames * channels;
  size_t previous_count = channels * CAPY_VORBIS_BLOCK_SAMPLES;
  float *previous = calloc(previous_count, sizeof(*previous));
  float *saved_previous = malloc(previous_count * sizeof(*saved_previous));
  float *spectrum = calloc(channels * largest / 2u, sizeof(*spectrum));
  float *pcm = malloc((samples + 1u) * sizeof(*pcm));
  float *pcm_scratch = malloc((samples + 1u) * sizeof(*pcm_scratch));
  float *trig = malloc((largest + largest / 4u) * sizeof(*trig));
  int32_t *bitrev = malloc(largest / 4u * sizeof(*bitrev));
  uint8_t *curve = malloc(largest / 2u);
  assert(previous && saved_previous && spectrum && pcm && pcm_scratch &&
         trig && bitrev && curve);
  summary.floors = summary.mappings = 1;
  setup.floor_type[0] = 1;
  setup.mappings[0].submaps = 1;
  uint16_t x[2] = {0, (uint16_t)(largest / 2u)};
  assert(capy_vorbis_floor1_prepare(x, 2, 1, &setup.floor1[0].plan) == 0);
  for (unsigned c = 0; c < channels; ++c) {
    floors[c].present = 1;
    floors[c].y[0] = floors[c].y[1] = 255;
  }

  for (unsigned packet = 0; packet < 2; ++packet) {
    size_t n = packet ? after : before, bins = n / 2u;
    size_t aux = packet && frames > n ? frames : n;
    size_t work_count = (channels + 1u) * n + 2u * aux;
    float *work = malloc((work_count + 1u) * sizeof(*work));
    assert(work);
    work[work_count] = 123.0f;
    struct capy_vorbis_mdct_plan mdct;
    assert(capy_vorbis_mdct_plan_init(n, trig, largest + largest / 4u,
        bitrev, largest / 4u, &mdct) == 0);
    struct capy_vorbis_packet_window window = {0};
    window.block_size = (uint16_t)n;
    window.left_end = (uint16_t)(n / 2u);
    window.right_start = (uint16_t)(n / 2u);
    window.right_end = (uint16_t)n;
    if (!packet && after < before) {
      window.right_start = (uint16_t)(3u * n / 4u - after / 4u);
      window.right_end = (uint16_t)(3u * n / 4u + after / 4u);
    } else if (packet && before < after) {
      window.left_start = (uint16_t)(n / 4u - before / 4u);
      window.left_end = (uint16_t)(n / 4u + before / 4u);
    }
    memset(spectrum, 0, channels * bins * sizeof(*spectrum));
    for (unsigned c = 0; c < channels; ++c) {
      spectrum[c * bins] = (float)(c + 1u) * 0.03125f;
      spectrum[c * bins + 1u] = -(float)(c + 1u) * 0.0078125f;
    }
    for (size_t i = 0; i <= samples; ++i) {
      pcm[i] = 123.0f;
      pcm_scratch[i] = 123.0f;
    }

    if (packet) {
      /* Each minimum is enforced before touching persistent output/state. */
      for (unsigned shortage = 0; shortage < 3; ++shortage) {
        result.frames = 123;
        result.primed = 1;
        assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
            floors, channels, 1024.0f, spectrum, channels * bins,
            previous, previous_count, pcm, samples - (shortage == 0),
            pcm_scratch, samples - (shortage == 1),
            work, work_count - (shortage == 2), curve, bins,
            &state, &result) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
        assert(!result.frames && !result.primed);
        assert(!memcmp(&state, &saved_state, sizeof(state)));
        assert(!memcmp(previous, saved_previous,
                       previous_count * sizeof(*previous)));
        for (size_t i = 0; i <= samples; ++i) assert(pcm[i] == 123.0f);
      }
    }
    assert(capy_vorbis_synthesis_finish(&summary, &setup, &window, &mdct,
        floors, channels, 1024.0f, spectrum, channels * bins,
        previous, previous_count, pcm, samples, pcm_scratch, samples,
        work, work_count, curve, bins, &state, &result) == 0);
    assert(state.previous_size == n && state.channels == channels &&
           state.primed && result.primed);
    assert(result.frames == (packet ? frames : 0u));
    assert(work[work_count] == 123.0f && pcm[samples] == 123.0f &&
           pcm_scratch[samples] == 123.0f);
    if (!packet) {
      saved_state = state;
      memcpy(saved_previous, previous, previous_count * sizeof(*previous));
    } else {
      /* Independent overlap indexing oracle, with distinct non-silent
       * channels, also verifies dense-to-interleaved channel placement. */
      for (unsigned c = 0; c < channels; ++c) {
        int nonzero = 0;
        for (size_t i = 0; i < frames; ++i) {
          size_t old_index = before / 2u + i;
          int64_t new_index = (int64_t)(after / 4u) -
                              (int64_t)(before / 4u) + (int64_t)i;
          double expected = 0.0;
          if (old_index < before)
            expected += saved_previous[c * before + old_index];
          if (new_index >= 0 && (size_t)new_index < after / 2u)
            expected += previous[c * after + (size_t)new_index];
          assert(pcm[i * channels + c] == (float)expected);
          if (pcm[i * channels + c] != 0.0f) nonzero = 1;
        }
        assert(nonzero);
      }
    }
    free(work);
  }
  free(curve);
  free(bitrev);
  free(trig);
  free(pcm_scratch);
  free(pcm);
  free(spectrum);
  free(saved_previous);
  free(previous);
}

static void test_block_transitions(void) {
  const unsigned channels[] = {1, 2, CAPY_AUDIO_MAX_CHANNELS};
  for (size_t c = 0; c < sizeof(channels) / sizeof(channels[0]); ++c)
    for (size_t before = 64; before <= CAPY_VORBIS_BLOCK_SAMPLES; before *= 2u)
      for (size_t after = 64; after <= CAPY_VORBIS_BLOCK_SAMPLES; after *= 2u)
        test_transition(channels[c], before, after);
  puts("[vorbis-synthesis] 192 block/channel transitions and capacity limits passed");
}

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

  test_block_transitions();
  puts("[vorbis-synthesis] floor/coupling/imdct/window/overlap lifecycle passed");
  return 0;
}

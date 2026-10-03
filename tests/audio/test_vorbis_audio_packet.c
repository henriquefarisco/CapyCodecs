#include "vorbis_audio_packet.h"
#include "vorbis_synthesis.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t packet[16];
static size_t position;
static void put(uint32_t value, unsigned bits) {
  for (unsigned i = 0; i < bits; ++i, ++position)
    packet[position / 8] |= (uint8_t)(((value >> i) & 1u) << (position % 8));
}

static void residue_config(struct capy_vorbis_residue_config *value) {
  memset(value, 0, sizeof(*value));
  for (unsigned c = 0; c < CAPY_VORBIS_RESIDUE_CLASSES; ++c)
    for (unsigned pass = 0; pass < CAPY_VORBIS_RESIDUE_PASSES; ++pass)
      value->books[c][pass] = -1;
  value->type = 2;
  value->end = 4;
  value->partition_size = 4;
  value->classifications = 1;
  value->classbook = 0;
  value->cascade[0] = 1;
  value->books[0][0] = 1;
}

int main(void) {
  enum { CHANNELS = 2, BINS = 32, TOTAL = CHANNELS * BINS };
  const uint8_t lengths[] = {1, 1}, setup_packet[] = {0x21, 0x43};
  struct capy_vorbis_huffman_node nodes[2][3];
  struct capy_vorbis_huffman trees[2];
  struct capy_vorbis_setup_summary summary = {0};
  struct capy_vorbis_setup_workspace setup = {0};
  struct capy_vorbis_floor1_values floors[CHANNELS], floor_scratch[CHANNELS];
  struct capy_vorbis_audio_packet_result result;
  float spectrum[TOTAL], work[5 * TOTAL];
  uint8_t classes[TOTAL];
  uint16_t x[2] = {0, BINS};

  assert(capy_vorbis_huffman_build(lengths, 2, nodes[0], 3, &trees[0]) == 0);
  assert(capy_vorbis_huffman_build(lengths, 2, nodes[1], 3, &trees[1]) == 0);
  setup.books[0].entries = 2; setup.books[0].dimensions = 2;
  setup.books[1].entries = 2; setup.books[1].dimensions = 2;
  setup.books[1].lookup_type = 2; setup.books[1].lookup_values = 4;
  setup.books[1].value_bits = 4; setup.books[1].delta_raw = 0x60100000u;
  summary.books = 2; summary.floors = 1; summary.residues = 2;
  summary.mappings = 1; summary.modes = 1;
  setup.floor_type[0] = 1;
  setup.floor1[0].book_count = 2;
  assert(capy_vorbis_floor1_prepare(x, 2, 1, &setup.floor1[0].plan) == 0);
  residue_config(&setup.residues[0]); residue_config(&setup.residues[1]);
  setup.mappings[0].submaps = 2;
  setup.mappings[0].mux[0] = 0; setup.mappings[0].mux[1] = 1;
  setup.mappings[0].floor[0] = setup.mappings[0].floor[1] = 0;
  setup.mappings[0].residue[0] = 0; setup.mappings[0].residue[1] = 1;

  memset(packet, 0, sizeof(packet)); position = 0;
  put(0, 1); /* audio packet; one mode therefore zero selector bits */
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    put(1, 1); put(255, 8); put(255, 8); /* present flat floor */
  }
  put(0, 3); put(0, 3); /* one class + two vectors in each submap */
  assert(capy_vorbis_audio_packet_decode(packet, (position + 7) / 8,
      sizeof(packet), setup_packet, sizeof(setup_packet), &summary, &setup,
      trees, 2, CHANNELS, 64, 64, 16, 100.0f,
      spectrum, TOTAL, floors, floor_scratch, work, 5 * TOTAL,
      classes, TOTAL, &result) == 0);
  assert(result.window.block_size == 64 && result.vectors == 4 &&
         !result.exhausted && floors[0].present && floors[1].present);
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    const float expected[] = {1, 2, 1, 2};
    for (unsigned i = 0; i < 4; ++i)
      assert(spectrum[channel * BINS + i] == expected[i]);
    for (unsigned i = 4; i < BINS; ++i) assert(spectrum[channel * BINS + i] == 0);
  }

  /* Drive the decoded packet through post-entropy synthesis twice: the first
   * primes overlap, the second publishes interleaved nonzero float PCM. */
  float trig[64 + 16]; int32_t bitrev[16];
  struct capy_vorbis_mdct_plan mdct;
  struct capy_vorbis_synthesis_state synth_state = {0};
  struct capy_vorbis_synthesis_result synth_result;
  static float previous[CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES];
  float pcm[CHANNELS * 64], pcm_scratch[CHANNELS * 64];
  float synth_work[CHANNELS * 64 + 3 * 64]; uint8_t curve[BINS];
  assert(capy_vorbis_mdct_plan_init(64, trig, 80, bitrev, 16, &mdct) == 0);
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &result.window, &mdct,
      floors, CHANNELS, 100.0f, spectrum, TOTAL,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * 64, pcm_scratch, CHANNELS * 64,
      synth_work, CHANNELS * 64 + 3 * 64, curve, BINS,
      &synth_state, &synth_result) == 0 && synth_result.frames == 0);
  assert(capy_vorbis_audio_packet_decode(packet, (position + 7) / 8,
      sizeof(packet), setup_packet, sizeof(setup_packet), &summary, &setup,
      trees, 2, CHANNELS, 64, 64, 16, 100.0f,
      spectrum, TOTAL, floors, floor_scratch, work, 5 * TOTAL,
      classes, TOTAL, &result) == 0);
  assert(capy_vorbis_synthesis_finish(&summary, &setup, &result.window, &mdct,
      floors, CHANNELS, 100.0f, spectrum, TOTAL,
      previous, CHANNELS * CAPY_VORBIS_BLOCK_SAMPLES,
      pcm, CHANNELS * 64, pcm_scratch, CHANNELS * 64,
      synth_work, CHANNELS * 64 + 3 * 64, curve, BINS,
      &synth_state, &synth_result) == 0 && synth_result.frames == 32);
  int pcm_nonzero = 0;
  for (size_t i = 0; i < synth_result.frames * CHANNELS; ++i)
    if (pcm[i] != 0.0f) pcm_nonzero = 1;
  assert(pcm_nonzero);

  /* Truncating within the first floor is nominal exhaustion and all-silent. */
  memset(spectrum, 0xa5, sizeof(spectrum));
  assert(capy_vorbis_audio_packet_decode(packet, 1, sizeof(packet),
      setup_packet, sizeof(setup_packet), &summary, &setup, trees, 2,
      CHANNELS, 64, 64, 16, 100.0f, spectrum, TOTAL, floors, floor_scratch,
      work, 5 * TOTAL, classes, TOTAL, &result) == 0);
  assert(result.exhausted);
  for (unsigned i = 0; i < TOTAL; ++i) assert(spectrum[i] == 0.0f);
  for (unsigned i = 0; i < CHANNELS; ++i) assert(floors[i].exhausted);

  /* A late residue descriptor failure cannot publish decoded floors/spectrum. */
  memset(spectrum, 0x5a, sizeof(spectrum));
  memset(floors, 0x3c, sizeof(floors));
  float saved_spectrum[TOTAL];
  struct capy_vorbis_floor1_values saved_floors[CHANNELS];
  memcpy(saved_spectrum, spectrum, sizeof(spectrum));
  memcpy(saved_floors, floors, sizeof(floors));
  setup.residues[0].cascade[0] = 0;
  assert(capy_vorbis_audio_packet_decode(packet, (position + 7) / 8,
      sizeof(packet), setup_packet, sizeof(setup_packet), &summary, &setup,
      trees, 2, CHANNELS, 64, 64, 16, 100.0f,
      spectrum, TOTAL, floors, floor_scratch, work, 5 * TOTAL,
      classes, TOTAL, &result) == CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(!memcmp(spectrum, saved_spectrum, sizeof(spectrum)));
  assert(!memcmp(floors, saved_floors, sizeof(floors)));
  setup.residues[0].cascade[0] = 1;

  assert(capy_vorbis_audio_packet_decode(packet, (position + 7) / 8,
      sizeof(packet), setup_packet, sizeof(setup_packet), &summary, &setup,
      trees, 2, CHANNELS, 64, 64, 16, 100.0f,
      spectrum, TOTAL, floors, floor_scratch, work, 5 * TOTAL,
      (uint8_t *)work, TOTAL, &result) == CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  puts("[vorbis-audio-packet] per-submap residue and packet-to-float-PCM passed");
  return 0;
}

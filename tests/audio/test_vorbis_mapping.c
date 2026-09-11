#include "vorbis_mapping.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t packet[2048];
static size_t position;

static void put(uint32_t value, unsigned width) {
  assert(position + width <= sizeof(packet) * 8u);
  for (unsigned bit = 0; bit < width; ++bit, ++position)
    packet[position / 8u] |=
        (uint8_t)(((value >> bit) & 1u) << (position % 8u));
}

static void fixture(void) {
  memset(packet, 0, sizeof(packet));
  position = 0;
  put(0, 16);             /* mapping type 0 */
  put(1, 1); put(1, 4);  /* two submaps */
  put(1, 1); put(0, 8);  /* one coupling step */
  put(0, 1); put(1, 1);  /* magnitude 0, angle 1 */
  put(0, 2);             /* reserved */
  put(0, 4); put(1, 4);  /* per-channel mux */
  put(77, 8); put(2, 8); put(1, 8);
  put(88, 8); put(1, 8); put(3, 8);
}

static int parse(size_t bit_count, struct capy_vorbis_mapping *mapping) {
  struct capy_vorbis_bits bits;
  size_t bytes = (position + 7u) / 8u;
  assert(capy_vorbis_bits_init(&bits, packet, bytes, bytes) == 0);
  bits.bit_count = bit_count;
  return capy_vorbis_mapping_read(&bits, 2, 3, 4, mapping);
}

static int zeroed(const struct capy_vorbis_mapping *mapping) {
  const struct capy_vorbis_mapping zero = {0};
  return memcmp(mapping, &zero, sizeof(zero)) == 0;
}

int main(void) {
  struct capy_vorbis_mapping mapping;
  fixture();
  assert(parse(position, &mapping) == 0);
  assert(mapping.submaps == 2 && mapping.coupling_steps == 1);
  assert(mapping.magnitude[0] == 0 && mapping.angle[0] == 1);
  assert(mapping.mux[0] == 0 && mapping.mux[1] == 1);
  assert(mapping.floor[0] == 2 && mapping.residue[0] == 1);
  assert(mapping.floor[1] == 1 && mapping.residue[1] == 3);

  for (size_t cut = 0; cut < position; ++cut) {
    memset(&mapping, 0xa5, sizeof(mapping));
    assert(parse(cut, &mapping) < 0);
    assert(zeroed(&mapping));
  }

  /* Default mapping: no coupling, one submap and all channels mux to zero. */
  memset(packet, 0, sizeof(packet)); position = 0;
  put(0, 16); put(0, 1); put(0, 1); put(0, 2);
  put(255, 8); put(0, 8); put(0, 8);
  assert(parse(position, &mapping) == 0);
  assert(mapping.submaps == 1 && mapping.coupling_steps == 0);
  for (size_t i = 0; i < CAPY_AUDIO_MAX_CHANNELS; ++i)
    assert(mapping.mux[i] == 0);

  /* Maximum encoded coupling count remains representable as 256, not zero. */
  memset(packet, 0, sizeof(packet)); position = 0;
  put(0, 16); put(0, 1); put(1, 1); put(255, 8);
  for (unsigned i = 0; i < 256; ++i) {
    put(0, 1); put(1, 1);
  }
  put(0, 2); put(0, 8); put(0, 8); put(0, 8);
  assert(parse(position, &mapping) == 0);
  assert(mapping.coupling_steps == 256 && mapping.angle[255] == 1);

  fixture();
  const size_t mapping_type = 0;
  packet[mapping_type] ^= 1u;
  assert(parse(position, &mapping) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  assert(zeroed(&mapping));
  packet[mapping_type] ^= 1u;

  /* The time placeholder is specified as unused and must not affect output. */
  packet[42 / 8u] ^= (uint8_t)(1u << (42 % 8u));
  assert(parse(position, &mapping) == 0 && mapping.floor[0] == 2);
  packet[42 / 8u] ^= (uint8_t)(1u << (42 % 8u));

  /* Pair equality, reserved bits, mux and floor/residue references fail. */
  const size_t invalid_bits[] = {30, 32, 35, 50, 60, 76, 84};
  for (size_t i = 0; i < sizeof(invalid_bits) / sizeof(invalid_bits[0]); ++i) {
    size_t bit = invalid_bits[i];
    packet[bit / 8u] ^= (uint8_t)(1u << (bit % 8u));
    memset(&mapping, 0xa5, sizeof(mapping));
    assert(parse(position, &mapping) < 0);
    assert(zeroed(&mapping));
    packet[bit / 8u] ^= (uint8_t)(1u << (bit % 8u));
  }

  struct capy_vorbis_bits bits;
  size_t bytes = (position + 7u) / 8u;
  assert(capy_vorbis_bits_init(&bits, packet, bytes, bytes) == 0);
  assert(capy_vorbis_mapping_read(&bits, 0, 3, 4, &mapping) ==
         CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(zeroed(&mapping));
  assert(capy_vorbis_mapping_read(&bits, 2, 3, 4, NULL) ==
         CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  puts("[vorbis-mapping] retained submaps/coupling, truncation and references passed");
  return 0;
}

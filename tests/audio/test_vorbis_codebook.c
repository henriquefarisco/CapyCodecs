#include "vorbis_codebook.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t data[1024], lengths[256];
static size_t position;
static void put(uint32_t value, unsigned bits) {
  assert(position + bits <= sizeof(data) * 8u);
  for (unsigned i = 0; i < bits; ++i, ++position)
    data[position / 8u] |= (uint8_t)(((value >> i) & 1u) << (position % 8u));
}
static void start(unsigned entries, unsigned dimensions) {
  memset(data, 0, sizeof(data)); position = 0;
  put(0x564342u, 24); put(dimensions, 16); put(entries, 24);
}
static void lookup(unsigned type, unsigned count) {
  put(type, 4);
  if (type && type <= 2u) {
    put(0, 32); put(0x60100000, 32); put(3, 4); put(1, 1);
    for (unsigned i = 0; i < count; ++i) put(i & 15u, 4);
  }
}
static int parse(struct capy_vorbis_book *out, size_t bit_count,
                  struct capy_vorbis_book_limits limits) {
  struct capy_vorbis_bits bits;
  assert(capy_vorbis_bits_init(&bits, data, (bit_count + 7u) / 8u, sizeof(data)) == 0);
  bits.bit_count = bit_count; /* Exercise truncation at every bit boundary. */
  int rc = capy_vorbis_book_parse(&bits, &limits, lengths, sizeof(lengths), out);
  if (rc) {
    assert(out->entries == 0 && out->lookup_values == 0);
    assert(capy_vorbis_book_parse(&bits, &limits, lengths, sizeof(lengths), out) == rc);
  } else assert(bits.position <= bit_count);
  return rc;
}
int main(void) {
  struct capy_vorbis_book out;
  struct capy_vorbis_book_limits limits = {256, 32, 8192};
  struct capy_vorbis_bits bits;
  data[0] = 0x81; data[1] = 0x7f;
  assert(capy_vorbis_bits_init(&bits, data, 2, 2) == 0);
  assert(capy_vorbis_bits_read(&bits, 0) == 0 && bits.position == 0);
  assert(capy_vorbis_bits_read(&bits, 3) == 1);
  assert(capy_vorbis_bits_read(&bits, 9) == 496);
  assert(capy_vorbis_bits_read(&bits, 4) == 7);
  assert(capy_vorbis_bits_read(&bits, 1) == 0 && bits.error == CAPY_AUDIO_ERR_TRUNCATED_DATA);
  assert(capy_vorbis_bits_init(&bits, data, SIZE_MAX, SIZE_MAX) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_vorbis_bits_init(&bits, data, 2, 2) == 0);
  assert(capy_vorbis_bits_read(&bits, 33) == 0 && bits.error == CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  /* Unordered complete two-entry tree, all three lookup forms. */
  for (unsigned type = 0; type <= 2; ++type) {
    start(2, 2); put(0, 1); put(0, 1); put(0, 5); put(0, 5);
    lookup(type, type == 1 ? 1 : 4);
    assert(parse(&out, position, limits) == 0);
    assert(out.entries == 2 && out.used_entries == 2 && lengths[0] == 1 && lengths[1] == 1);
    assert(out.lookup_values == (type == 1 ? 1u : type == 2 ? 4u : 0u));
    for (size_t n = 0; n < position; ++n) assert(parse(&out, n, limits) < 0);
  }
  limits.max_lookup_values = 3;
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  limits.max_lookup_values = 8192;
  limits.max_entries = 1;
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  limits.max_entries = 256;
  limits.max_dimensions = 1;
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  limits.max_dimensions = 32;
  /* Ordered four entries, an empty length-one run, all length two. */
  start(4, 2); put(1, 1); put(0, 5); put(0, 3); put(4, 3); lookup(1, 2);
  assert(parse(&out, position, limits) == 0 && out.lookup_values == 2);
  for (unsigned i = 0; i < 4; ++i) assert(lengths[i] == 2);
  /* An ordered run cannot exceed remaining entries. */
  start(2, 1); put(1, 1); put(0, 5); put(3, 2); lookup(0, 0);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  /* Zero runs cannot force an infinite loop or length >32. */
  start(2, 1); put(1, 1); put(0, 5);
  for (unsigned i = 0; i < 32; ++i) put(0, 2);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  /* Sparse one-active-entry exception, then invalid length two. */
  start(3, 1); put(0, 1); put(1, 1); put(0, 1); put(1, 1); put(0, 5); put(0, 1); lookup(0, 0);
  assert(parse(&out, position, limits) == 0 && out.used_entries == 1);
  assert(lengths[0] == 0 && lengths[1] == 1 && lengths[2] == 0);
  start(1, 1); put(0, 1); put(0, 1); put(1, 5); lookup(0, 0);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  /* Under/oversubscribed and all-unused trees. */
  start(2, 1); put(0, 1); put(0, 1); put(1, 5); put(1, 5); lookup(0, 0);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  start(3, 1); put(0, 1); put(0, 1);
  for (unsigned i = 0; i < 3; ++i) put(0, 5);
  lookup(0, 0);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  start(1, 1); put(0, 1); put(1, 1); put(0, 1); lookup(0, 0);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  start(1, 1); put(0, 1); put(0, 1); put(0, 5); lookup(3, 0);
  assert(parse(&out, position, limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  /* Fixed corpus mutations with output-reset/sticky-error invariants. */
  /* Deepest legal tree exercises 32-bit code lengths without shift overflow. */
  start(33, 1); put(0, 1); put(0, 1);
  for (unsigned i = 1; i <= 31; ++i) put(i - 1u, 5);
  put(31, 5); put(31, 5); lookup(0, 0);
  assert(parse(&out, position, limits) == 0 && lengths[32] == 32);
  /* Integer lookup root just below and above a perfect square. */
  start(15, 2); put(0, 1); put(0, 1); put(2, 5);
  for (unsigned i = 0; i < 14; ++i) put(3, 5);
  lookup(1, 3);
  assert(parse(&out, position, limits) == 0 && out.lookup_values == 3);
  start(17, 2); put(0, 1); put(0, 1);
  for (unsigned i = 0; i < 15; ++i) put(3, 5);
  put(4, 5); put(4, 5); lookup(1, 4);
  assert(parse(&out, position, limits) == 0 && out.lookup_values == 4);
  start(2, 2); put(0, 1); put(0, 1); put(0, 5); put(0, 5); lookup(2, 4);
  uint32_t seed = 0x92710ab;
  for (unsigned i = 0; i < 10000; ++i) {
    seed = seed * 1664525u + 1013904223u;
    size_t at = seed % ((position + 7) / 8);
    uint8_t old = data[at]; data[at] ^= (uint8_t)(1u + (seed >> 24));
    (void)parse(&out, position, limits);
    data[at] = old;
  }
  puts("[vorbis-codebook] bits/trees/lookups/limits/mutations passed");
  return 0;
}

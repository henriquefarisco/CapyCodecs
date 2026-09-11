#include "ogg_reader.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t bytes[4096], scratch[1024];
static void put32(uint8_t *p, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8u * i));
}
static void put64(uint8_t *p, uint64_t value) {
  put32(p, (uint32_t)value); put32(p + 4, (uint32_t)(value >> 32));
}
static void checksum(uint8_t *p, size_t n) {
  uint32_t crc = 0;
  memset(p + 22, 0, 4);
  for (size_t i = 0; i < n; ++i) {
    crc ^= (uint32_t)p[i] << 24;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc << 1) ^ (0x04c11db7u & (0u - (crc >> 31)));
  }
  put32(p + 22, crc);
}
static size_t page(size_t offset, unsigned flags, uint32_t seq,
                   const uint8_t *laces, size_t count) {
  uint8_t *p = bytes + offset;
  size_t body = 0;
  memset(p, 0, 27);
  memcpy(p, "OggS", 4);
  p[5] = (uint8_t)flags;
  put32(p + 14, 123u);
  put32(p + 18, seq);
  p[26] = (uint8_t)count;
  memcpy(p + 27, laces, count);
  for (size_t i = 0; i < count; ++i) body += laces[i];
  for (size_t i = 0; i < body; ++i) p[27 + count + i] = (uint8_t)i;
  checksum(p, 27 + count + body);
  return 27 + count + body;
}
static void init(struct capy_ogg_reader *r, size_t n, size_t packet, unsigned pages) {
  assert(capy_ogg_reader_init(r, bytes, n, sizeof(bytes), packet, pages) == 0);
}
static int drain(size_t n, size_t packet, unsigned pages) {
  struct capy_ogg_reader r;
  size_t size;
  init(&r, n, packet, pages);
  int rc;
  do { rc = capy_ogg_reader_next(&r, scratch, sizeof(scratch), &size); }
  while (rc == 1);
  assert(size == 0);
  assert(capy_ogg_reader_next(&r, scratch, sizeof(scratch), &size) == rc);
  return rc;
}
int main(void) {
  struct capy_ogg_reader r;
  size_t length, first, n;
  /* Independently generated with Xiph libogg ogg_page_checksum_set. */
  static const uint8_t golden[] = {
    79,103,103,83,0,6,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,
    179,186,219,96,1,3,97,98,99
  };
  memcpy(bytes, golden, sizeof(golden));
  init(&r, sizeof(golden), 1024, 8);
  assert(capy_ogg_reader_next(&r, scratch, 1024, &length) == 1);
  assert(length == 3 && memcmp(scratch, "abc", 3) == 0);
  assert(capy_ogg_reader_next(&r, scratch, 1024, &length) == 0);
  const uint8_t multi[] = {0, 255, 0, 3};
  n = page(0, 6, 0, multi, sizeof(multi));
  put64(bytes + 6, UINT64_C(0x1020304050607080)); checksum(bytes, n);
  init(&r, n, sizeof(scratch), 8);
  assert(capy_ogg_reader_next(&r, scratch, sizeof(scratch), &length) == 1 && length == 0);
  assert(!r.packet_has_granule);
  assert(capy_ogg_reader_next(&r, scratch, sizeof(scratch), &length) == 1 && length == 255);
  assert(!r.packet_has_granule);
  for (size_t i = 0; i < length; ++i) assert(scratch[i] == (uint8_t)i);
  assert(capy_ogg_reader_next(&r, scratch, sizeof(scratch), &length) == 1 && length == 3);
  assert(r.packet_has_granule && r.packet_granule == UINT64_C(0x1020304050607080));
  assert(capy_ogg_reader_next(&r, scratch, sizeof(scratch), &length) == 0);
  for (size_t i = 0; i < n; ++i) assert(drain(i, 1024, 8) < 0);
  for (size_t i = 0; i < n; ++i) {
    bytes[i] ^= 1u;
    assert(drain(n, 1024, 8) < 0);
    bytes[i] ^= 1u;
  }
  assert(drain(n, 254, 8) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  const uint8_t full[] = {255}, tail[] = {0, 2};
  first = page(0, 2, 0, full, 1);
  n = first + page(first, 5, 1, tail, 2);
  init(&r, n, 1024, 8);
  assert(capy_ogg_reader_next(&r, scratch, 1024, &length) == 1 && length == 255);
  assert(capy_ogg_reader_next(&r, scratch, 1024, &length) == 1 && length == 2);
  assert(capy_ogg_reader_next(&r, scratch, 1024, &length) == 0);
  assert(drain(n, 1024, 1) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  bytes[first + 5] = 4; checksum(bytes + first, n - first);
  assert(drain(n, 1024, 8) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  bytes[first + 5] = 5; put32(bytes + first + 18, 2); checksum(bytes + first, n - first);
  assert(drain(n, 1024, 8) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  put32(bytes + first + 18, 1); put32(bytes + first + 14, 456); checksum(bytes + first, n - first);
  assert(drain(n, 1024, 8) == CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT);
  n = page(0, 6, 0, full, 1);
  assert(drain(n, 1024, 8) == CAPY_AUDIO_ERR_TRUNCATED_DATA);
  n = page(0, 6, 0, tail, 2);
  bytes[n] = 0;
  assert(drain(n + 1, 1024, 8) == CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT);
  bytes[5] = 0; checksum(bytes, n);
  assert(drain(n, 1024, 8) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  n = page(0, 2, 0, tail, 2); /* Missing EOS is not a clean end. */
  assert(drain(n, 1024, 8) == CAPY_AUDIO_ERR_TRUNCATED_DATA);
  /* Empty pages preserve continuation and consume the page budget. */
  first = page(0, 2, 0, full, 1);
  n = first + page(first, 1, 1, tail, 0);
  n += page(n, 5, 2, tail, 2);
  assert(drain(n, 1024, 3) == 0);
  assert(drain(n, 1024, 2) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  /* A packet spanning three pages is bounded cumulatively, not per page. */
  first = page(0, 2, 0, full, 1);
  n = first + page(first, 1, 1, full, 1);
  n += page(n, 5, 2, tail, 2);
  assert(drain(n, 510, 3) == 0);
  assert(drain(n, 509, 3) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_ogg_reader_init(&r, bytes, n, n - 1, 1024, 8) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_ogg_reader_next(&r, scratch, 1024, &length) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  init(&r, n, 1024, 8);
  assert(capy_ogg_reader_next(&r, scratch, 1023, &length) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  /* Deterministic malformed framing corpus: recompute CRC to exercise past
   * the integrity gate. Length/lacing mutations also exercise short bodies. */
  uint32_t seed = 0x1096ab31u;
  for (unsigned trial = 0; trial < 10000; ++trial) {
    memcpy(bytes, golden, sizeof(golden));
    seed = seed * 1664525u + 1013904223u;
    size_t at = seed % sizeof(golden);
    bytes[at] ^= (uint8_t)(1u + (seed >> 24));
    checksum(bytes, sizeof(golden));
    (void)drain(sizeof(golden), 1024, 8);
  }
  puts("[ogg-reader] framing/CRC/continuation/limits/truncation passed");
  return 0;
}

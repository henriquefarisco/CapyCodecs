#include "vorbis_codebook.h"

int capy_vorbis_bits_init(struct capy_vorbis_bits *b, const uint8_t *data,
                          size_t size, size_t max_bytes) {
  if (!b) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *b = (struct capy_vorbis_bits){0};
  if (!data || !max_bytes) return b->error = CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (size > max_bytes || size > SIZE_MAX / 8u)
    return b->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  b->data = data;
  b->bit_count = size * 8u;
  return 0;
}
uint32_t capy_vorbis_bits_read(struct capy_vorbis_bits *b, unsigned count) {
  if (!b || b->error) return 0;
  if (count > 32u) { b->error = CAPY_AUDIO_ERR_INVALID_ARGUMENT; return 0; }
  if (count > b->bit_count - b->position) {
    b->error = CAPY_AUDIO_ERR_TRUNCATED_DATA;
    return 0;
  }
  uint32_t result = 0;
  for (unsigned i = 0; i < count; ++i, ++b->position)
    result |= (uint32_t)((b->data[b->position / 8u] >>
                         (b->position % 8u)) & 1u) << i;
  return result;
}
static unsigned ilog(uint32_t value) {
  unsigned bits = 0;
  while (value) { ++bits; value >>= 1; }
  return bits;
}
static int power_fits(uint32_t base, uint32_t exponent, uint32_t limit) {
  uint32_t value = 1;
  for (uint32_t i = 0; i < exponent; ++i) {
    if (value > limit / base) return 0;
    value *= base;
  }
  return 1;
}
static uint32_t lookup1(uint32_t entries, uint32_t dimensions) {
  uint32_t low = 1, high = entries;
  while (low < high) {
    uint32_t mid = low + (high - low + 1u) / 2u;
    if (power_fits(mid, dimensions, entries)) low = mid;
    else high = mid - 1u;
  }
  return low;
}
int capy_vorbis_book_parse(struct capy_vorbis_bits *b,
    const struct capy_vorbis_book_limits *limits, uint8_t *lengths,
    size_t capacity, struct capy_vorbis_book *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_book){0};
  if (!b) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (b->error) return b->error;
  if (!b->data || !limits || !lengths || !limits->max_entries ||
      !limits->max_dimensions || !limits->max_lookup_values)
    return b->error = CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  struct capy_vorbis_book book = {0};
  uint32_t counts[33] = {0};
  uint32_t sync = capy_vorbis_bits_read(b, 24);
  if (b->error) return b->error;
  if (sync != 0x564342u) return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
  book.dimensions = capy_vorbis_bits_read(b, 16);
  book.entries = capy_vorbis_bits_read(b, 24);
  if (b->error) return b->error;
  if (!book.dimensions || !book.entries)
    return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (book.entries > limits->max_entries || book.entries > capacity ||
      book.entries > 65536u || book.dimensions > limits->max_dimensions ||
      book.dimensions > 256u)
    return b->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  uint32_t ordered = capy_vorbis_bits_read(b, 1);
  if (ordered) {
    unsigned length = capy_vorbis_bits_read(b, 5) + 1u;
    uint32_t entry = 0;
    while (entry < book.entries && !b->error) {
      if (length > 32u) return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
      uint32_t count = capy_vorbis_bits_read(b, ilog(book.entries - entry));
      if (b->error) break;
      if (count > book.entries - entry)
        return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
      for (uint32_t i = 0; i < count; ++i) lengths[entry++] = (uint8_t)length;
      ++length; /* Zero-count runs progress too; at most 32 runs. */
    }
  } else {
    uint32_t sparse = capy_vorbis_bits_read(b, 1);
    for (uint32_t i = 0; i < book.entries && !b->error; ++i) {
      uint32_t used = !sparse || capy_vorbis_bits_read(b, 1);
      lengths[i] = used ? (uint8_t)(capy_vorbis_bits_read(b, 5) + 1u) : 0u;
    }
  }
  if (b->error) return b->error;
  for (uint32_t i = 0; i < book.entries; ++i) {
    ++counts[lengths[i]];
    if (lengths[i]) ++book.used_entries;
  }
  uint64_t available = 1;
  for (unsigned i = 1; i <= 32; ++i) {
    available *= 2u;
    if (counts[i] > available) return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
    available -= counts[i];
  }
  /* Vorbis 2015 erratum: a single active entry must have length one. */
  if (!book.used_entries || (available && !(book.used_entries == 1u && counts[1] == 1u)))
    return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
  book.lookup_type = capy_vorbis_bits_read(b, 4);
  if (b->error) return b->error;
  if (book.lookup_type > 2u) return b->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (book.lookup_type) {
    book.minimum_raw = capy_vorbis_bits_read(b, 32);
    book.delta_raw = capy_vorbis_bits_read(b, 32);
    book.value_bits = capy_vorbis_bits_read(b, 4) + 1u;
    book.sequence = capy_vorbis_bits_read(b, 1);
    if (b->error) return b->error;
    book.lookup_values = book.lookup_type == 1u ?
        lookup1(book.entries, book.dimensions) : book.entries * book.dimensions;
    if (book.lookup_values > limits->max_lookup_values)
      return b->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    size_t remaining = b->bit_count - b->position;
    if (book.lookup_values > remaining / book.value_bits)
      return b->error = CAPY_AUDIO_ERR_TRUNCATED_DATA;
    book.lookup_bit_offset = b->position;
    b->position += (size_t)book.lookup_values * book.value_bits;
  }
  *out = book;
  return 0;
}

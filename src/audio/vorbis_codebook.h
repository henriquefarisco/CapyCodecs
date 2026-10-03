#ifndef CAPY_VORBIS_CODEBOOK_H
#define CAPY_VORBIS_CODEBOOK_H
#include "capy_audio.h"

/* Private setup primitives, not a complete setup validator or PCM decoder. */
struct capy_vorbis_bits {
  const uint8_t *data;
  size_t bit_count, position;
  int error;
};
struct capy_vorbis_book_limits {
  uint32_t max_entries, max_dimensions, max_lookup_values;
};
struct capy_vorbis_book {
  uint32_t entries, dimensions, used_entries, lookup_type, lookup_values;
  uint32_t minimum_raw, delta_raw, value_bits, sequence;
  size_t lookup_bit_offset;
};
int capy_vorbis_bits_init(struct capy_vorbis_bits *bits, const uint8_t *data,
                          size_t size, size_t max_bytes);
/* Reads 0..32 bits LSB-first. Failure sticks, result is zero. */
uint32_t capy_vorbis_bits_read(struct capy_vorbis_bits *bits, unsigned count);
/* Caller supplies one byte per entry for lengths (zero means unused).
 * Input, lengths, state and output must not overlap. On failure output is zero
 * and lengths may contain partial data, which must not be consumed. No heap.
 * Lookup values remain borrowed in bits->data at lookup_bit_offset; they are
 * size-checked/skipped here, not expanded to floating-point vectors. */
int capy_vorbis_book_parse(struct capy_vorbis_bits *bits,
    const struct capy_vorbis_book_limits *limits, uint8_t *lengths,
    size_t capacity, struct capy_vorbis_book *out);
#endif

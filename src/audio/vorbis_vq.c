#include "vorbis_vq.h"
#include <float.h>
#ifdef __FAST_MATH__
#error "Vorbis VQ requires finite range checks and gradual underflow; disable fast-math"
#endif
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "Vorbis VQ requires binary32 float");
_Static_assert(DBL_MANT_DIG >= 53 && DBL_MAX_EXP >= 1024 && DBL_MIN_EXP <= -1021,
               "Vorbis VQ requires at least binary64 double range");

int capy_vorbis_float_unpack(uint32_t raw, float *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = 0.0f;
  uint32_t mantissa = raw & 0x1fffffu;
  if (!mantissa) return 0;
  int exponent = (int)((raw >> 21) & 0x3ffu) - 788;
  double value = (double)mantissa;
  /* Binary exponentiation: at most ten iterations, all intermediate powers
   * and products remain normal/exact in binary64 over the encoded range.
   * Only the final binary32 conversion may underflow or round. */
  unsigned power=(unsigned)(exponent<0 ? -exponent : exponent);
  double factor=exponent<0 ? 0.5 : 2.0;
  while (power) {
    if (power&1u) value*=factor;
    power>>=1;
    if (power) factor*=factor;
  }
  if (raw & 0x80000000u) value = -value;
  if (!(value <= FLT_MAX && value >= -FLT_MAX))
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  *out = (float)value;
  return 0;
}
int capy_vorbis_vq_expand(const struct capy_vorbis_book *book,
    const uint8_t *packet, size_t packet_size, uint32_t symbol,
    float max_abs, float *out, size_t capacity) {
  if (!book || !packet || !out || !(max_abs > 0.0f && max_abs <= FLT_MAX))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (!book->entries || book->entries > 65536u || !book->dimensions ||
      book->dimensions > 256u || symbol >= book->entries || book->sequence > 1u)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (capacity < book->dimensions) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (book->lookup_type != 1u && book->lookup_type != 2u)
    return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  if (!book->lookup_values || book->value_bits < 1u || book->value_bits > 16u)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (book->lookup_type == 2u &&
      book->lookup_values != book->entries * book->dimensions)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (book->lookup_type == 1u && book->lookup_values > book->entries)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  struct capy_vorbis_bits bits;
  int rc = capy_vorbis_bits_init(&bits, packet, packet_size, packet_size);
  if (rc) return rc;
  if (book->lookup_bit_offset > bits.bit_count ||
      book->lookup_values > (bits.bit_count - book->lookup_bit_offset) / book->value_bits)
    return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  float minimum, delta, temporary[256], last = 0.0f;
  rc = capy_vorbis_float_unpack(book->minimum_raw, &minimum);
  if (rc) return rc;
  rc = capy_vorbis_float_unpack(book->delta_raw, &delta);
  if (rc) return rc;
  uint32_t radix_symbol = symbol;
  for (uint32_t i = 0; i < book->dimensions; ++i) {
    uint32_t index;
    if (book->lookup_type == 1u) {
      index = radix_symbol % book->lookup_values;
      radix_symbol /= book->lookup_values; /* No growing index-divisor overflow. */
    } else index = symbol * book->dimensions + i;
    bits.position = book->lookup_bit_offset + (size_t)index * book->value_bits;
    uint32_t multiplicand = capy_vorbis_bits_read(&bits, book->value_bits);
    if (bits.error) return bits.error;
    double value = (double)multiplicand * (double)delta + (double)minimum + (double)last;
    if (!(value <= (double)max_abs && value >= -(double)max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    temporary[i] = (float)value;
    if (book->sequence) last = temporary[i];
  }
  for (uint32_t i = 0; i < book->dimensions; ++i) out[i] = temporary[i];
  return 0;
}

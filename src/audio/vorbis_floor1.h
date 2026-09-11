#ifndef CAPY_VORBIS_FLOOR1_H
#define CAPY_VORBIS_FLOOR1_H
#include "capy_audio.h"
#define CAPY_VORBIS_FLOOR1_POINTS 65u
#define CAPY_VORBIS_FLOOR1_BINS 4096u
struct capy_vorbis_floor1_plan {
  uint16_t x[CAPY_VORBIS_FLOOR1_POINTS];
  uint8_t low[CAPY_VORBIS_FLOOR1_POINTS], high[CAPY_VORBIS_FLOOR1_POINTS];
  uint8_t order[CAPY_VORBIS_FLOOR1_POINTS];
  uint8_t count, multiplier;
};
/* Private, integer-only floor1 curve stage. No allocation, FP, I/O or callbacks.
 * Prepare from setup X coordinates, preserving entry order. x[0]=0, x[1] is
 * a power of two <=32768; other points are distinct and between endpoints.
 * Output plan resets on failure and must remain immutable after success.
 * All buffers/descriptors must be nonoverlapping. */
int capy_vorbis_floor1_prepare(const uint16_t *x, size_t count,
    unsigned multiplier, struct capy_vorbis_floor1_plan *out);
/* Input is one present floor's decoded scalar Y values, NOT packet bytes.
 * Produce inverse-dB TABLE INDICES (0..255), not gains or PCM. Wrapped values
 * outside the multiplier's range fail closed. Output is unchanged on error.
 * count must equal plan.count; 1 <= bins <=4096, capacity >= bins.
 * Packet decoding, absent-floor handling and gain application are separate. */
int capy_vorbis_floor1_curve(const struct capy_vorbis_floor1_plan *plan,
    const uint32_t *wrapped, size_t count, size_t bins,
    uint8_t *out, size_t capacity);
#endif

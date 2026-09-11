#ifndef CAPY_VORBIS_COUPLING_H
#define CAPY_VORBIS_COUPLING_H

#include "capy_audio.h"

#define CAPY_VORBIS_COUPLING_CHANNELS 8u
#define CAPY_VORBIS_COUPLING_STEPS 256u
#define CAPY_VORBIS_SPECTRAL_BINS 4096u

/* Private inverse-coupling stage for already-decoded spectral residue vectors.
 * Vectors use channel-major storage; only the first `bins` values of each
 * `stride` are transformed. Coupling pairs are applied in reverse setup order.
 *
 * The caller supplies `channels * bins` scratch floats. The original vectors
 * remain unchanged on every error; padding between channel vectors is never
 * touched. Inputs, scratch and pair descriptors are immutable/non-overlapping
 * regions owned by the caller. No allocation, I/O, callbacks or PCM output.
 */
int capy_vorbis_inverse_coupling(
    float *vectors, size_t vector_capacity, size_t channels, size_t stride,
    size_t bins, const uint8_t *magnitude, const uint8_t *angle, size_t steps,
    float max_abs, float *scratch, size_t scratch_capacity);

#endif

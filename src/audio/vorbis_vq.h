#ifndef CAPY_VORBIS_VQ_H
#define CAPY_VORBIS_VQ_H
#include "vorbis_codebook.h"
/* Private Vorbis packed-float and vector expansion primitives. Requires normal
 * binary32/binary64 FP semantics (no fast-math; round-to-nearest and gradual
 * underflow). Host/runtime
 * owns FP initialization. No allocation, libm, I/O or callbacks. */
int capy_vorbis_float_unpack(uint32_t raw, float *out);
/* Expand one used symbol from a validated codebook into caller memory. The
 * caller establishes symbol membership using the Huffman decoder. Packet is
 * the immutable setup packet used to parse the book; offsets must refer to it.
 * max_abs is an explicit positive finite amplitude budget. Capacity >= dims.
 * Output remains unchanged on any error. No full codebook vector table is
 * allocated. Buffers/descriptors must not overlap. Not a PCM decoder. */
int capy_vorbis_vq_expand(const struct capy_vorbis_book *book,
    const uint8_t *packet, size_t packet_size, uint32_t symbol,
    float max_abs, float *out, size_t capacity);
#endif

#ifndef CAPY_VORBIS_HUFFMAN_H
#define CAPY_VORBIS_HUFFMAN_H
#include "vorbis_codebook.h"
struct capy_vorbis_huffman_node { uint32_t child[2], symbol; };
struct capy_vorbis_huffman {
  const struct capy_vorbis_huffman_node *nodes;
  uint32_t count, entries;
};
/* Private entropy decoder. Caller owns node storage, immutable after success.
 * Input/output/storage must not overlap. At most 2*used-1 nodes (one for the
 * single-symbol exception). No allocation. Output is reset on build failure;
 * partial storage must be discarded. Entry order follows Vorbis, not sorted
 * canonical Huffman order. Entries are limited to 65536 and lengths to 32. */
int capy_vorbis_huffman_build(const uint8_t *lengths, uint32_t entries,
    struct capy_vorbis_huffman_node *storage, size_t capacity,
    struct capy_vorbis_huffman *out);
/* At most 32 input bits; single-symbol books consume one bit (either value).
 * On failure symbol=UINT32_MAX and the bit reader retains a sticky error. */
int capy_vorbis_huffman_decode(const struct capy_vorbis_huffman *tree,
    struct capy_vorbis_bits *bits, uint32_t *symbol);
#endif

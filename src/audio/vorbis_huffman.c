#include "vorbis_huffman.h"
#define EMPTY UINT32_MAX
static void empty(struct capy_vorbis_huffman_node *node) {
  node->child[0] = node->child[1] = node->symbol = EMPTY;
}
int capy_vorbis_huffman_build(const uint8_t *lengths, uint32_t entries,
    struct capy_vorbis_huffman_node *storage, size_t capacity,
    struct capy_vorbis_huffman *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_huffman){0};
  if (!lengths || !storage || !entries) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (entries > 65536u) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  uint32_t counts[33] = {0}, used = 0, single = 0;
  for (uint32_t i = 0; i < entries; ++i) {
    if (lengths[i] > 32u) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    if (lengths[i]) { ++counts[lengths[i]]; ++used; single = i; }
  }
  uint64_t slots = 1;
  for (unsigned i = 1; i <= 32; ++i) {
    slots *= 2u;
    if (counts[i] > slots) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    slots -= counts[i];
  }
  if (!used || (slots && !(used == 1u && counts[1] == 1u)))
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  uint32_t needed = used * 2u - 1u;
  if (capacity < needed) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  empty(storage);
  uint32_t nodes = 1;
  if (used == 1u) storage[0].symbol = single;
  else {
    /* Free leftmost prefixes form at most one slot at each depth. Deeper
     * slots precede shallower ones lexicographically. Split the deepest slot
     * no deeper than the requested length; retain each right sibling. */
    uint32_t prefix[33] = {0};
    uint64_t free_depths = 1;
    for (uint32_t entry = 0; entry < entries; ++entry) {
      unsigned length = lengths[entry];
      if (!length) continue;
      int depth = (int)length;
      while (depth >= 0 && !(free_depths & (UINT64_C(1) << depth))) --depth;
      if (depth < 0) return CAPY_AUDIO_ERR_CORRUPT_DATA;
      uint32_t code = prefix[depth];
      free_depths &= ~(UINT64_C(1) << depth);
      while ((unsigned)depth < length) {
        ++depth;
        code <<= 1;
        prefix[depth] = code | 1u;
        free_depths |= UINT64_C(1) << depth;
      }
      uint32_t node = 0;
      for (unsigned bit = length; bit; --bit) {
        unsigned direction = (code >> (bit - 1u)) & 1u;
        uint32_t next = storage[node].child[direction];
        if (next == EMPTY) {
          if (nodes >= needed) return CAPY_AUDIO_ERR_CORRUPT_DATA;
          next = nodes++;
          empty(&storage[next]);
          storage[node].child[direction] = next;
        }
        node = next;
      }
      storage[node].symbol = entry;
    }
    if (free_depths || nodes != needed) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  out->nodes = storage;
  out->count = nodes;
  out->entries = entries;
  return 0;
}
int capy_vorbis_huffman_decode(const struct capy_vorbis_huffman *tree,
    struct capy_vorbis_bits *bits, uint32_t *symbol) {
  if (symbol) *symbol = EMPTY;
  if (!bits) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (bits->error) return bits->error;
  if (!tree || !tree->nodes || !tree->count || !symbol)
    return bits->error = CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  uint32_t node = 0;
  if (tree->count == 1u) {
    (void)capy_vorbis_bits_read(bits, 1);
    if (bits->error) return bits->error;
    if (tree->nodes[0].symbol >= tree->entries)
      return bits->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
    *symbol = tree->nodes[0].symbol;
    return 0;
  }
  for (unsigned depth = 0; depth < 32; ++depth) {
    unsigned bit = capy_vorbis_bits_read(bits, 1);
    if (bits->error) return bits->error;
    node = tree->nodes[node].child[bit];
    if (node >= tree->count) return bits->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
    if (tree->nodes[node].symbol != EMPTY) {
      if (tree->nodes[node].symbol >= tree->entries)
        return bits->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
      *symbol = tree->nodes[node].symbol;
      return 0;
    }
  }
  return bits->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
}

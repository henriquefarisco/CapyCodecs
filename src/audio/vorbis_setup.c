#include "vorbis_setup.h"
#include "vorbis_floor1_packet.h"
#include "vorbis_mapping.h"
#include "vorbis_huffman.h"
#define R(n) capy_vorbis_bits_read(b, (n))
static int bad(struct capy_vorbis_bits *b) {
  return b->error ? b->error : CAPY_AUDIO_ERR_CORRUPT_DATA;
}
static int floor_read(struct capy_vorbis_bits *b,
                       const struct capy_vorbis_book *books, uint32_t count,
                       uint8_t *floor_type,
                       struct capy_vorbis_floor0_config *floor0,
                       struct capy_vorbis_floor1_config *floor1) {
  uint32_t type = R(16);
  *floor_type = (uint8_t)type;
  if (type == 0u) {
    struct capy_vorbis_floor0_config config = {0};
    config.order = (uint16_t)R(8);
    config.rate = (uint16_t)R(16);
    config.bark_map_size = (uint16_t)R(16);
    config.amplitude_bits = (uint8_t)R(6);
    config.amplitude_offset = (uint8_t)R(8);
    config.book_count = (uint8_t)(R(4) + 1u);
    if (!config.order || !config.rate || !config.bark_map_size) return bad(b);
    uint32_t n = config.book_count;
    for (uint32_t i = 0; i < n; ++i) {
      uint32_t book = R(8);
      if (book >= count || !books[book].lookup_type) return bad(b);
      config.books[i] = (uint8_t)book;
    }
    if (b->error) return b->error;
    *floor0 = config;
  } else if (type == 1u) {
    return capy_vorbis_floor1_config_read(b,count,floor1);
  } else return bad(b);
  return b->error;
}
static int residue_read(struct capy_vorbis_bits *b,
                         const struct capy_vorbis_book *books, uint32_t count,
                         struct capy_vorbis_residue_config *out) {
  struct capy_vorbis_residue_config config = {0};
  for (unsigned i = 0; i < CAPY_VORBIS_RESIDUE_CLASSES; ++i)
    for (unsigned j = 0; j < CAPY_VORBIS_RESIDUE_PASSES; ++j)
      config.books[i][j] = -1;
  uint32_t type = R(16);
  config.begin = R(24);
  config.end = R(24);
  config.partition_size = R(24) + 1u;
  config.classifications = (uint8_t)(R(6) + 1u);
  config.classbook = (uint8_t)R(8);
  if (type > 2u || config.end < config.begin || config.classbook >= count)
    return bad(b);
  config.type = (uint8_t)type;
  uint32_t combinations = 1;
  for (uint32_t i = 0; i < books[config.classbook].dimensions; ++i) {
    if (combinations > books[config.classbook].entries / config.classifications)
      return bad(b);
    combinations *= config.classifications;
  }
  for (uint32_t i = 0; i < config.classifications; ++i) {
    uint32_t low = R(3), high = 0;
    if (R(1)) high = R(5);
    config.cascade[i] = (uint8_t)(low | (high << 3));
  }
  for (uint32_t i = 0; i < config.classifications; ++i)
    for (unsigned j = 0; j < 8; ++j)
      if (config.cascade[i] & (1u << j)) {
        uint32_t book = R(8);
        if (book >= count || !books[book].lookup_type) return bad(b);
        config.books[i][j] = (int16_t)book;
      }
  if (b->error) return b->error;
  *out = config;
  return 0;
}
static int setup_parse(const uint8_t *packet, size_t size,
    uint16_t channels, const struct capy_vorbis_setup_limits *limits,
    struct capy_vorbis_setup_workspace *workspace, uint8_t *lengths,
    size_t length_capacity, struct capy_vorbis_huffman *trees,
    size_t tree_capacity, struct capy_vorbis_huffman_node *nodes,
    size_t node_capacity, size_t *node_count,
    struct capy_vorbis_setup_summary *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_setup_summary){0};
  if (node_count) *node_count = 0;
  if (!packet || !limits || !workspace || !lengths || !length_capacity ||
      !channels || channels > CAPY_AUDIO_MAX_CHANNELS ||
      !limits->max_books || !limits->max_total_entries ||
      !limits->max_total_lookup_values)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if ((trees || nodes || node_count) &&
      (!trees || !nodes || !node_count || !tree_capacity || !node_capacity))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  struct capy_vorbis_bits reader, *b = &reader;
  int rc = capy_vorbis_bits_init(b, packet, size, limits->max_packet_bytes);
  if (rc) return rc;
  static const uint8_t signature[] = {5,'v','o','r','b','i','s'};
  for (unsigned i = 0; i < sizeof(signature); ++i) {
    if (R(8) != signature[i]) return bad(b);
  }
  struct capy_vorbis_setup_summary summary = {0};
  summary.books = R(8) + 1u;
  if (b->error) return b->error;
  if (summary.books > limits->max_books) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (trees && summary.books > tree_capacity)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  for (uint32_t i = 0; i < summary.books; ++i) {
    struct capy_vorbis_book_limits per_book = limits->book;
    uint32_t remaining = limits->max_total_entries - summary.entries;
    if (!remaining) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    if (per_book.max_entries > remaining) per_book.max_entries = remaining;
    remaining = limits->max_total_lookup_values - summary.lookup_values;
    /* A maptype-zero book uses no lookup entries. The primitive requires a
     * positive configured budget, so permit parsing one value then reject it
     * below when the aggregate budget is exhausted. No vector is allocated. */
    if (per_book.max_lookup_values > remaining)
      per_book.max_lookup_values = remaining ? remaining : 1u;
    rc = capy_vorbis_book_parse(b, &per_book, lengths, length_capacity,
                                &workspace->books[i]);
    if (rc) return rc;
    const struct capy_vorbis_book *book = &workspace->books[i];
    if (trees) {
      size_t used = *node_count;
      if (used > node_capacity) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
      rc = capy_vorbis_huffman_build(lengths, book->entries, nodes + used,
                                      node_capacity - used, &trees[i]);
      if (rc) return rc;
      if (trees[i].count > node_capacity - used)
        return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
      *node_count = used + trees[i].count;
    }
    if (book->lookup_values > remaining) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    summary.entries += book->entries;
    summary.lookup_values += book->lookup_values;
  }
  uint32_t times = R(6) + 1u;
  for (uint32_t i = 0; i < times; ++i) if (R(16)) return bad(b);
  summary.floors = R(6) + 1u;
  for (uint32_t i = 0; i < summary.floors; ++i) {
    rc = floor_read(b, workspace->books, summary.books,
                    &workspace->floor_type[i], &workspace->floor0[i],
                    &workspace->floor1[i]);
    if (rc) return rc;
  }
  summary.residues = R(6) + 1u;
  for (uint32_t i = 0; i < summary.residues; ++i) {
    rc = residue_read(b, workspace->books, summary.books,
                      &workspace->residues[i]);
    if (rc) return rc;
  }
  summary.mappings = R(6) + 1u;
  for (uint32_t i = 0; i < summary.mappings; ++i) {
    rc = capy_vorbis_mapping_read(b, channels, summary.floors,
                                  summary.residues, &workspace->mappings[i]);
    if (rc) return rc;
  }
  summary.modes = R(6) + 1u;
  for (uint32_t i = 0; i < summary.modes; ++i) {
    struct capy_vorbis_mode mode = {0};
    mode.blockflag = (uint8_t)R(1);
    if (R(16) || R(16)) return bad(b);
    mode.mapping = (uint8_t)R(8);
    if (mode.mapping >= summary.mappings) return bad(b);
    workspace->modes[i] = mode;
  }
  if (!R(1) || b->error) return bad(b);
  summary.consumed_bits = b->position;
  *out = summary;
  return 0;
}

int capy_vorbis_setup_validate(const uint8_t *packet, size_t size,
    uint16_t channels, const struct capy_vorbis_setup_limits *limits,
    struct capy_vorbis_setup_workspace *workspace, uint8_t *lengths,
    size_t length_capacity, struct capy_vorbis_setup_summary *out) {
  return setup_parse(packet, size, channels, limits, workspace, lengths,
                     length_capacity, 0, 0, 0, 0, 0, out);
}

int capy_vorbis_setup_prepare(const uint8_t *packet, size_t size,
    uint16_t channels, const struct capy_vorbis_setup_limits *limits,
    struct capy_vorbis_setup_workspace *workspace, uint8_t *lengths,
    size_t length_capacity, struct capy_vorbis_huffman *trees,
    size_t tree_capacity, struct capy_vorbis_huffman_node *nodes,
    size_t node_capacity, size_t *node_count,
    struct capy_vorbis_setup_summary *out) {
  if (!out || !node_count) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_setup_summary){0};
  *node_count = 0;
  size_t parsed_nodes = 0;
  struct capy_vorbis_setup_summary summary = {0};
  int rc = setup_parse(packet, size, channels, limits, workspace, lengths,
                       length_capacity, trees, tree_capacity, nodes,
                       node_capacity, &parsed_nodes, &summary);
  if (rc) return rc;
  *node_count = parsed_nodes;
  *out = summary;
  return CAPY_AUDIO_OK;
}

#ifndef CAPY_VORBIS_SETUP_H
#define CAPY_VORBIS_SETUP_H
#include "vorbis_floor1_packet.h"
#include "vorbis_mapping.h"

#define CAPY_VORBIS_SETUP_FLOORS 64u
#define CAPY_VORBIS_SETUP_RESIDUES 64u
#define CAPY_VORBIS_SETUP_MAPPINGS 64u
#define CAPY_VORBIS_SETUP_MODES 64u
#define CAPY_VORBIS_RESIDUE_CLASSES 64u
#define CAPY_VORBIS_RESIDUE_PASSES 8u

struct capy_vorbis_floor0_config {
  uint16_t order, rate, bark_map_size;
  uint8_t amplitude_bits, amplitude_offset, book_count;
  uint8_t books[16];
};

struct capy_vorbis_residue_config {
  uint32_t begin, end, partition_size;
  uint8_t type, classifications, classbook;
  uint8_t cascade[CAPY_VORBIS_RESIDUE_CLASSES];
  int16_t books[CAPY_VORBIS_RESIDUE_CLASSES][CAPY_VORBIS_RESIDUE_PASSES];
};

struct capy_vorbis_mode {
  uint8_t blockflag, mapping;
};

/* Private bounded retained setup state. It does not decode audio packets. */
struct capy_vorbis_setup_limits {
  size_t max_packet_bytes;
  uint32_t max_books, max_total_entries, max_total_lookup_values;
  struct capy_vorbis_book_limits book;
};
struct capy_vorbis_setup_summary {
  uint32_t books, floors, residues, mappings, modes, entries, lookup_values;
  size_t consumed_bits;
};
struct capy_vorbis_setup_workspace {
  struct capy_vorbis_book books[256];
  uint8_t floor_type[CAPY_VORBIS_SETUP_FLOORS];
  struct capy_vorbis_floor0_config floor0[CAPY_VORBIS_SETUP_FLOORS];
  struct capy_vorbis_floor1_config floor1[CAPY_VORBIS_SETUP_FLOORS];
  struct capy_vorbis_residue_config residues[CAPY_VORBIS_SETUP_RESIDUES];
  struct capy_vorbis_mapping mappings[CAPY_VORBIS_SETUP_MAPPINGS];
  struct capy_vorbis_mode modes[CAPY_VORBIS_SETUP_MODES];
};
/* Caller owns retained workspace and reusable one-book length scratch; no allocation.
 * Input/state/scratch/output must not overlap. Only output is reset on failure;
 * partial workspace/scratch must be discarded. On success the workspace owns
 * all setup descriptors, while codebook lookup values remain borrowed from
 * the immutable packet. Channels must come from a
 * successfully validated identification header. */
int capy_vorbis_setup_validate(const uint8_t *packet, size_t size,
    uint16_t channels, const struct capy_vorbis_setup_limits *limits,
    struct capy_vorbis_setup_workspace *workspace, uint8_t *lengths,
    size_t length_capacity, struct capy_vorbis_setup_summary *out);

/* Decoder-ready variant: while parsing each book, retain its Huffman tree in
 * caller-owned node storage before the reusable length scratch is overwritten.
 * On failure summary/node_count reset; workspace, trees and nodes are partial
 * scratch and must be discarded. Tree node pointers borrow `nodes`. */
int capy_vorbis_setup_prepare(const uint8_t *packet, size_t size,
    uint16_t channels, const struct capy_vorbis_setup_limits *limits,
    struct capy_vorbis_setup_workspace *workspace, uint8_t *lengths,
    size_t length_capacity, struct capy_vorbis_huffman *trees,
    size_t tree_capacity, struct capy_vorbis_huffman_node *nodes,
    size_t node_capacity, size_t *node_count,
    struct capy_vorbis_setup_summary *out);
#endif

#ifndef CAPY_VORBIS_FLOOR1_PACKET_H
#define CAPY_VORBIS_FLOOR1_PACKET_H
#include "vorbis_floor1.h"
#include "vorbis_huffman.h"
struct capy_vorbis_floor1_class {
  uint8_t dimensions, subclasses;
  uint16_t masterbook;
  int16_t books[8]; /* -1 is an unused subclass. */
};
struct capy_vorbis_floor1_config {
  struct capy_vorbis_floor1_plan plan;
  struct capy_vorbis_floor1_class classes[16];
  uint8_t partition_class[31], partitions;
  uint16_t book_count;
};
struct capy_vorbis_floor1_values {
  uint32_t y[65];
  uint8_t present, exhausted;
};
/* Private setup reader. Starts AFTER the 16-bit floor type (must be type 1).
 * Owns no heap or packet storage. Output resets on failure. The successful
 * caller-owned config and supplied Huffman trees must remain immutable.
 * All descriptors/buffers must not overlap. */
int capy_vorbis_floor1_config_read(struct capy_vorbis_bits *bits,
    unsigned book_count, struct capy_vorbis_floor1_config *out);
/* Reads one channel's floor at the current audio-packet bit position.
 * Success can mean absent (present=0). Packet exhaustion is nominal Vorbis:
 * returns 0 with present=0, exhausted=1 and keeps the reader's TRUNCATED error.
 * The future packet orchestrator must then zero ALL channels and go to overlap,
 * not continue decoding residue. Other errors return nonzero, clearing output.
 * No partial Y values are published. Not a complete audio-packet decoder. */
int capy_vorbis_floor1_packet_read(const struct capy_vorbis_floor1_config *config,
    const struct capy_vorbis_huffman *books, size_t book_count,
    struct capy_vorbis_bits *bits, struct capy_vorbis_floor1_values *out);
#endif

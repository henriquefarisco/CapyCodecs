#ifndef CAPY_VORBIS_AUDIO_PACKET_H
#define CAPY_VORBIS_AUDIO_PACKET_H

#include "vorbis_residue_decode.h"
#include "vorbis_packet.h"

struct capy_vorbis_audio_packet_result {
  struct capy_vorbis_packet_window window;
  uint32_t vectors;
  uint8_t exhausted;
};

/* Private audio packet entropy orchestrator. It reads mode/floors and decodes
 * each mapping submap as its own dense channel bundle, including residue type
 * 2. Output spectrum and floor values are transactional; packet bits are not.
 * Floor entropy exhaustion is nominal and publishes an all-silent packet.
 *
 * `work` requires 5*channels*(block_size/2) floats. `classes` requires
 * channels*(block_size/2) bytes. `floor_scratch` requires `channels` entries.
 * All output and scratch regions are caller-owned and non-overlapping. */
int capy_vorbis_audio_packet_decode(
    const uint8_t *packet, size_t packet_size, size_t max_packet_bytes,
    const uint8_t *setup_packet, size_t setup_size,
    const struct capy_vorbis_setup_summary *summary,
    const struct capy_vorbis_setup_workspace *setup,
    const struct capy_vorbis_huffman *trees, size_t tree_count,
    unsigned channels, unsigned short_block, unsigned long_block,
    uint32_t max_vectors, float max_abs,
    float *spectrum, size_t spectrum_capacity,
    struct capy_vorbis_floor1_values *floors,
    struct capy_vorbis_floor1_values *floor_scratch,
    float *work, size_t work_capacity, uint8_t *classes,
    size_t class_capacity, struct capy_vorbis_audio_packet_result *result);

#endif

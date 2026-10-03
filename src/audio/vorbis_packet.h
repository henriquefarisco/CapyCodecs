#ifndef CAPY_VORBIS_PACKET_H
#define CAPY_VORBIS_PACKET_H

#include "vorbis_setup.h"

struct capy_vorbis_packet_window {
  uint16_t block_size, left_start, left_end, right_start, right_end;
  uint8_t mode, mapping, blockflag, previous_window, next_window;
};

/* Private audio-packet prefix reader. Setup is retained and validated; block
 * sizes come from the identification header. Output resets on failure. The
 * reader keeps sticky truncation/corruption and never rolls bits back. */
int capy_vorbis_packet_window_read(struct capy_vorbis_bits *bits,
    const struct capy_vorbis_setup_summary *summary,
    const struct capy_vorbis_setup_workspace *setup,
    unsigned short_block, unsigned long_block,
    struct capy_vorbis_packet_window *out);

#endif

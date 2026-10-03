#ifndef CAPY_OGG_READER_H
#define CAPY_OGG_READER_H

#include "capy_audio.h"

/* Private development interface, not capy-codec-audio ABI. One complete,
 * immutable, single logical stream; no chaining/multiplexing or resync.
 * Caller owns state/input and a non-overlapping packet scratch buffer.
 * No allocation, I/O, callbacks or hidden mutable globals. */
struct capy_ogg_reader {
  const uint8_t *data;
  size_t size, offset, body, page_end, packet_size, max_packet_bytes;
  uint32_t serial, sequence, pages, max_pages;
  uint64_t page_granule, packet_granule;
  unsigned segment, segments, flags;
  int started, continued, ended, packet_has_granule, error;
  const uint8_t *laces;
};

int capy_ogg_reader_init(struct capy_ogg_reader *reader,
                         const uint8_t *data, size_t size,
                         size_t max_input_bytes, size_t max_packet_bytes,
                         uint32_t max_pages);
/* 1: packet (including legal zero-length packets); 0: verified EOS;
 * negative: capy_audio_error, sticky until reinitialized. The same scratch
 * must be supplied on every call. Output length is zero on EOF/error. */
int capy_ogg_reader_next(struct capy_ogg_reader *reader, uint8_t *scratch,
                         size_t capacity, size_t *packet_size);
/* Consume and validate a complete packet, retaining only its bounded prefix.
 * packet_size is the full length, NOT the number of bytes copied. */
int capy_ogg_reader_next_prefix(struct capy_ogg_reader *reader, uint8_t *scratch,
                                size_t capacity, size_t *packet_size);

#endif

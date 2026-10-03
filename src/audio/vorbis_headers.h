#ifndef CAPY_VORBIS_HEADERS_H
#define CAPY_VORBIS_HEADERS_H

#include "capy_audio.h"

/* Private packet-level parsers. No PCM capability or setup validation implied.
 * Caller supplies immutable packets and explicit limits. No retained pointers,
 * allocation or I/O. Output is zeroed on every failure. */
struct capy_vorbis_identification {
  uint32_t sample_rate;
  uint16_t channels, blocksize_small, blocksize_large;
};
struct capy_vorbis_comments {
  uint32_t vendor_bytes, comment_count;
  size_t comment_bytes;
};
int capy_vorbis_parse_identification(const uint8_t *packet, size_t size,
    const struct capy_audio_limits *limits,
    struct capy_vorbis_identification *out);
/* Validates framing/lengths only; does not expose or render untrusted strings.
 * max_input_bytes limits the packet; max_chunks limits the comment count.
 * Strict policy: malformed/truncated comments return an error; a future caller
 * may explicitly discard optional tags without treating this as valid metadata. */
int capy_vorbis_parse_comments(const uint8_t *packet, size_t size,
    const struct capy_audio_limits *limits, struct capy_vorbis_comments *out);

#endif

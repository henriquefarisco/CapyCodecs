#include "ogg_reader.h"

static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
         (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t le64(const uint8_t *p) {
  return (uint64_t)le32(p) | (uint64_t)le32(p + 4u) << 32;
}

/* Ogg direct CRC: zero initial/final XOR, checksum field treated as zero.
 * Each page is at most 65307 bytes; total work is linear in bounded input. */
static uint32_t page_crc(const uint8_t *p, size_t size) {
  uint32_t crc = 0;
  size_t i;
  unsigned bit;
  for (i = 0; i < size; ++i) {
    crc ^= (uint32_t)((i >= 22u && i < 26u) ? 0u : p[i]) << 24;
    for (bit = 0; bit < 8u; ++bit)
      crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04c11db7u : 0u);
  }
  return crc;
}

int capy_ogg_reader_init(struct capy_ogg_reader *r,
                         const uint8_t *data, size_t size,
                         size_t max_input_bytes, size_t max_packet_bytes,
                         uint32_t max_pages) {
  if (!r) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *r = (struct capy_ogg_reader){0};
  r->error = CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (!data || !max_input_bytes || !max_packet_bytes || !max_pages)
    return r->error;
  if (size > max_input_bytes) {
    r->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    return r->error;
  }
  r->data = data;
  r->size = size;
  r->max_packet_bytes = max_packet_bytes;
  r->max_pages = max_pages;
  r->error = 0;
  return CAPY_AUDIO_OK;
}

static int next_page(struct capy_ogg_reader *r) {
  const uint8_t *p;
  size_t header, body = 0;
  unsigned i, flags, segments;
  uint32_t serial, sequence;
  if (r->pages >= r->max_pages) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (r->size - r->offset < 27u) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  p = r->data + r->offset;
  if (p[0] != 'O' || p[1] != 'g' || p[2] != 'g' || p[3] != 'S')
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (p[4] != 0u) return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  flags = p[5];
  if (flags & ~7u) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  segments = p[26];
  header = 27u + segments;
  if (header > r->size - r->offset) return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  for (i = 0; i < segments; ++i) body += p[27u + i];
  if (body > r->size - r->offset - header)
    return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  if (page_crc(p, header + body) != le32(p + 22u))
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  serial = le32(p + 14u);
  sequence = le32(p + 18u);
  if (!r->started) {
    if (!(flags & 2u) || (flags & 1u) || sequence != 0u)
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
  } else {
    if (serial != r->serial) return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
    if ((flags & 2u) || sequence != r->sequence + 1u ||
        !!(flags & 1u) != r->continued)
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  /* Empty pages cannot terminate an unfinished packet. */
  if ((flags & 4u) &&
      ((segments && p[26u + segments] == 255u) ||
       (!segments && r->continued)))
    return CAPY_AUDIO_ERR_TRUNCATED_DATA;
  r->serial = serial;
  r->sequence = sequence;
  r->started = 1;
  ++r->pages;
  r->flags = flags;
  r->page_granule = le64(p + 6u);
  r->segments = segments;
  r->segment = 0;
  r->laces = p + 27u;
  r->body = r->offset + header;
  r->page_end = r->body + body;
  r->offset = r->page_end;
  return 0;
}

int capy_ogg_reader_next(struct capy_ogg_reader *r, uint8_t *scratch,
                         size_t capacity, size_t *packet_size) {
  if (packet_size) *packet_size = 0;
  if (!r || !scratch || !packet_size) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  r->packet_has_granule = 0;
  if (r->error) return r->error;
  if (capacity < r->max_packet_bytes)
    return r->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (r->ended) return 0;
  for (;;) {
    if (r->segment == r->segments) {
      if (r->started && (r->flags & 4u)) {
        if (r->offset != r->size)
          return r->error = CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
        r->ended = 1;
        return 0;
      }
      int rc = next_page(r);
      if (rc) return r->error = rc;
      continue;
    }
    size_t length = r->laces[r->segment++];
    if (length > r->max_packet_bytes - r->packet_size)
      return r->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    for (size_t i = 0; i < length; ++i)
      scratch[r->packet_size + i] = r->data[r->body + i];
    r->body += length;
    r->packet_size += length;
    r->continued = length == 255u;
    if (!r->continued) {
      unsigned later_complete = 0;
      for (unsigned segment = r->segment; segment < r->segments; ++segment)
        if (r->laces[segment] < 255u) later_complete = 1;
      if (!later_complete && r->page_granule != UINT64_MAX) {
        r->packet_granule = r->page_granule;
        r->packet_has_granule = 1;
      }
      *packet_size = r->packet_size;
      r->packet_size = 0;
      return 1;
    }
  }
}

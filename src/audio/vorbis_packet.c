#include "vorbis_packet.h"

static int bad(struct capy_vorbis_bits *bits) {
  if (!bits->error) bits->error = CAPY_AUDIO_ERR_CORRUPT_DATA;
  return bits->error;
}

static int valid_block(unsigned value) {
  return value >= 64u && value <= 8192u && !(value & (value - 1u));
}

int capy_vorbis_packet_window_read(struct capy_vorbis_bits *bits,
    const struct capy_vorbis_setup_summary *summary,
    const struct capy_vorbis_setup_workspace *setup,
    unsigned short_block, unsigned long_block,
    struct capy_vorbis_packet_window *out) {
  struct capy_vorbis_packet_window window = {0};
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = window;
  if (!bits || !bits->data || bits->position > bits->bit_count || !summary ||
      !setup || !summary->modes || summary->modes > CAPY_VORBIS_SETUP_MODES ||
      !summary->mappings || summary->mappings > CAPY_VORBIS_SETUP_MAPPINGS ||
      !valid_block(short_block) || !valid_block(long_block) ||
      short_block > long_block)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (bits->error) return bits->error;
  if (capy_vorbis_bits_read(bits, 1)) return bad(bits);

  unsigned width = 0;
  for (uint32_t value = summary->modes - 1u; value; value >>= 1) ++width;
  uint32_t mode = capy_vorbis_bits_read(bits, width);
  if (bits->error) return bits->error;
  if (mode >= summary->modes || setup->modes[mode].mapping >= summary->mappings)
    return bad(bits);
  window.mode = (uint8_t)mode;
  window.mapping = setup->modes[mode].mapping;
  window.blockflag = setup->modes[mode].blockflag;
  if (window.blockflag > 1u) return bad(bits);
  if (window.blockflag) {
    window.previous_window = (uint8_t)capy_vorbis_bits_read(bits, 1);
    window.next_window = (uint8_t)capy_vorbis_bits_read(bits, 1);
    if (bits->error) return bits->error;
  }

  unsigned n = window.blockflag ? long_block : short_block;
  window.block_size = (uint16_t)n;
  if (window.blockflag && !window.previous_window) {
    window.left_start = (uint16_t)(n / 4u - short_block / 4u);
    window.left_end = (uint16_t)(n / 4u + short_block / 4u);
  } else {
    window.left_start = 0;
    window.left_end = (uint16_t)(n / 2u);
  }
  if (window.blockflag && !window.next_window) {
    window.right_start = (uint16_t)(3u * n / 4u - short_block / 4u);
    window.right_end = (uint16_t)(3u * n / 4u + short_block / 4u);
  } else {
    window.right_start = (uint16_t)(n / 2u);
    window.right_end = (uint16_t)n;
  }
  *out = window;
  return CAPY_AUDIO_OK;
}

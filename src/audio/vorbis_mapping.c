#include "vorbis_mapping.h"

#define R(n) capy_vorbis_bits_read(bits, (n))

static int bad(struct capy_vorbis_bits *bits) {
  return bits->error ? bits->error : CAPY_AUDIO_ERR_CORRUPT_DATA;
}

int capy_vorbis_mapping_read(struct capy_vorbis_bits *bits,
    unsigned channels, unsigned floors, unsigned residues,
    struct capy_vorbis_mapping *out) {
  struct capy_vorbis_mapping mapping = {0};
  unsigned width = 0;

  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = mapping;
  if (!bits || !bits->data || bits->position > bits->bit_count ||
      !channels || channels > CAPY_AUDIO_MAX_CHANNELS ||
      !floors || floors > 64u || !residues || residues > 64u)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (bits->error) return bits->error;
  if (R(16)) return bad(bits);
  mapping.submaps = 1;
  if (R(1)) mapping.submaps = (uint8_t)(R(4) + 1u);
  if (R(1)) {
    mapping.coupling_steps = (uint16_t)(R(8) + 1u);
    for (unsigned value = channels - 1u; value; value >>= 1) ++width;
    for (uint16_t i = 0; i < mapping.coupling_steps; ++i) {
      uint32_t magnitude = R(width);
      uint32_t angle = R(width);
      if (magnitude == angle || magnitude >= channels || angle >= channels)
        return bad(bits);
      mapping.magnitude[i] = (uint8_t)magnitude;
      mapping.angle[i] = (uint8_t)angle;
    }
  }
  if (R(2)) return bad(bits);
  if (mapping.submaps > 1u) {
    for (unsigned channel = 0; channel < channels; ++channel) {
      uint32_t mux = R(4);
      if (mux >= mapping.submaps) return bad(bits);
      mapping.mux[channel] = (uint8_t)mux;
    }
  }
  for (uint8_t submap = 0; submap < mapping.submaps; ++submap) {
    (void)R(8); /* unused time placeholder: deliberately ignored by Vorbis I */
    uint32_t floor = R(8);
    uint32_t residue = R(8);
    if (floor >= floors || residue >= residues) return bad(bits);
    mapping.floor[submap] = (uint8_t)floor;
    mapping.residue[submap] = (uint8_t)residue;
  }
  if (bits->error) return bits->error;
  *out = mapping;
  return CAPY_AUDIO_OK;
}

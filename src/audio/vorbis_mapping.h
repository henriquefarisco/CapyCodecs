#ifndef CAPY_VORBIS_MAPPING_H
#define CAPY_VORBIS_MAPPING_H

#include "vorbis_codebook.h"

#define CAPY_VORBIS_MAPPING_SUBMAPS 16u
#define CAPY_VORBIS_MAPPING_COUPLING_STEPS 256u

struct capy_vorbis_mapping {
  uint16_t coupling_steps;
  uint8_t submaps;
  uint8_t magnitude[CAPY_VORBIS_MAPPING_COUPLING_STEPS];
  uint8_t angle[CAPY_VORBIS_MAPPING_COUPLING_STEPS];
  uint8_t mux[CAPY_AUDIO_MAX_CHANNELS];
  uint8_t floor[CAPY_VORBIS_MAPPING_SUBMAPS];
  uint8_t residue[CAPY_VORBIS_MAPPING_SUBMAPS];
};

/* Private retained mapping-type-0 parser. `bits` is positioned immediately
 * before the 16-bit mapping type. Counts come from already-validated setup
 * state. Output resets on failure; consumed bits are not rolled back.
 * No allocation, I/O, callbacks or synthesis. */
int capy_vorbis_mapping_read(struct capy_vorbis_bits *bits,
    unsigned channels, unsigned floors, unsigned residues,
    struct capy_vorbis_mapping *out);

#endif

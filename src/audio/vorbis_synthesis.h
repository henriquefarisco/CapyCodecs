#ifndef CAPY_VORBIS_SYNTHESIS_H
#define CAPY_VORBIS_SYNTHESIS_H

#include "vorbis_floor1_gain.h"
#include "vorbis_mdct.h"
#include "vorbis_setup.h"

struct capy_vorbis_synthesis_state {
  uint16_t previous_size;
  uint8_t channels, primed;
};

struct capy_vorbis_synthesis_result {
  size_t frames;
  uint8_t primed;
};

/* Private post-entropy synthesis stage. `spectrum` contains channel-major
 * residue vectors for the selected mapping and is consumed even on error.
 * A floor packet marked exhausted makes the whole packet silent, as required
 * by Vorbis. Hard errors leave PCM, previous-window storage and state intact.
 *
 * `previous` is dense channel-major storage with capacity channels*8192. Once
 * primed, block overlap is bound to the original channel count.
 * Let n=block_size and frames=previous_size/4+n/4 when primed, otherwise zero.
 * `work` needs (channels+1)*n + 2*max(n,frames) floats. Allocating
 * (channels+3)*the_stream_maximum_block_size is sufficient for every packet.
 * `pcm` and `pcm_scratch` each need frames*channels floats; `curve` needs n/2
 * bytes. All mutable buffers must be distinct. The first packet primes overlap
 * state and intentionally produces zero frames. */
int capy_vorbis_synthesis_finish(
    const struct capy_vorbis_setup_summary *summary,
    const struct capy_vorbis_setup_workspace *setup,
    const struct capy_vorbis_packet_window *window,
    const struct capy_vorbis_mdct_plan *plan,
    const struct capy_vorbis_floor1_values *floors, unsigned channels,
    float max_abs, float *spectrum, size_t spectrum_capacity,
    float *previous, size_t previous_capacity,
    float *pcm, size_t pcm_capacity,
    float *pcm_scratch, size_t pcm_scratch_capacity,
    float *work, size_t work_capacity, uint8_t *curve, size_t curve_capacity,
    struct capy_vorbis_synthesis_state *state,
    struct capy_vorbis_synthesis_result *result);

#endif

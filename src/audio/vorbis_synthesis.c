#include "vorbis_synthesis.h"

#include "vorbis_coupling.h"

#include <float.h>

#ifdef __FAST_MATH__
#error "Vorbis synthesis requires finite checks; disable fast-math"
#endif

static int valid_block(size_t n) {
  return n >= 64u && n <= CAPY_VORBIS_BLOCK_SAMPLES && !(n & (n - 1u));
}

static int overlap(const void *a, size_t a_size, const void *b, size_t b_size) {
  uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
  if (a_size > UINTPTR_MAX - aa || b_size > UINTPTR_MAX - bb) return 1;
  return aa < bb + b_size && bb < aa + a_size;
}

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
    struct capy_vorbis_synthesis_result *result) {
  struct capy_vorbis_synthesis_result done = {0};
  if (!result) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *result = done;
  if (!summary || !setup || !window || !plan || !floors || !channels ||
      channels > CAPY_AUDIO_MAX_CHANNELS || !(max_abs > 0.0f) ||
      max_abs > FLT_MAX || !spectrum || !previous || !pcm || !pcm_scratch ||
      !work || !curve || !state || state->primed > 1u ||
      window->mapping >= summary->mappings || !summary->mappings ||
      summary->mappings > CAPY_VORBIS_SETUP_MAPPINGS)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;

  size_t n = window->block_size, bins = n / 2u;
  if (!valid_block(n) || plan->n != n || window->left_start > window->left_end ||
      window->left_end > window->right_start ||
      window->right_start > window->right_end || window->right_end > n)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (channels > SIZE_MAX / bins || channels > SIZE_MAX / n ||
      spectrum_capacity < channels * bins ||
      previous_capacity < channels * CAPY_VORBIS_BLOCK_SAMPLES ||
      curve_capacity < bins)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (state->primed &&
      (!valid_block(state->previous_size) || state->channels != channels))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;

  size_t frames = state->primed ? state->previous_size / 4u + n / 4u : 0u;
  if (frames && channels > SIZE_MAX / frames)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  size_t samples = frames * channels;
  /* A long-to-short overlap can produce more samples than the current block.
   * Both overlap buffers need that capacity; the window scratch still needs n.
   * All sizes are bounded by the validated channel and block limits above. */
  size_t aux_size = frames > n ? frames : n;
  size_t work_size = channels * n + 2u * aux_size + n;
  if (pcm_capacity < samples || pcm_scratch_capacity < samples ||
      work_capacity < work_size)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;

  size_t spectrum_bytes = channels * bins * sizeof(*spectrum);
  size_t previous_bytes = channels * CAPY_VORBIS_BLOCK_SAMPLES * sizeof(*previous);
  size_t pcm_bytes = samples * sizeof(*pcm);
  size_t work_bytes = work_size * sizeof(*work);
  if (overlap(spectrum, spectrum_bytes, previous, previous_bytes) ||
      overlap(spectrum, spectrum_bytes, work, work_bytes) ||
      overlap(previous, previous_bytes, work, work_bytes) ||
      (samples && (overlap(pcm, pcm_bytes, pcm_scratch, pcm_bytes) ||
                   overlap(pcm, pcm_bytes, previous, previous_bytes) ||
                   overlap(pcm_scratch, pcm_bytes, previous, previous_bytes) ||
                   overlap(pcm, pcm_bytes, work, work_bytes) ||
                   overlap(pcm_scratch, pcm_bytes, work, work_bytes))))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;

  const struct capy_vorbis_mapping *mapping = &setup->mappings[window->mapping];
  if (!mapping->submaps || mapping->submaps > CAPY_VORBIS_MAPPING_SUBMAPS ||
      mapping->coupling_steps > CAPY_VORBIS_MAPPING_COUPLING_STEPS)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  for (unsigned channel = 0; channel < channels; ++channel) {
    if (mapping->mux[channel] >= mapping->submaps) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    unsigned floor = mapping->floor[mapping->mux[channel]];
    if (floor >= summary->floors || setup->floor_type[floor] != 1u)
      return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  }

  float *blocks = work;
  float *aux0 = blocks + channels * n;
  float *aux1 = aux0 + aux_size;
  float *aux2 = aux1 + aux_size;
  int rc = capy_vorbis_inverse_coupling(spectrum, channels * bins, channels,
      bins, bins, mapping->magnitude, mapping->angle, mapping->coupling_steps,
      max_abs, blocks, channels * bins);
  if (rc) return rc;

  int silent = 0;
  for (unsigned channel = 0; channel < channels; ++channel)
    if (floors[channel].exhausted) silent = 1;

  for (unsigned channel = 0; channel < channels; ++channel) {
    float *channel_spectrum = spectrum + (size_t)channel * bins;
    if (silent || !floors[channel].present) {
      for (size_t i = 0; i < bins; ++i) channel_spectrum[i] = 0.0f;
    } else {
      unsigned floor = mapping->floor[mapping->mux[channel]];
      const struct capy_vorbis_floor1_plan *floor_plan = &setup->floor1[floor].plan;
      rc = capy_vorbis_floor1_curve(floor_plan, floors[channel].y,
                                    floor_plan->count, bins, curve, curve_capacity);
      if (rc) return rc;
      rc = capy_vorbis_floor1_apply_gain(curve, bins, max_abs,
          channel_spectrum, bins, aux0, n);
      if (rc) return rc;
    }
    rc = capy_vorbis_mdct_backward(plan, channel_spectrum, bins, max_abs,
                                    aux0, n, aux1, n);
    if (rc) return rc;
    rc = capy_vorbis_window_apply(window, aux0, n, max_abs,
                                   blocks + (size_t)channel * n, n, aux2, n);
    if (rc) return rc;
  }

  if (state->primed) {
    for (unsigned channel = 0; channel < channels; ++channel) {
      size_t produced = 0;
      rc = capy_vorbis_overlap_add(
          previous + (size_t)channel * state->previous_size,
          state->previous_size, blocks + (size_t)channel * n, n, max_abs,
          aux0, aux_size, aux1, aux_size, &produced);
      if (rc) return rc;
      if (produced != frames) return CAPY_AUDIO_ERR_CORRUPT_DATA;
      for (size_t frame = 0; frame < frames; ++frame)
        pcm_scratch[frame * channels + channel] = aux0[frame];
    }
  }

  for (size_t i = 0; i < samples; ++i) pcm[i] = pcm_scratch[i];
  for (unsigned channel = 0; channel < channels; ++channel)
    for (size_t i = 0; i < n; ++i)
      previous[(size_t)channel * n + i] = blocks[(size_t)channel * n + i];
  state->previous_size = (uint16_t)n;
  state->channels = (uint8_t)channels;
  state->primed = 1;
  done.frames = frames;
  done.primed = 1;
  *result = done;
  return CAPY_AUDIO_OK;
}

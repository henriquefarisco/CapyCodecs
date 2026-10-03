#include "vorbis_audio_packet.h"

#include <float.h>

#ifdef __FAST_MATH__
#error "Vorbis packet decoding requires finite checks; disable fast-math"
#endif

static int overlap(const void *a, size_t a_size, const void *b, size_t b_size) {
  uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
  if (a_size > UINTPTR_MAX - aa || b_size > UINTPTR_MAX - bb) return 1;
  return aa < bb + b_size && bb < aa + a_size;
}

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
    size_t class_capacity, struct capy_vorbis_audio_packet_result *result) {
  struct capy_vorbis_audio_packet_result done = {0};
  if (!result) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *result = done;
  if (!packet || !packet_size || !max_packet_bytes || !setup_packet ||
      !setup_size || !summary || !setup || !trees || !summary->books ||
      summary->books > 256u || tree_count < summary->books || !channels ||
      channels > CAPY_AUDIO_MAX_CHANNELS || !max_vectors ||
      !(max_abs > 0.0f && max_abs <= FLT_MAX) || !spectrum || !floors ||
      !floor_scratch || !work || !classes)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;

  struct capy_vorbis_bits bits;
  int rc = capy_vorbis_bits_init(&bits, packet, packet_size, max_packet_bytes);
  if (rc) return rc;
  rc = capy_vorbis_packet_window_read(&bits, summary, setup, short_block,
                                       long_block, &done.window);
  if (rc) return rc;
  size_t bins = done.window.block_size / 2u;
  if (channels > SIZE_MAX / bins) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  size_t total = channels * bins;
  if (spectrum_capacity < total || work_capacity < 5u * total ||
      class_capacity < total)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  const void *mutable_regions[] = {spectrum, floors, floor_scratch, work, classes};
  const size_t mutable_sizes[] = {
    total * sizeof(*spectrum), channels * sizeof(*floors),
    channels * sizeof(*floor_scratch), 5u * total * sizeof(*work), total
  };
  for (size_t i = 0; i < sizeof(mutable_regions) / sizeof(mutable_regions[0]); ++i) {
    if (overlap(mutable_regions[i], mutable_sizes[i], packet, packet_size) ||
        overlap(mutable_regions[i], mutable_sizes[i], setup_packet, setup_size))
      return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
    for (size_t j = i + 1u;
         j < sizeof(mutable_regions) / sizeof(mutable_regions[0]); ++j)
      if (overlap(mutable_regions[i], mutable_sizes[i],
                  mutable_regions[j], mutable_sizes[j]))
        return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }

  const struct capy_vorbis_mapping *mapping = &setup->mappings[done.window.mapping];
  if (!mapping->submaps || mapping->submaps > CAPY_VORBIS_MAPPING_SUBMAPS ||
      mapping->coupling_steps > CAPY_VORBIS_MAPPING_COUPLING_STEPS)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;

  uint8_t skip[CAPY_AUDIO_MAX_CHANNELS] = {0};
  for (unsigned channel = 0; channel < channels; ++channel) {
    if (mapping->mux[channel] >= mapping->submaps)
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    unsigned floor = mapping->floor[mapping->mux[channel]];
    if (floor >= summary->floors) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    if (setup->floor_type[floor] != 1u)
      return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
    rc = capy_vorbis_floor1_packet_read(&setup->floor1[floor], trees,
        summary->books, &bits, &floor_scratch[channel]);
    if (rc) return rc;
    skip[channel] = (uint8_t)!floor_scratch[channel].present;
    if (floor_scratch[channel].exhausted) {
      for (unsigned c = 0; c < channels; ++c) {
        floor_scratch[c] = (struct capy_vorbis_floor1_values){0};
        floor_scratch[c].exhausted = 1;
      }
      for (size_t i = 0; i < total; ++i) work[i] = 0.0f;
      done.exhausted = 1;
      goto commit;
    }
  }

  for (size_t step = 0; step < mapping->coupling_steps; ++step) {
    unsigned magnitude = mapping->magnitude[step], angle = mapping->angle[step];
    if (magnitude >= channels || angle >= channels || magnitude == angle)
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    if (!skip[magnitude] || !skip[angle]) skip[magnitude] = skip[angle] = 0;
  }

  float *staged = work;
  float *bundle = staged + total;
  float *residue_work = bundle + total;
  for (size_t i = 0; i < total; ++i) staged[i] = 0.0f;
  uint32_t vectors = 0;
  for (unsigned submap = 0; submap < mapping->submaps; ++submap) {
    uint8_t channel_index[CAPY_AUDIO_MAX_CHANNELS];
    uint8_t bundle_skip[CAPY_AUDIO_MAX_CHANNELS];
    unsigned bundle_channels = 0;
    for (unsigned channel = 0; channel < channels; ++channel)
      if (mapping->mux[channel] == submap) {
        channel_index[bundle_channels] = (uint8_t)channel;
        bundle_skip[bundle_channels] = skip[channel];
        for (size_t bin = 0; bin < bins; ++bin)
          bundle[(size_t)bundle_channels * bins + bin] =
              staged[(size_t)channel * bins + bin];
        ++bundle_channels;
      }
    if (!bundle_channels) continue;
    unsigned residue = mapping->residue[submap];
    if (residue >= summary->residues) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    struct capy_vorbis_residue_decode_result decoded = {0};
    rc = capy_vorbis_residue_decode(&setup->residues[residue], setup->books,
        trees, summary->books, setup_packet, setup_size, &bits,
        bundle_channels, bundle_skip, bins, max_vectors - vectors, max_abs,
        bundle, (size_t)bundle_channels * bins,
        residue_work, 3u * total, classes, class_capacity, &decoded);
    if (rc) return rc;
    if (decoded.vectors > max_vectors - vectors)
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    vectors += decoded.vectors;
    if (decoded.exhausted) done.exhausted = 1;
    for (unsigned local = 0; local < bundle_channels; ++local) {
      unsigned channel = channel_index[local];
      for (size_t bin = 0; bin < bins; ++bin)
        staged[(size_t)channel * bins + bin] =
            bundle[(size_t)local * bins + bin];
    }
  }
  done.vectors = vectors;

commit:
  for (size_t i = 0; i < total; ++i) spectrum[i] = work[i];
  for (unsigned channel = 0; channel < channels; ++channel)
    floors[channel] = floor_scratch[channel];
  *result = done;
  return CAPY_AUDIO_OK;
}

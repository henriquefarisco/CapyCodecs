#include "vorbis_residue_decode.h"

#include <float.h>

#ifdef __FAST_MATH__
#error "Vorbis residue scheduling requires finite checks; disable fast-math"
#endif

static int config_valid(const struct capy_vorbis_residue_config *config,
                        size_t book_count) {
  if (!config || config->type > 2u || config->end < config->begin ||
      !config->partition_size || !config->classifications ||
      config->classifications > CAPY_VORBIS_RESIDUE_CLASSES ||
      config->classbook >= book_count) {
    return 0;
  }
  for (unsigned c = 0; c < config->classifications; ++c)
    for (unsigned pass = 0; pass < CAPY_VORBIS_RESIDUE_PASSES; ++pass) {
      int book = config->books[c][pass];
      if (config->cascade[c] & (1u << pass)) {
        if (book < 0 || (size_t)book >= book_count) return 0;
      } else if (book != -1) return 0;
    }
  return 1;
}

static int classify(const struct capy_vorbis_residue_config *config,
                    const struct capy_vorbis_huffman *trees,
                    struct capy_vorbis_bits *audio, uint8_t *classes,
                    size_t at, size_t count, size_t dimensions) {
  uint32_t word;
  int rc = capy_vorbis_huffman_decode(&trees[config->classbook], audio, &word);
  if (rc) return rc;
  /* A final partial group uses the prefix of the full classbook word, not
   * its least-significant digits. Discard the unused suffix without writing
   * beyond the caller's exact partition-sized classification scratch. */
  for (size_t i = dimensions; i; --i) {
    if (i <= count)
      classes[at + i - 1u] = (uint8_t)(word % config->classifications);
    word /= config->classifications;
  }
  return 0;
}

static void commit(float *out, const float *scratch, size_t count) {
  for (size_t i = 0; i < count; ++i) out[i] = scratch[i];
}

int capy_vorbis_residue_decode(
    const struct capy_vorbis_residue_config *config,
    const struct capy_vorbis_book *books,
    const struct capy_vorbis_huffman *trees, size_t book_count,
    const uint8_t *setup, size_t setup_size, struct capy_vorbis_bits *audio,
    unsigned channels, const uint8_t *skip, size_t bins,
    uint32_t max_vectors, float max_abs,
    float *out, size_t out_capacity, float *scratch, size_t scratch_capacity,
    uint8_t *classes, size_t class_capacity,
    struct capy_vorbis_residue_decode_result *result) {
  struct capy_vorbis_residue_decode_result done = {0};
  if (!result) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *result = done;
  if (!books || !trees || !book_count || book_count > 256u || !setup ||
      !setup_size || !audio || !audio->data || audio->position > audio->bit_count ||
      !channels || channels > CAPY_AUDIO_MAX_CHANNELS || !skip || !bins ||
      bins > CAPY_VORBIS_FLOOR1_BINS || !(max_abs > 0.0f && max_abs <= FLT_MAX) ||
      !out || !scratch || !classes || !config_valid(config, book_count)) {
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  }

  size_t total = (size_t)channels * bins;
  if (out_capacity < total || scratch_capacity < total)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (audio->error && audio->error != CAPY_AUDIO_ERR_TRUNCATED_DATA)
    return audio->error;
  for (size_t i = 0; i < total; ++i) {
    if (!(out[i] <= max_abs && out[i] >= -max_abs))
      return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    scratch[i] = out[i];
  }

  unsigned active = 0;
  for (unsigned channel = 0; channel < channels; ++channel)
    if (!skip[channel]) ++active;
  if (!active) {
    *result = done;
    return CAPY_AUDIO_OK;
  }

  size_t domain = config->type == 2u ? total : bins;
  size_t end = config->end < domain ? config->end : domain;
  if (config->begin >= end) {
    *result = done;
    return CAPY_AUDIO_OK;
  }
  size_t partitions = (end - config->begin) / config->partition_size;
  if (!partitions) {
    *result = done;
    return CAPY_AUDIO_OK;
  }
  size_t class_channels = config->type == 2u ? 1u : active;
  if (partitions > CAPY_VORBIS_FLOOR1_BINS ||
      class_channels > class_capacity / partitions)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  size_t temp_count = config->partition_size * (config->type == 2u ? 2u : 1u);
  if (temp_count > scratch_capacity - total)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;

  const struct capy_vorbis_book *classbook = &books[config->classbook];
  if (!classbook->dimensions || classbook->dimensions > 256u ||
      !trees[config->classbook].nodes || !trees[config->classbook].count)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  size_t classwords = classbook->dimensions;
  float *partition_scratch = scratch + total;
  float *interleaved = partition_scratch + config->partition_size;

  for (unsigned pass = 0; pass < CAPY_VORBIS_RESIDUE_PASSES; ++pass) {
    for (size_t partition = 0; partition < partitions;) {
      size_t group = partitions - partition;
      if (group > classwords) group = classwords;
      size_t class_slot = 0;
      if (config->type == 2u) {
        if (pass == 0) {
          int rc = classify(config, trees, audio, classes, partition, group,
                            classwords);
          if (rc == CAPY_AUDIO_ERR_TRUNCATED_DATA) goto exhausted;
          if (rc) return rc;
        }
      } else {
        for (unsigned channel = 0; channel < channels; ++channel) {
          if (skip[channel]) continue;
          if (pass == 0) {
            int rc = classify(config, trees, audio, classes,
                              class_slot * partitions + partition, group,
                              classwords);
            if (rc == CAPY_AUDIO_ERR_TRUNCATED_DATA) goto exhausted;
            if (rc) return rc;
          }
          ++class_slot;
        }
      }

      for (size_t offset = 0; offset < group; ++offset) {
        size_t current = partition + offset;
        if (config->type == 2u) {
          int book_index = config->books[classes[current]][pass];
          if (book_index >= 0) {
            size_t flat = config->begin + current * config->partition_size;
            for (size_t i = 0; i < config->partition_size; ++i) {
              size_t sample = flat + i;
              interleaved[i] = scratch[(sample % channels) * bins + sample / channels];
            }
            struct capy_vorbis_residue_result part = {0};
            int rc = capy_vorbis_residue_partition(1u, &books[book_index],
                &trees[book_index], setup, setup_size, audio,
                config->partition_size, max_vectors - done.vectors, max_abs,
                interleaved, config->partition_size, partition_scratch,
                config->partition_size, &part);
            done.vectors += part.vectors;
            if (rc) return rc;
            for (size_t i = 0; i < config->partition_size; ++i) {
              size_t sample = flat + i;
              scratch[(sample % channels) * bins + sample / channels] = interleaved[i];
            }
            ++done.partitions;
            if (part.exhausted) goto exhausted;
          }
        } else {
          class_slot = 0;
          for (unsigned channel = 0; channel < channels; ++channel) {
            if (skip[channel]) continue;
            int book_index = config->books[
                classes[class_slot * partitions + current]][pass];
            if (book_index >= 0) {
              size_t start = (size_t)channel * bins + config->begin +
                             current * config->partition_size;
              struct capy_vorbis_residue_result part = {0};
              int rc = capy_vorbis_residue_partition(config->type,
                  &books[book_index], &trees[book_index], setup, setup_size,
                  audio, config->partition_size, max_vectors - done.vectors,
                  max_abs, scratch + start, config->partition_size,
                  partition_scratch, config->partition_size, &part);
              done.vectors += part.vectors;
              if (rc) return rc;
              ++done.partitions;
              if (part.exhausted) goto exhausted;
            }
            ++class_slot;
          }
        }
      }
      partition += group;
    }
  }
  commit(out, scratch, total);
  *result = done;
  return CAPY_AUDIO_OK;

exhausted:
  done.exhausted = 1;
  commit(out, scratch, total);
  *result = done;
  return CAPY_AUDIO_OK;
}

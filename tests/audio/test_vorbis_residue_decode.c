#include "vorbis_residue_decode.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct capy_vorbis_bits reader(const uint8_t *data, size_t bits) {
  struct capy_vorbis_bits value;
  assert(capy_vorbis_bits_init(&value, data, 1, 1) == 0);
  value.bit_count = bits;
  return value;
}

static void config(struct capy_vorbis_residue_config *value, unsigned type,
                   unsigned end, unsigned partition_size) {
  memset(value, 0, sizeof(*value));
  for (unsigned i = 0; i < CAPY_VORBIS_RESIDUE_CLASSES; ++i)
    for (unsigned j = 0; j < CAPY_VORBIS_RESIDUE_PASSES; ++j)
      value->books[i][j] = -1;
  value->type = (uint8_t)type;
  value->end = end;
  value->partition_size = partition_size;
  value->classifications = 1;
  value->classbook = 0;
  value->cascade[0] = 1;
  value->books[0][0] = 1;
}

int main(void) {
  const uint8_t lengths[] = {1, 1}, setup[] = {0x21, 0x43}, audio_data[] = {0};
  struct capy_vorbis_huffman_node nodes[2][3];
  struct capy_vorbis_huffman trees[2];
  assert(capy_vorbis_huffman_build(lengths, 2, nodes[0], 3, &trees[0]) == 0);
  assert(capy_vorbis_huffman_build(lengths, 2, nodes[1], 3, &trees[1]) == 0);
  struct capy_vorbis_book books[2] = {{0}};
  books[0].entries = 2; books[0].dimensions = 2;
  books[1].entries = 2; books[1].dimensions = 2; books[1].lookup_type = 2;
  books[1].lookup_values = 4; books[1].value_bits = 4;
  books[1].delta_raw = 0x60100000u;
  float out[8], scratch[16];
  uint8_t classes[8], skip[2] = {0, 0};
  struct capy_vorbis_residue_config cfg;
  struct capy_vorbis_residue_decode_result result;

  for (unsigned type = 0; type < 2; ++type) {
    config(&cfg, type, 4, 4);
    memset(out, 0, sizeof(out));
    struct capy_vorbis_bits bits = reader(audio_data, 6);
    assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
        &bits, 2, skip, 4, 8, 100, out, 8, scratch, 16,
        classes, 8, &result) == 0);
    const float expected[2][4] = {{1,1,2,2}, {1,2,1,2}};
    for (unsigned channel = 0; channel < 2; ++channel)
      for (unsigned i = 0; i < 4; ++i)
        assert(out[channel * 4 + i] == expected[type][i]);
    assert(result.partitions == 2 && result.vectors == 4 && !result.exhausted);
  }

  config(&cfg, 2, 8, 4);
  memset(out, 0, sizeof(out));
  struct capy_vorbis_bits bits = reader(audio_data, 5);
  assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
      &bits, 2, skip, 4, 8, 100, out, 8, scratch, 16,
      classes, 8, &result) == 0);
  const float type2[] = {1,1,1,1, 2,2,2,2};
  for (unsigned i = 0; i < 8; ++i) assert(out[i] == type2[i]);
  assert(result.partitions == 2 && result.vectors == 4 && !result.exhausted);

  skip[1] = 1;
  config(&cfg, 1, 4, 4);
  memset(out, 0, sizeof(out));
  bits = reader(audio_data, 3);
  assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
      &bits, 2, skip, 4, 8, 100, out, 8, scratch, 16,
      classes, 8, &result) == 0);
  assert(out[0] == 1 && out[1] == 2 && out[2] == 1 && out[3] == 2 &&
         out[4] == 0 && bits.position == 3);

  skip[0] = 1;
  bits = reader(audio_data, 0);
  assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
      &bits, 2, skip, 4, 8, 100, out, 8, scratch, 16,
      classes, 8, &result) == 0);
  assert(bits.position == 0 && !result.partitions && !result.exhausted);

  skip[0] = skip[1] = 0;
  memset(out, 0, sizeof(out));
  bits = reader(audio_data, 3);
  assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
      &bits, 2, skip, 4, 8, 100, out, 8, scratch, 16,
      classes, 8, &result) == 0);
  assert(result.exhausted && result.partitions == 1 && result.vectors == 1);
  assert(out[0] == 1 && out[1] == 2 && out[4] == 0);

  float before[8]; memcpy(before, out, sizeof(out));
  bits = reader(audio_data, 4);
  assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
      &bits, 2, skip, 4, 0, 100, out, 8, scratch, 16,
      classes, 8, &result) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(memcmp(before, out, sizeof(out)) == 0 && !result.partitions);
  cfg.cascade[0] = 0;
  assert(capy_vorbis_residue_decode(&cfg, books, trees, 2, setup, 2,
      &bits, 2, skip, 4, 8, 100, out, 8, scratch, 16,
      classes, 8, &result) == CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(memcmp(before, out, sizeof(out)) == 0);

  puts("[vorbis-residue-decode] types 0/1/2, skip, exhaustion and failures passed");
  return 0;
}

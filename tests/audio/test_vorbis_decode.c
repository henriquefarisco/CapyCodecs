#include "vorbis_decode.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct allocations { size_t calls, fail, live; };
static void *allocate(size_t n, void *context) {
  struct allocations *a = context;
  if (++a->calls == a->fail) return 0;
  void *p = malloc(n);
  if (p) ++a->live;
  return p;
}
static void release(void *p, void *context) {
  struct allocations *a = context;
  assert(p && a->live);
  --a->live;
  free(p);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  FILE *f = fopen(argv[1], "rb"); assert(f);
  assert(fseek(f, 0, SEEK_END) == 0);
  long bytes = ftell(f); assert(bytes > 0);
  rewind(f);
  size_t size = (size_t)bytes;
  uint8_t *input = malloc(size); assert(input);
  assert(fread(input, 1, size, f) == size); fclose(f);
  struct allocations allocations = {0};
  struct capy_audio_allocator allocator = {allocate, release, &allocations};
  struct capy_audio_limits limits;
  capy_audio_default_limits(&limits);
  struct capy_vorbis_decode_budget budget = {16u << 20, 4096, 16000000};
  struct capy_audio_pcm pcm;
  struct capy_audio_metadata metadata;
  enum capy_audio_container container;
  assert(capy_audio_detect_memory(input, size, &container) == 0);
  assert(container == CAPY_AUDIO_CONTAINER_OGG_VORBIS);
  assert(capy_audio_query_memory(input, size, &limits, &metadata) == 0);
  assert(capy_audio_decode_memory_limited(input, size, &allocator, &limits, &pcm) == 0);
  assert(metadata.frame_count == pcm.metadata.frame_count &&
         metadata.pcm_bytes == pcm.metadata.pcm_bytes &&
         pcm.metadata.container == CAPY_AUDIO_CONTAINER_OGG_VORBIS);
  capy_audio_pcm_free(&pcm);
  /* The kernel consumer uses a stricter policy than the library defaults. */
  struct capy_audio_limits kernel_limits = {
    .max_input_bytes = 8u << 20, .max_output_bytes = 8u << 20,
    .max_frames = (8u << 20) / 2u, .max_sample_rate = 48000,
    .max_channels = 2, .max_chunks = 4096
  };
  assert(capy_audio_decode_memory_limited(input, size, &allocator,
                                         &kernel_limits, &pcm) == 0);
  capy_audio_pcm_free(&pcm);
  allocations = (struct allocations){0};
  assert(capy_vorbis_decode_memory(input, size, &allocator, &limits, &budget, &pcm) == 0);
  assert(pcm.samples && pcm.metadata.channels == 2 && pcm.metadata.sample_rate == 48000);
  size_t calls = allocations.calls;
  f = fopen(argv[2], "wb"); assert(f);
  assert(fwrite(pcm.samples, 1, pcm.metadata.pcm_bytes, f) == pcm.metadata.pcm_bytes);
  fclose(f);
  capy_audio_pcm_free(&pcm); assert(!allocations.live);

  for (size_t fail = 1; fail <= calls; ++fail) {
    allocations = (struct allocations){.fail = fail};
    assert(capy_vorbis_decode_memory(input, size, &allocator, &limits, &budget, &pcm) ==
           CAPY_AUDIO_ERR_OUT_OF_MEMORY);
    assert(!allocations.live && !pcm.samples && !pcm.metadata.frame_count);
  }
  allocations = (struct allocations){0};
  for (unsigned test = 0; test < 6; ++test) {
    struct capy_audio_limits limited = limits;
    struct capy_vorbis_decode_budget small = budget;
    if (test == 0) small.max_work_bytes = 1;
    if (test == 1) small.max_packets = 1;
    if (test == 2) small.max_vectors = 1;
    if (test == 3) limited.max_output_bytes = 2;
    if (test == 4) limited.max_frames = 1;
    if (test == 5) limited.max_input_bytes = size - 1;
    assert(capy_vorbis_decode_memory(input, size, &allocator, &limited, &small, &pcm) ==
           CAPY_AUDIO_ERR_RESOURCE_LIMIT);
    assert(!allocations.live && !pcm.samples);
  }
  for (size_t n = 0; n < size; n += size / 13u + 1u) {
    assert(capy_vorbis_decode_memory(input, n, &allocator, &limits, &budget, &pcm) < 0);
    assert(!allocations.live && !pcm.samples);
  }
  input[size-1u] ^= 0x80;
  assert(capy_audio_query_memory(input, size, &limits, &metadata) < 0);
  assert(!metadata.frame_count);
  assert(capy_vorbis_decode_memory(input, size, &allocator, &limits, &budget, &pcm) < 0);
  assert(!allocations.live && !pcm.samples);
  free(input);
  printf("[vorbis-decode] bounded decode, %zu allocation failures, quotas, truncation and CRC: PASS\n", calls);
  return 0;
}

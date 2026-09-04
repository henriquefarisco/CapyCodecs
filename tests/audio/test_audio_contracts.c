#include "capy_audio.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int failures;

#define EXPECT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr);                 \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

struct test_heap {
  uint8_t bytes[256];
  size_t used;
  int frees;
  int fail;
};

static void *test_alloc(size_t size, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  if (!heap || heap->fail || size == 0u || size > sizeof(heap->bytes)) {
    return 0;
  }
  heap->used = size;
  return heap->bytes;
}

static void test_free(void *ptr, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  if (ptr && heap) {
    ++heap->frees;
  }
}

static const uint8_t wav_s16_stereo[] = {
    'R','I','F','F', 44,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0, 1,0, 2,0,
    0x80,0xBB,0,0, 0x00,0xEE,0x02,0, 4,0, 16,0,
    'd','a','t','a', 8,0,0,0, 0,0, 0xFF,0x7F, 0,0x80, 0xFF,0xFF};

static const uint8_t wav_s24_mono[] = {
    'R','I','F','F', 42,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
    0x80,0xBB,0,0, 0x80,0x32,0x02,0, 3,0, 24,0,
    'd','a','t','a', 6,0,0,0, 0,0,0, 0xFF,0xFF,0x7F};

static struct capy_audio_allocator allocator_for(struct test_heap *heap) {
  struct capy_audio_allocator allocator;
  allocator.alloc = test_alloc;
  allocator.free = test_free;
  allocator.user_data = heap;
  return allocator;
}

static void test_abi_and_defaults(void) {
  struct capy_audio_limits limits;
  uint32_t features = capy_audio_codec_features();
  capy_audio_default_limits(&limits);
  EXPECT(capy_audio_abi_version() == 1u);
  EXPECT((features & CAPY_AUDIO_FEATURE_WAV_PCM_DECODE) != 0u);
  EXPECT((features & CAPY_AUDIO_FEATURE_ALLOCATOR_INJECTION) != 0u);
  EXPECT((features & CAPY_AUDIO_FEATURE_PER_CALL_LIMITS) != 0u);
  EXPECT(limits.max_channels == CAPY_AUDIO_MAX_CHANNELS);
  EXPECT(limits.max_sample_rate == CAPY_AUDIO_MAX_SAMPLE_RATE);
  EXPECT(limits.max_output_bytes == CAPY_AUDIO_MAX_OUTPUT_BYTES);
  EXPECT(capy_audio_strerror(CAPY_AUDIO_ERR_CORRUPT_DATA)[0] != '\0');
  EXPECT(capy_audio_strerror((enum capy_audio_error)-99)[0] != '\0');
  EXPECT(capy_audio_sample_format_name(CAPY_AUDIO_SAMPLE_S16_LE)[0] == 's');
}

static void test_query_decode_and_free(void) {
  struct capy_audio_metadata metadata;
  struct capy_audio_pcm pcm;
  struct test_heap heap = {{0}, 0u, 0, 0};
  struct capy_audio_allocator allocator = allocator_for(&heap);
  enum capy_audio_container container = CAPY_AUDIO_CONTAINER_UNKNOWN;
  EXPECT(capy_audio_detect_memory(wav_s16_stereo, sizeof(wav_s16_stereo),
                                  &container) == CAPY_AUDIO_OK);
  EXPECT(container == CAPY_AUDIO_CONTAINER_WAV);
  EXPECT(capy_audio_query_memory(wav_s16_stereo, sizeof(wav_s16_stereo), 0,
                                 &metadata) == CAPY_AUDIO_OK);
  EXPECT(metadata.sample_format == CAPY_AUDIO_SAMPLE_S16_LE);
  EXPECT(metadata.sample_rate == 48000u);
  EXPECT(metadata.channels == 2u);
  EXPECT(metadata.frame_count == 2u);
  EXPECT(metadata.pcm_bytes == 8u);
  EXPECT(capy_audio_decode_memory(wav_s16_stereo, sizeof(wav_s16_stereo),
                                  &allocator, &pcm) == CAPY_AUDIO_OK);
  EXPECT(pcm.samples == heap.bytes);
  EXPECT(pcm.samples[2] == 0xFFu && pcm.samples[3] == 0x7Fu);
  capy_audio_pcm_free(&pcm);
  EXPECT(heap.frees == 1);
  EXPECT(pcm.samples == 0 && pcm.metadata.frame_count == 0u);
}

static void test_supported_sample_formats(void) {
  struct capy_audio_metadata metadata;
  uint8_t wav32[sizeof(wav_s16_stereo)];
  size_t i;
  EXPECT(capy_audio_query_memory(wav_s24_mono, sizeof(wav_s24_mono), 0,
                                 &metadata) == CAPY_AUDIO_OK);
  EXPECT(metadata.sample_format == CAPY_AUDIO_SAMPLE_S24_LE_PACKED);
  EXPECT(metadata.block_align == 3u && metadata.frame_count == 2u);

  for (i = 0; i < sizeof(wav32); ++i) wav32[i] = wav_s16_stereo[i];
  wav32[22] = 1u;
  wav32[32] = 4u;
  wav32[34] = 32u;
  EXPECT(capy_audio_query_memory(wav32, sizeof(wav32), 0, &metadata) ==
         CAPY_AUDIO_OK);
  EXPECT(metadata.sample_format == CAPY_AUDIO_SAMPLE_S32_LE);
  EXPECT(metadata.channels == 1u && metadata.frame_count == 2u);
  wav32[20] = 3u;
  EXPECT(capy_audio_query_memory(wav32, sizeof(wav32), 0, &metadata) ==
         CAPY_AUDIO_OK);
  EXPECT(metadata.sample_format == CAPY_AUDIO_SAMPLE_F32_LE);
}

static void test_fail_closed_cases(void) {
  struct capy_audio_metadata metadata;
  struct capy_audio_pcm pcm;
  struct capy_audio_limits limits;
  struct test_heap heap = {{0}, 0u, 0, 0};
  struct capy_audio_allocator allocator = allocator_for(&heap);
  uint8_t broken[sizeof(wav_s16_stereo)];
  size_t i;

  for (i = 0; i < sizeof(broken); ++i) broken[i] = wav_s16_stereo[i];
  EXPECT(capy_audio_query_memory(0, 0u, 0, &metadata) ==
         CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  EXPECT(capy_audio_query_memory(wav_s16_stereo, 8u, 0, &metadata) ==
         CAPY_AUDIO_ERR_TRUNCATED_DATA);
  broken[8] = 'X';
  EXPECT(capy_audio_query_memory(broken, sizeof(broken), 0, &metadata) ==
         CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT);

  for (i = 0; i < sizeof(broken); ++i) broken[i] = wav_s16_stereo[i];
  broken[4] = 60u;
  EXPECT(capy_audio_query_memory(broken, sizeof(broken), 0, &metadata) ==
         CAPY_AUDIO_ERR_TRUNCATED_DATA);
  broken[4] = 44u;
  broken[32] = 2u;
  EXPECT(capy_audio_query_memory(broken, sizeof(broken), 0, &metadata) ==
         CAPY_AUDIO_ERR_CORRUPT_DATA);

  for (i = 0; i < sizeof(broken); ++i) broken[i] = wav_s16_stereo[i];
  broken[20] = 6u;
  EXPECT(capy_audio_query_memory(broken, sizeof(broken), 0, &metadata) ==
         CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT);
  for (i = 0; i < sizeof(broken); ++i) broken[i] = wav_s16_stereo[i];
  broken[44] = 7u;
  EXPECT(capy_audio_query_memory(broken, sizeof(broken) - 1u, 0, &metadata) ==
         CAPY_AUDIO_ERR_TRUNCATED_DATA);

  capy_audio_default_limits(&limits);
  limits.max_output_bytes = 4u;
  EXPECT(capy_audio_query_memory(wav_s16_stereo, sizeof(wav_s16_stereo),
                                 &limits, &metadata) ==
         CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  capy_audio_default_limits(&limits);
  limits.max_frames = 1u;
  EXPECT(capy_audio_query_memory(wav_s16_stereo, sizeof(wav_s16_stereo),
                                 &limits, &metadata) ==
         CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  capy_audio_default_limits(&limits);
  limits.max_chunks = 1u;
  EXPECT(capy_audio_query_memory(wav_s16_stereo, sizeof(wav_s16_stereo),
                                 &limits, &metadata) ==
         CAPY_AUDIO_ERR_RESOURCE_LIMIT);

  for (i = 0; i < sizeof(broken); ++i) broken[i] = wav_s16_stereo[i];
  broken[4] = 36u;
  broken[40] = 0u;
  EXPECT(capy_audio_query_memory(broken, sizeof(broken), 0, &metadata) ==
         CAPY_AUDIO_ERR_CORRUPT_DATA);

  heap.fail = 1;
  EXPECT(capy_audio_decode_memory(wav_s16_stereo, sizeof(wav_s16_stereo),
                                  &allocator, &pcm) ==
         CAPY_AUDIO_ERR_OUT_OF_MEMORY);
  EXPECT(pcm.samples == 0 && pcm.metadata.pcm_bytes == 0u);
}

int main(void) {
  test_abi_and_defaults();
  test_query_decode_and_free();
  test_supported_sample_formats();
  test_fail_closed_cases();
  if (failures != 0) {
    fprintf(stderr, "audio contract tests failed: %d\n", failures);
    return 1;
  }
  puts("audio contract tests passed");
  return 0;
}

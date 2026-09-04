#ifndef CAPY_CODECS_AUDIO_H
#define CAPY_CODECS_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define CAPY_AUDIO_ABI_VERSION 1u
#define CAPY_AUDIO_MAX_CHANNELS 8u
#define CAPY_AUDIO_MAX_SAMPLE_RATE 192000u
#define CAPY_AUDIO_MAX_OUTPUT_BYTES (64u * 1024u * 1024u)
#define CAPY_AUDIO_MAX_CHUNKS 4096u

#define CAPY_AUDIO_FEATURE_WAV_PCM_DECODE 0x00000001u
#define CAPY_AUDIO_FEATURE_INTERLEAVED_OUTPUT 0x00000002u
#define CAPY_AUDIO_FEATURE_ALLOCATOR_INJECTION 0x00000004u
#define CAPY_AUDIO_FEATURE_PER_CALL_LIMITS 0x00000008u
#define CAPY_AUDIO_FEATURE_METADATA 0x00000010u

enum capy_audio_error {
  CAPY_AUDIO_OK = 0,
  CAPY_AUDIO_ERR_INVALID_ARGUMENT = -1,
  CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT = -2,
  CAPY_AUDIO_ERR_CORRUPT_DATA = -3,
  CAPY_AUDIO_ERR_TRUNCATED_DATA = -4,
  CAPY_AUDIO_ERR_OUT_OF_MEMORY = -5,
  CAPY_AUDIO_ERR_RESOURCE_LIMIT = -6
};

enum capy_audio_container {
  CAPY_AUDIO_CONTAINER_UNKNOWN = 0,
  CAPY_AUDIO_CONTAINER_WAV = 1
};

enum capy_audio_sample_format {
  CAPY_AUDIO_SAMPLE_UNKNOWN = 0,
  CAPY_AUDIO_SAMPLE_S16_LE = 1,
  CAPY_AUDIO_SAMPLE_S24_LE_PACKED = 2,
  CAPY_AUDIO_SAMPLE_S32_LE = 3,
  CAPY_AUDIO_SAMPLE_F32_LE = 4
};

typedef void *(*capy_audio_alloc_fn)(size_t size, void *user_data);
typedef void (*capy_audio_free_fn)(void *ptr, void *user_data);

struct capy_audio_allocator {
  capy_audio_alloc_fn alloc;
  capy_audio_free_fn free;
  void *user_data;
};

struct capy_audio_limits {
  size_t max_input_bytes;
  size_t max_output_bytes;
  uint64_t max_frames;
  uint32_t max_sample_rate;
  uint16_t max_channels;
  uint32_t max_chunks;
};

struct capy_audio_metadata {
  enum capy_audio_container container;
  enum capy_audio_sample_format sample_format;
  uint32_t sample_rate;
  uint16_t channels;
  uint16_t bits_per_sample;
  uint16_t block_align;
  uint64_t frame_count;
  size_t pcm_bytes;
};

struct capy_audio_pcm {
  struct capy_audio_metadata metadata;
  uint8_t *samples;
  struct capy_audio_allocator allocator;
};

uint32_t capy_audio_abi_version(void);
uint32_t capy_audio_codec_features(void);
void capy_audio_default_limits(struct capy_audio_limits *limits);
int capy_audio_detect_memory(const uint8_t *data, size_t size,
                             enum capy_audio_container *out_container);
int capy_audio_query_memory(const uint8_t *data, size_t size,
                            const struct capy_audio_limits *limits,
                            struct capy_audio_metadata *out_metadata);
int capy_audio_decode_memory(const uint8_t *data, size_t size,
                             const struct capy_audio_allocator *allocator,
                             struct capy_audio_pcm *out);
int capy_audio_decode_memory_limited(
    const uint8_t *data, size_t size,
    const struct capy_audio_allocator *allocator,
    const struct capy_audio_limits *limits, struct capy_audio_pcm *out);
const char *capy_audio_strerror(enum capy_audio_error error);
const char *capy_audio_sample_format_name(enum capy_audio_sample_format format);
void capy_audio_pcm_free(struct capy_audio_pcm *pcm);

#endif

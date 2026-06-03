#ifndef CAPY_CODECS_IMAGE_H
#define CAPY_CODECS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#define CAPY_IMAGE_MAX_WIDTH 4096u
#define CAPY_IMAGE_MAX_HEIGHT 4096u
#define CAPY_IMAGE_ABI_VERSION 2u
#define CAPY_IMAGE_FEATURE_BMP_DECODE 0x00000001u
#define CAPY_IMAGE_FEATURE_PNG_DECODE 0x00000002u
#define CAPY_IMAGE_FEATURE_JPEG_DECODE 0x00000004u
#define CAPY_IMAGE_FEATURE_ARGB32_OUTPUT 0x00000008u
#define CAPY_IMAGE_FEATURE_ALLOCATOR_INJECTION 0x00000010u
#define CAPY_IMAGE_FEATURE_PNG_INFLATER_INJECTION 0x00000020u
#define CAPY_IMAGE_FEATURE_PER_CALL_LIMITS 0x00000040u
#define CAPY_IMAGE_FEATURE_DETECT 0x00000080u
#define CAPY_IMAGE_FEATURE_GENERIC_DECODE 0x00000100u
#define CAPY_IMAGE_FEATURE_METADATA 0x00000200u
#define CAPY_IMAGE_FEATURE_QOI_DECODE 0x00000400u

enum capy_image_error {
  CAPY_IMAGE_OK = 0,
  CAPY_IMAGE_ERR_INVALID_ARGUMENT = -1,
  CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT = -2,
  CAPY_IMAGE_ERR_CORRUPT_DATA = -3,
  CAPY_IMAGE_ERR_TRUNCATED_DATA = -4,
  CAPY_IMAGE_ERR_OUT_OF_MEMORY = -5,
  CAPY_IMAGE_ERR_RESOURCE_LIMIT = -6,
  CAPY_IMAGE_ERR_INFLATER_FAILED = -7
};

enum capy_image_format {
  CAPY_IMAGE_FORMAT_UNKNOWN = 0,
  CAPY_IMAGE_FORMAT_BMP = 1,
  CAPY_IMAGE_FORMAT_PNG = 2,
  CAPY_IMAGE_FORMAT_JPEG = 3,
  CAPY_IMAGE_FORMAT_QOI = 4
};

typedef void *(*capy_image_alloc_fn)(size_t size, void *user_data);
typedef void (*capy_image_free_fn)(void *ptr, void *user_data);
typedef int (*capy_image_inflate_fn)(uint8_t *dest, size_t *dest_len,
                                     const uint8_t *source,
                                     size_t source_len, void *user_data);

struct capy_image_allocator {
  capy_image_alloc_fn alloc;
  capy_image_free_fn free;
  void *user_data;
};

struct capy_image_inflater {
  capy_image_inflate_fn inflate;
  void *user_data;
};

struct capy_image_rgba32 {
  uint32_t width;
  uint32_t height;
  uint32_t *pixels;
  struct capy_image_allocator allocator;
};

struct capy_image_limits {
  uint32_t max_width;
  uint32_t max_height;
  size_t max_output_bytes;
  size_t max_temporary_bytes;
};

struct capy_image_metadata {
  enum capy_image_format format;
  uint32_t width;
  uint32_t height;
  uint32_t channels;
  uint32_t bits_per_channel;
  uint32_t has_alpha;
};

uint32_t capy_image_abi_version(void);
uint32_t capy_image_codec_features(void);
void capy_image_default_limits(struct capy_image_limits *limits);
int capy_bmp_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           struct capy_image_rgba32 *out);
int capy_bmp_decode_memory_limited(const uint8_t *data, size_t size,
                                   const struct capy_image_allocator *allocator,
                                   const struct capy_image_limits *limits,
                                   struct capy_image_rgba32 *out);
int capy_png_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           const struct capy_image_inflater *inflater,
                           struct capy_image_rgba32 *out);
int capy_png_decode_memory_limited(const uint8_t *data, size_t size,
                                   const struct capy_image_allocator *allocator,
                                   const struct capy_image_inflater *inflater,
                                   const struct capy_image_limits *limits,
                                   struct capy_image_rgba32 *out);
int capy_jpeg_decode_memory(const uint8_t *data, size_t size,
                            const struct capy_image_allocator *allocator,
                            struct capy_image_rgba32 *out);
int capy_jpeg_decode_memory_limited(const uint8_t *data, size_t size,
                                    const struct capy_image_allocator *allocator,
                                    const struct capy_image_limits *limits,
                                    struct capy_image_rgba32 *out);
int capy_qoi_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           struct capy_image_rgba32 *out);
int capy_qoi_decode_memory_limited(const uint8_t *data, size_t size,
                                   const struct capy_image_allocator *allocator,
                                   const struct capy_image_limits *limits,
                                   struct capy_image_rgba32 *out);
int capy_image_detect_memory(const uint8_t *data, size_t size,
                             enum capy_image_format *out_format);
int capy_image_decode_memory(const uint8_t *data, size_t size,
                             const struct capy_image_allocator *allocator,
                             const struct capy_image_inflater *inflater,
                             const struct capy_image_limits *limits,
                             struct capy_image_rgba32 *out);
int capy_image_query_memory(const uint8_t *data, size_t size,
                            struct capy_image_metadata *out_meta);
void capy_image_rgba32_free(struct capy_image_rgba32 *image);

#endif

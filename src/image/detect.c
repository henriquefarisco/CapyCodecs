#include "capy_image.h"

static const uint8_t capy_detect_png_sig[8] = {
    0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};

static int capy_detect_prefix_equal(const uint8_t *data, const uint8_t *prefix,
                                    size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] != prefix[i]) {
      return 0;
    }
  }
  return 1;
}

static void capy_detect_rgba32_reset(struct capy_image_rgba32 *image) {
  if (!image) {
    return;
  }
  image->width = 0;
  image->height = 0;
  image->pixels = 0;
  image->allocator.alloc = 0;
  image->allocator.free = 0;
  image->allocator.user_data = 0;
}

int capy_image_detect_memory(const uint8_t *data, size_t size,
                             enum capy_image_format *out_format) {
  if (!data || !out_format) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  *out_format = CAPY_IMAGE_FORMAT_UNKNOWN;
  if (size >= 2u && data[0] == 0x42u && data[1] == 0x4Du) {
    *out_format = CAPY_IMAGE_FORMAT_BMP;
    return CAPY_IMAGE_OK;
  }
  if (size >= sizeof(capy_detect_png_sig) &&
      capy_detect_prefix_equal(data, capy_detect_png_sig,
                               sizeof(capy_detect_png_sig))) {
    *out_format = CAPY_IMAGE_FORMAT_PNG;
    return CAPY_IMAGE_OK;
  }
  if (size >= 2u && data[0] == 0xFFu && data[1] == 0xD8u) {
    *out_format = CAPY_IMAGE_FORMAT_JPEG;
    return CAPY_IMAGE_OK;
  }
  if (size >= 4u && data[0] == 0x71u && data[1] == 0x6Fu &&
      data[2] == 0x69u && data[3] == 0x66u) {
    *out_format = CAPY_IMAGE_FORMAT_QOI;
    return CAPY_IMAGE_OK;
  }
  return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
}

int capy_image_decode_memory(const uint8_t *data, size_t size,
                             const struct capy_image_allocator *allocator,
                             const struct capy_image_inflater *inflater,
                             const struct capy_image_limits *limits,
                             struct capy_image_rgba32 *out) {
  enum capy_image_format format = CAPY_IMAGE_FORMAT_UNKNOWN;
  int rc = capy_image_detect_memory(data, size, &format);
  if (rc != CAPY_IMAGE_OK) {
    capy_detect_rgba32_reset(out);
    return rc;
  }
  switch (format) {
    case CAPY_IMAGE_FORMAT_BMP:
      return capy_bmp_decode_memory_limited(data, size, allocator, limits, out);
    case CAPY_IMAGE_FORMAT_PNG:
      return capy_png_decode_memory_limited(data, size, allocator, inflater,
                                            limits, out);
    case CAPY_IMAGE_FORMAT_JPEG:
      return capy_jpeg_decode_memory_limited(data, size, allocator, limits,
                                             out);
    case CAPY_IMAGE_FORMAT_QOI:
      return capy_qoi_decode_memory_limited(data, size, allocator, limits, out);
    default:
      capy_detect_rgba32_reset(out);
      return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
}

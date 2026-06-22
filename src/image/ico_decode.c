#include "capy_image.h"

#include <string.h>

static uint16_t capy_ico_u16le(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t capy_ico_u32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static void capy_ico_rgba32_reset(struct capy_image_rgba32 *image) {
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

/* Decode a BMP sub-image of an ICO by synthesising a standalone BMP (a
 * 14-byte file header plus the embedded BITMAPINFOHEADER whose AND-mask
 * doubled height is halved) and delegating to the validated BMP decoder, so
 * the BMP pixel path stays a single source of truth. The trailing AND mask is
 * left in place and simply not read (the BMP decoder only reads the XOR rows).
 */
static int capy_ico_decode_bmp(const uint8_t *image_data, uint32_t image_size,
                               const struct capy_image_allocator *allocator,
                               const struct capy_image_limits *limits,
                               struct capy_image_rgba32 *out) {
  uint32_t raw_height;
  int32_t real_height;
  uint8_t *bmp;
  size_t bmp_size;
  int rc;
  if (image_size < 40u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  raw_height = capy_ico_u32le(image_data + 8u);
  if (raw_height == 0u || (raw_height & 1u) != 0u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  real_height = (int32_t)(raw_height / 2u);
  bmp_size = (size_t)14u + (size_t)image_size;
  bmp = (uint8_t *)allocator->alloc(bmp_size, allocator->user_data);
  if (!bmp) {
    return CAPY_IMAGE_ERR_OUT_OF_MEMORY;
  }
  bmp[0] = 0x42u;
  bmp[1] = 0x4Du;
  bmp[2] = (uint8_t)(bmp_size & 0xFFu);
  bmp[3] = (uint8_t)((bmp_size >> 8) & 0xFFu);
  bmp[4] = (uint8_t)((bmp_size >> 16) & 0xFFu);
  bmp[5] = (uint8_t)((bmp_size >> 24) & 0xFFu);
  bmp[6] = 0u;
  bmp[7] = 0u;
  bmp[8] = 0u;
  bmp[9] = 0u;
  bmp[10] = 54u;
  bmp[11] = 0u;
  bmp[12] = 0u;
  bmp[13] = 0u;
  memcpy(bmp + 14, image_data, (size_t)image_size);
  bmp[22] = (uint8_t)((uint32_t)real_height & 0xFFu);
  bmp[23] = (uint8_t)(((uint32_t)real_height >> 8) & 0xFFu);
  bmp[24] = (uint8_t)(((uint32_t)real_height >> 16) & 0xFFu);
  bmp[25] = (uint8_t)(((uint32_t)real_height >> 24) & 0xFFu);
  rc = capy_bmp_decode_memory_limited(bmp, bmp_size, allocator, limits, out);
  allocator->free(bmp, allocator->user_data);
  return rc;
}

int capy_ico_decode_memory_limited(const uint8_t *data, size_t size,
                                   const struct capy_image_allocator *allocator,
                                   const struct capy_image_inflater *inflater,
                                   const struct capy_image_limits *limits,
                                   struct capy_image_rgba32 *out) {
  uint16_t count;
  uint32_t i;
  uint32_t best_idx = 0u;
  uint32_t best_area = 0u;
  uint32_t width;
  uint32_t height;
  uint32_t image_offset;
  uint32_t image_size;
  const uint8_t *entry;
  const uint8_t *image_data;
  if (!out) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  capy_ico_rgba32_reset(out);
  if (!data || !allocator || !allocator->alloc || !allocator->free ||
      !inflater || !inflater->inflate) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  if (size < 6u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  count = capy_ico_u16le(data + 4u);
  if (count == 0u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  if (size < 6u + (size_t)count * 16u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  for (i = 0u; i < (uint32_t)count; ++i) {
    entry = data + 6u + (size_t)i * 16u;
    width = entry[0] == 0u ? 256u : (uint32_t)entry[0];
    height = entry[1] == 0u ? 256u : (uint32_t)entry[1];
    if (width * height >= best_area) {
      best_area = width * height;
      best_idx = i;
    }
  }
  entry = data + 6u + (size_t)best_idx * 16u;
  image_offset = capy_ico_u32le(entry + 12u);
  image_size = capy_ico_u32le(entry + 8u);
  if (image_size < 4u || image_offset < 6u + (uint32_t)count * 16u ||
      (size_t)image_offset > size ||
      (size_t)image_size > size - (size_t)image_offset) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  image_data = data + image_offset;
  if (image_data[0] == 0x89u && image_data[1] == 0x50u &&
      image_data[2] == 0x4Eu && image_data[3] == 0x47u) {
    return capy_png_decode_memory_limited(image_data, image_size, allocator,
                                          inflater, limits, out);
  }
  if (image_size >= 40u && capy_ico_u32le(image_data) == 40u) {
    return capy_ico_decode_bmp(image_data, image_size, allocator, limits, out);
  }
  return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
}

int capy_ico_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           const struct capy_image_inflater *inflater,
                           struct capy_image_rgba32 *out) {
  struct capy_image_limits limits;
  capy_image_default_limits(&limits);
  return capy_ico_decode_memory_limited(data, size, allocator, inflater,
                                        &limits, out);
}

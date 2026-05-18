#include "capy_image.h"

static uint16_t capy_bmp_u16le(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t capy_bmp_u32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t capy_bmp_i32le(const uint8_t *p) {
  return (int32_t)capy_bmp_u32le(p);
}

static void capy_image_rgba32_reset(struct capy_image_rgba32 *image) {
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

int capy_bmp_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           struct capy_image_rgba32 *out) {
  int32_t width;
  int32_t height;
  int bottom_up;
  uint32_t bpp;
  uint32_t row_size;
  uint32_t pixel_offset;
  size_t pixel_bytes;

  if (!out) {
    return -1;
  }
  capy_image_rgba32_reset(out);
  if (!data || !allocator || !allocator->alloc || !allocator->free ||
      size < 54u) {
    return -1;
  }

  if (capy_bmp_u16le(data) != 0x4D42u) {
    return -1;
  }
  if (capy_bmp_u32le(data + 14u) < 40u) {
    return -1;
  }
  if (capy_bmp_u16le(data + 26u) != 1u) {
    return -1;
  }
  bpp = capy_bmp_u16le(data + 28u);
  if (bpp != 24u && bpp != 32u) {
    return -1;
  }
  if (capy_bmp_u32le(data + 30u) != 0u) {
    return -1;
  }

  width = capy_bmp_i32le(data + 18u);
  height = capy_bmp_i32le(data + 22u);
  bottom_up = height > 0 ? 1 : 0;
  if (height == INT32_MIN) {
    return -1;
  }
  if (height < 0) {
    height = -height;
  }
  if (width <= 0 || height <= 0 || (uint32_t)width > CAPY_IMAGE_MAX_WIDTH ||
      (uint32_t)height > CAPY_IMAGE_MAX_HEIGHT) {
    return -1;
  }

  row_size = ((bpp * (uint32_t)width + 31u) / 32u) * 4u;
  pixel_offset = capy_bmp_u32le(data + 10u);
  if (pixel_offset < 54u || pixel_offset >= size) {
    return -1;
  }
  pixel_bytes = (size_t)(uint32_t)width * (size_t)(uint32_t)height *
                sizeof(uint32_t);
  if (pixel_bytes / sizeof(uint32_t) / (size_t)(uint32_t)width !=
      (size_t)(uint32_t)height) {
    return -1;
  }

  out->pixels = (uint32_t *)allocator->alloc(pixel_bytes, allocator->user_data);
  if (!out->pixels) {
    capy_image_rgba32_reset(out);
    return -1;
  }
  out->allocator = *allocator;

  for (int32_t y = 0; y < height; ++y) {
    int32_t src_y = bottom_up ? (height - 1 - y) : y;
    size_t row_offset = (size_t)pixel_offset + (size_t)((uint32_t)src_y) *
                                                (size_t)row_size;
    const uint8_t *row;
    if (row_offset > size || row_size > size - row_offset) {
      capy_image_rgba32_free(out);
      return -1;
    }
    row = data + row_offset;
    for (int32_t x = 0; x < width; ++x) {
      uint32_t off = (uint32_t)x * (bpp == 32u ? 4u : 3u);
      uint32_t pixel = 0xFF000000u | ((uint32_t)row[off + 2u] << 16) |
                       ((uint32_t)row[off + 1u] << 8) | (uint32_t)row[off];
      out->pixels[y * (uint32_t)width + x] = pixel;
    }
  }
  out->width = (uint32_t)width;
  out->height = (uint32_t)height;

  return 0;
}

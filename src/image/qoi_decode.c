#include "capy_image.h"

#define CAPY_QOI_HEADER_SIZE 14u
#define CAPY_QOI_PADDING_SIZE 8u

#define CAPY_QOI_OP_INDEX 0x00u
#define CAPY_QOI_OP_DIFF 0x40u
#define CAPY_QOI_OP_LUMA 0x80u
#define CAPY_QOI_OP_RUN 0xC0u
#define CAPY_QOI_OP_RGB 0xFEu
#define CAPY_QOI_OP_RGBA 0xFFu
#define CAPY_QOI_MASK_2 0xC0u

struct capy_qoi_px {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

static uint32_t capy_qoi_u32be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void capy_qoi_rgba32_reset(struct capy_image_rgba32 *image) {
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

static uint8_t capy_qoi_index_pos(struct capy_qoi_px px) {
  return (uint8_t)(((uint32_t)px.r * 3u + (uint32_t)px.g * 5u +
                    (uint32_t)px.b * 7u + (uint32_t)px.a * 11u) %
                   64u);
}

static const uint8_t capy_qoi_padding[CAPY_QOI_PADDING_SIZE] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};

int capy_qoi_decode_memory_limited(const uint8_t *data, size_t size,
                                   const struct capy_image_allocator *allocator,
                                   const struct capy_image_limits *limits,
                                   struct capy_image_rgba32 *out) {
  struct capy_image_limits eff;
  struct capy_qoi_px index[64];
  struct capy_qoi_px px;
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorspace;
  uint32_t *pixels;
  size_t pixel_count;
  size_t out_bytes;
  size_t pos;
  size_t op_end;
  size_t i;
  uint32_t run = 0u;

  if (!out) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  capy_qoi_rgba32_reset(out);
  if (!data || !allocator || !allocator->alloc || !allocator->free) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  if (limits) {
    eff = *limits;
  } else {
    capy_image_default_limits(&eff);
  }
  if (size < CAPY_QOI_HEADER_SIZE + CAPY_QOI_PADDING_SIZE) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  if (data[0] != 0x71u || data[1] != 0x6Fu || data[2] != 0x69u ||
      data[3] != 0x66u) {
    return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
  width = capy_qoi_u32be(data + 4u);
  height = capy_qoi_u32be(data + 8u);
  channels = data[12];
  colorspace = data[13];
  if (width == 0u || height == 0u || (channels != 3u && channels != 4u) ||
      colorspace > 1u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  {
    size_t k;
    for (k = 0u; k < CAPY_QOI_PADDING_SIZE; ++k) {
      if (data[size - CAPY_QOI_PADDING_SIZE + k] != capy_qoi_padding[k]) {
        return CAPY_IMAGE_ERR_CORRUPT_DATA;
      }
    }
  }
  if (width > eff.max_width || height > eff.max_height) {
    return CAPY_IMAGE_ERR_RESOURCE_LIMIT;
  }
  pixel_count = (size_t)width * (size_t)height;
  if (pixel_count / (size_t)width != (size_t)height) {
    return CAPY_IMAGE_ERR_RESOURCE_LIMIT;
  }
  out_bytes = pixel_count * sizeof(uint32_t);
  if (out_bytes / sizeof(uint32_t) != pixel_count ||
      out_bytes > eff.max_output_bytes) {
    return CAPY_IMAGE_ERR_RESOURCE_LIMIT;
  }
  pixels = (uint32_t *)allocator->alloc(out_bytes, allocator->user_data);
  if (!pixels) {
    return CAPY_IMAGE_ERR_OUT_OF_MEMORY;
  }

  for (i = 0u; i < 64u; ++i) {
    index[i].r = 0u;
    index[i].g = 0u;
    index[i].b = 0u;
    index[i].a = 0u;
  }
  px.r = 0u;
  px.g = 0u;
  px.b = 0u;
  px.a = 255u;
  pos = CAPY_QOI_HEADER_SIZE;
  op_end = size - CAPY_QOI_PADDING_SIZE;

  for (i = 0u; i < pixel_count; ++i) {
    if (run > 0u) {
      --run;
    } else {
      uint8_t b1;
      if (pos >= op_end) {
        allocator->free(pixels, allocator->user_data);
        capy_qoi_rgba32_reset(out);
        return CAPY_IMAGE_ERR_TRUNCATED_DATA;
      }
      b1 = data[pos++];
      if (b1 == CAPY_QOI_OP_RGB) {
        if (op_end - pos < 3u) {
          allocator->free(pixels, allocator->user_data);
          capy_qoi_rgba32_reset(out);
          return CAPY_IMAGE_ERR_TRUNCATED_DATA;
        }
        px.r = data[pos];
        px.g = data[pos + 1u];
        px.b = data[pos + 2u];
        pos += 3u;
      } else if (b1 == CAPY_QOI_OP_RGBA) {
        if (op_end - pos < 4u) {
          allocator->free(pixels, allocator->user_data);
          capy_qoi_rgba32_reset(out);
          return CAPY_IMAGE_ERR_TRUNCATED_DATA;
        }
        px.r = data[pos];
        px.g = data[pos + 1u];
        px.b = data[pos + 2u];
        px.a = data[pos + 3u];
        pos += 4u;
      } else if ((b1 & CAPY_QOI_MASK_2) == CAPY_QOI_OP_INDEX) {
        px = index[b1 & 0x3Fu];
      } else if ((b1 & CAPY_QOI_MASK_2) == CAPY_QOI_OP_DIFF) {
        int dr = (int)((b1 >> 4) & 0x03u) - 2;
        int dg = (int)((b1 >> 2) & 0x03u) - 2;
        int db = (int)(b1 & 0x03u) - 2;
        px.r = (uint8_t)((int)px.r + dr);
        px.g = (uint8_t)((int)px.g + dg);
        px.b = (uint8_t)((int)px.b + db);
      } else if ((b1 & CAPY_QOI_MASK_2) == CAPY_QOI_OP_LUMA) {
        uint8_t b2;
        int vg;
        if (op_end - pos < 1u) {
          allocator->free(pixels, allocator->user_data);
          capy_qoi_rgba32_reset(out);
          return CAPY_IMAGE_ERR_TRUNCATED_DATA;
        }
        b2 = data[pos++];
        vg = (int)(b1 & 0x3Fu) - 32;
        px.r = (uint8_t)((int)px.r + vg + (int)((b2 >> 4) & 0x0Fu) - 8);
        px.g = (uint8_t)((int)px.g + vg);
        px.b = (uint8_t)((int)px.b + vg + (int)(b2 & 0x0Fu) - 8);
      } else {
        run = (uint32_t)(b1 & 0x3Fu);
      }
      index[capy_qoi_index_pos(px)] = px;
    }
    {
      uint8_t a_out = (channels == 4u) ? px.a : 255u;
      pixels[i] = ((uint32_t)a_out << 24) | ((uint32_t)px.r << 16) |
                  ((uint32_t)px.g << 8) | (uint32_t)px.b;
    }
  }

  out->width = width;
  out->height = height;
  out->pixels = pixels;
  out->allocator = *allocator;
  return CAPY_IMAGE_OK;
}

int capy_qoi_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           struct capy_image_rgba32 *out) {
  struct capy_image_limits limits;
  capy_image_default_limits(&limits);
  return capy_qoi_decode_memory_limited(data, size, allocator, &limits, out);
}

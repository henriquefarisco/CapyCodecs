#include "capy_image.h"

/* Header-only metadata query: parse just enough of each container header to
   report format, dimensions and pixel characteristics without allocating or
   decoding. The acceptance checks mirror the per-codec decoders, so a query
   that returns CAPY_IMAGE_OK implies the header is decodable (a later decode
   may still fail on resource limits, a missing PNG inflater, allocation or
   corrupt entropy data). has_alpha describes the decoded ARGB32 output: it is
   1 only when the output may carry non-opaque alpha. */

static uint16_t capy_meta_u16le(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t capy_meta_u32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static int32_t capy_meta_i32le(const uint8_t *p) {
  return (int32_t)capy_meta_u32le(p);
}

static uint32_t capy_meta_u32be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int capy_meta_bmp(const uint8_t *data, size_t size,
                         struct capy_image_metadata *m) {
  int32_t width;
  int32_t height;
  uint32_t bpp;
  if (size < 54u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  if (capy_meta_u32le(data + 14u) < 40u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  if (capy_meta_u16le(data + 26u) != 1u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  bpp = capy_meta_u16le(data + 28u);
  if (bpp != 1u && bpp != 4u && bpp != 8u && bpp != 24u && bpp != 32u) {
    return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
  if (capy_meta_u32le(data + 30u) != 0u) {
    return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
  width = capy_meta_i32le(data + 18u);
  height = capy_meta_i32le(data + 22u);
  if (height == INT32_MIN) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  if (height < 0) {
    height = -height;
  }
  if (width <= 0 || height <= 0) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  m->format = CAPY_IMAGE_FORMAT_BMP;
  m->width = (uint32_t)width;
  m->height = (uint32_t)height;
  m->channels = (bpp == 32u) ? 4u : 3u;
  m->bits_per_channel = 8u;
  m->has_alpha = 0u;
  return CAPY_IMAGE_OK;
}

static int capy_meta_png(const uint8_t *data, size_t size,
                         struct capy_image_metadata *m) {
  uint32_t width;
  uint32_t height;
  uint8_t bit_depth;
  uint8_t color_type;
  uint32_t channels;
  if (size < 26u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  if (capy_meta_u32be(data + 8u) != 13u ||
      capy_meta_u32be(data + 12u) != 0x49484452u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  width = capy_meta_u32be(data + 16u);
  height = capy_meta_u32be(data + 20u);
  bit_depth = data[24];
  color_type = data[25];
  if (width == 0u || height == 0u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  switch (color_type) {
    case 0u:
      channels = 1u;
      break;
    case 2u:
      channels = 3u;
      break;
    case 4u:
      channels = 2u;
      break;
    case 6u:
      channels = 4u;
      break;
    default:
      return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
  if (bit_depth != 8u) {
    return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
  m->format = CAPY_IMAGE_FORMAT_PNG;
  m->width = width;
  m->height = height;
  m->channels = channels;
  m->bits_per_channel = bit_depth;
  m->has_alpha = (color_type == 4u || color_type == 6u) ? 1u : 0u;
  return CAPY_IMAGE_OK;
}

static int capy_meta_jpeg(const uint8_t *data, size_t size,
                          struct capy_image_metadata *m) {
  size_t pos = 2u;
  while (pos + 1u < size) {
    uint8_t marker;
    int seg_len;
    if (data[pos] != 0xFFu) {
      ++pos;
      continue;
    }
    while (pos < size && data[pos] == 0xFFu) {
      ++pos;
    }
    if (pos >= size) {
      break;
    }
    marker = data[pos++];
    if (marker == 0xD8u || marker == 0x01u ||
        (marker >= 0xD0u && marker <= 0xD7u)) {
      continue;
    }
    if (marker == 0xD9u) {
      return CAPY_IMAGE_ERR_CORRUPT_DATA;
    }
    if (pos + 2u > size) {
      return CAPY_IMAGE_ERR_TRUNCATED_DATA;
    }
    seg_len = ((int)data[pos] << 8) | (int)data[pos + 1u];
    if (seg_len < 2) {
      return CAPY_IMAGE_ERR_CORRUPT_DATA;
    }
    if ((size_t)seg_len > size - pos) {
      return CAPY_IMAGE_ERR_TRUNCATED_DATA;
    }
    if (marker >= 0xC0u && marker <= 0xCFu && marker != 0xC4u &&
        marker != 0xC8u && marker != 0xCCu) {
      if (marker != 0xC0u) {
        return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
      }
      if (seg_len < 8) {
        return CAPY_IMAGE_ERR_CORRUPT_DATA;
      }
      {
        uint8_t precision = data[pos + 2u];
        uint32_t height = (uint32_t)(((uint32_t)data[pos + 3u] << 8) |
                                     (uint32_t)data[pos + 4u]);
        uint32_t width = (uint32_t)(((uint32_t)data[pos + 5u] << 8) |
                                    (uint32_t)data[pos + 6u]);
        uint8_t ncomp = data[pos + 7u];
        if (precision != 8u || (ncomp != 1u && ncomp != 3u) || width == 0u ||
            height == 0u) {
          return CAPY_IMAGE_ERR_CORRUPT_DATA;
        }
        m->format = CAPY_IMAGE_FORMAT_JPEG;
        m->width = width;
        m->height = height;
        m->channels = ncomp;
        m->bits_per_channel = 8u;
        m->has_alpha = 0u;
        return CAPY_IMAGE_OK;
      }
    }
    if (marker == 0xDAu) {
      return CAPY_IMAGE_ERR_CORRUPT_DATA;
    }
    pos += (size_t)seg_len;
  }
  return CAPY_IMAGE_ERR_TRUNCATED_DATA;
}

static int capy_meta_qoi(const uint8_t *data, size_t size,
                         struct capy_image_metadata *m) {
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorspace;
  if (size < 14u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  width = capy_meta_u32be(data + 4u);
  height = capy_meta_u32be(data + 8u);
  channels = data[12];
  colorspace = data[13];
  if (width == 0u || height == 0u || (channels != 3u && channels != 4u) ||
      colorspace > 1u) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  m->format = CAPY_IMAGE_FORMAT_QOI;
  m->width = width;
  m->height = height;
  m->channels = channels;
  m->bits_per_channel = 8u;
  m->has_alpha = (channels == 4u) ? 1u : 0u;
  return CAPY_IMAGE_OK;
}

static int capy_meta_ico(const uint8_t *data, size_t size,
                         struct capy_image_metadata *m) {
  uint16_t count;
  uint16_t bpp;
  uint32_t i;
  uint32_t best_idx = 0u;
  uint32_t best_area = 0u;
  uint32_t width;
  uint32_t height;
  uint32_t image_offset;
  uint32_t image_size;
  const uint8_t *entry;
  const uint8_t *image_data;
  int rc;
  if (size < 6u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  count = capy_meta_u16le(data + 4u);
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
  image_offset = capy_meta_u32le(entry + 12u);
  image_size = capy_meta_u32le(entry + 8u);
  if (image_size < 4u || image_offset < 6u + (uint32_t)count * 16u ||
      (size_t)image_offset > size ||
      (size_t)image_size > size - (size_t)image_offset) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  image_data = data + image_offset;
  if (image_data[0] == 0x89u && image_data[1] == 0x50u &&
      image_data[2] == 0x4Eu && image_data[3] == 0x47u) {
    rc = capy_meta_png(image_data, image_size, m);
    if (rc != CAPY_IMAGE_OK) {
      return rc;
    }
    m->format = CAPY_IMAGE_FORMAT_ICO;
    return CAPY_IMAGE_OK;
  }
  if (image_size >= 40u && capy_meta_u32le(image_data) == 40u) {
    bpp = capy_meta_u16le(image_data + 14u);
    width = capy_meta_u32le(image_data + 4u);
    height = capy_meta_u32le(image_data + 8u) / 2u;
    if (width == 0u || height == 0u) {
      return CAPY_IMAGE_ERR_CORRUPT_DATA;
    }
    m->format = CAPY_IMAGE_FORMAT_ICO;
    m->width = width;
    m->height = height;
    m->channels = (bpp == 32u) ? 4u : 3u;
    m->bits_per_channel = 8u;
    m->has_alpha = (bpp == 32u) ? 1u : 0u;
    return CAPY_IMAGE_OK;
  }
  return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
}

int capy_image_query_memory(const uint8_t *data, size_t size,
                            struct capy_image_metadata *out_meta) {
  enum capy_image_format format = CAPY_IMAGE_FORMAT_UNKNOWN;
  int rc;
  if (!data || !out_meta) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  out_meta->format = CAPY_IMAGE_FORMAT_UNKNOWN;
  out_meta->width = 0u;
  out_meta->height = 0u;
  out_meta->channels = 0u;
  out_meta->bits_per_channel = 0u;
  out_meta->has_alpha = 0u;
  rc = capy_image_detect_memory(data, size, &format);
  if (rc != CAPY_IMAGE_OK) {
    return rc;
  }
  switch (format) {
    case CAPY_IMAGE_FORMAT_BMP:
      return capy_meta_bmp(data, size, out_meta);
    case CAPY_IMAGE_FORMAT_PNG:
      return capy_meta_png(data, size, out_meta);
    case CAPY_IMAGE_FORMAT_JPEG:
      return capy_meta_jpeg(data, size, out_meta);
    case CAPY_IMAGE_FORMAT_QOI:
      return capy_meta_qoi(data, size, out_meta);
    case CAPY_IMAGE_FORMAT_ICO:
      return capy_meta_ico(data, size, out_meta);
    default:
      return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
}

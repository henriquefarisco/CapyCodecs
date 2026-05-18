#include "capy_image.h"

#define CAPY_PNG_SIG_LEN 8u
#define CAPY_PNG_MAX_DIM 1024u
#define CAPY_PNG_MAX_PIXELS (CAPY_PNG_MAX_DIM * CAPY_PNG_MAX_DIM)
#define CAPY_PNG_MAX_RAW (CAPY_PNG_MAX_PIXELS * 5u)

static const uint8_t capy_png_sig[CAPY_PNG_SIG_LEN] = {
    0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};

static uint32_t capy_png_u32be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void capy_png_zero(void *ptr, size_t len) {
  uint8_t *p = (uint8_t *)ptr;
  while (len--) {
    *p++ = 0;
  }
}

static void capy_png_rgba32_reset(struct capy_image_rgba32 *image) {
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

static int capy_png_mem_equal(const uint8_t *a, const uint8_t *b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (a[i] != b[i]) {
      return 0;
    }
  }
  return 1;
}

static void capy_png_copy(uint8_t *dst, const uint8_t *src, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    dst[i] = src[i];
  }
}

static uint8_t capy_png_paeth(uint8_t a, uint8_t b, uint8_t c) {
  int p = (int)a + (int)b - (int)c;
  int pa = p - (int)a;
  int pb = p - (int)b;
  int pc = p - (int)c;
  if (pa < 0) pa = -pa;
  if (pb < 0) pb = -pb;
  if (pc < 0) pc = -pc;
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

static void capy_png_free_temp(const struct capy_image_allocator *allocator,
                               void *ptr) {
  if (allocator && allocator->free && ptr) {
    allocator->free(ptr, allocator->user_data);
  }
}

static int capy_png_idat_append(const struct capy_image_allocator *allocator,
                                uint8_t **buf, size_t *len, size_t *cap,
                                const uint8_t *chunk, size_t chunk_len) {
  if (!allocator || !allocator->alloc || !buf || !len || !cap || !chunk) {
    return -1;
  }
  if (*len + chunk_len < *len) {
    return -1;
  }
  if (*len + chunk_len > CAPY_PNG_MAX_RAW) {
    return -1;
  }
  if (*len + chunk_len > *cap) {
    size_t needed = *len + chunk_len;
    size_t new_cap = *cap ? *cap : 256u;
    uint8_t *next;
    while (new_cap < needed) {
      size_t grown = new_cap * 2u;
      if (grown <= new_cap) {
        new_cap = needed;
        break;
      }
      new_cap = grown;
    }
    if (new_cap > CAPY_PNG_MAX_RAW) {
      new_cap = CAPY_PNG_MAX_RAW;
    }
    if (new_cap < needed) {
      return -1;
    }
    next = (uint8_t *)allocator->alloc(new_cap, allocator->user_data);
    if (!next) {
      return -1;
    }
    if (*buf && *len) {
      capy_png_copy(next, *buf, *len);
    }
    capy_png_free_temp(allocator, *buf);
    *buf = next;
    *cap = new_cap;
  }
  capy_png_copy(*buf + *len, chunk, chunk_len);
  *len += chunk_len;
  return 0;
}

static int capy_png_channels(uint8_t color_type) {
  switch (color_type) {
    case 0u:
      return 1;
    case 2u:
      return 3;
    case 4u:
      return 2;
    case 6u:
      return 4;
    default:
      return 0;
  }
}

static int capy_png_reconstruct_row(uint8_t *recon, const uint8_t *row,
                                    const uint8_t *prev, size_t row_bytes,
                                    int channels) {
  uint8_t filter;
  const uint8_t *in;
  if (!recon || !row || channels <= 0) {
    return -1;
  }
  filter = row[0];
  in = row + 1;
  if (filter > 4u) {
    return -1;
  }
  for (size_t i = 0; i < row_bytes; ++i) {
    uint8_t a = i >= (size_t)channels ? recon[i - (size_t)channels] : 0u;
    uint8_t b = prev ? prev[i] : 0u;
    uint8_t c = (prev && i >= (size_t)channels)
                    ? prev[i - (size_t)channels]
                    : 0u;
    switch (filter) {
      case 0u:
        recon[i] = in[i];
        break;
      case 1u:
        recon[i] = (uint8_t)(in[i] + a);
        break;
      case 2u:
        recon[i] = (uint8_t)(in[i] + b);
        break;
      case 3u:
        recon[i] = (uint8_t)(in[i] + ((uint8_t)(((uint16_t)a + b) >> 1)));
        break;
      case 4u:
        recon[i] = (uint8_t)(in[i] + capy_png_paeth(a, b, c));
        break;
      default:
        return -1;
    }
  }
  return 0;
}

static void capy_png_write_pixel(uint32_t *pixels, uint32_t index,
                                 const uint8_t *recon, uint32_t x,
                                 int channels) {
  uint8_t r = 0u;
  uint8_t g = 0u;
  uint8_t b = 0u;
  uint8_t a = 0xFFu;
  if (channels == 1) {
    r = recon[x];
    g = recon[x];
    b = recon[x];
  } else if (channels == 2) {
    r = recon[x * 2u];
    g = recon[x * 2u];
    b = recon[x * 2u];
    a = recon[x * 2u + 1u];
  } else if (channels == 3) {
    r = recon[x * 3u];
    g = recon[x * 3u + 1u];
    b = recon[x * 3u + 2u];
  } else if (channels == 4) {
    r = recon[x * 4u];
    g = recon[x * 4u + 1u];
    b = recon[x * 4u + 2u];
    a = recon[x * 4u + 3u];
  }
  pixels[index] = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                  ((uint32_t)g << 8) | (uint32_t)b;
}

int capy_png_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           const struct capy_image_inflater *inflater,
                           struct capy_image_rgba32 *out) {
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint8_t bit_depth = 0u;
  uint8_t color_type = 0u;
  uint8_t compression_method = 0u;
  uint8_t filter_method = 0u;
  uint8_t interlace = 0u;
  int channels = 0;
  uint8_t *idat = 0;
  uint8_t *raw = 0;
  uint8_t *recon = 0;
  uint8_t *prev_recon = 0;
  uint32_t *pixels = 0;
  size_t idat_len = 0u;
  size_t idat_cap = 0u;
  size_t pos = 0u;
  uint8_t ihdr_seen = 0u;
  uint8_t idat_seen = 0u;
  int ok = 0;

  if (!out) {
    return -1;
  }
  capy_png_rgba32_reset(out);
  if (!data || !allocator || !allocator->alloc || !allocator->free ||
      !inflater || !inflater->inflate || size < CAPY_PNG_SIG_LEN) {
    return -1;
  }
  if (!capy_png_mem_equal(data, capy_png_sig, CAPY_PNG_SIG_LEN)) {
    return -1;
  }
  pos = CAPY_PNG_SIG_LEN;

  while (pos + 8u <= size) {
    uint32_t chunk_len = capy_png_u32be(data + pos);
    uint32_t chunk_type;
    const uint8_t *chunk_data;
    pos += 4u;
    chunk_type = capy_png_u32be(data + pos);
    pos += 4u;
    if ((size_t)chunk_len > size - pos || 4u > size - pos - chunk_len) {
      goto done;
    }
    chunk_data = data + pos;
    if (chunk_type == 0x49484452u && chunk_len == 13u) {
      if (ihdr_seen || idat_seen) {
        goto done;
      }
      width = capy_png_u32be(chunk_data);
      height = capy_png_u32be(chunk_data + 4u);
      bit_depth = chunk_data[8];
      color_type = chunk_data[9];
      compression_method = chunk_data[10];
      filter_method = chunk_data[11];
      interlace = chunk_data[12];
      ihdr_seen = 1u;
    } else if (chunk_type == 0x49444154u) {
      if (!ihdr_seen) {
        goto done;
      }
      if (capy_png_idat_append(allocator, &idat, &idat_len, &idat_cap,
                               chunk_data, chunk_len) != 0) {
        goto done;
      }
      idat_seen = 1u;
    } else if (chunk_type == 0x49454E44u) {
      break;
    } else if (chunk_type == 0x504C5445u) {
      if (!ihdr_seen || idat_seen) {
        goto done;
      }
    } else if ((chunk_type & 0x20000000u) == 0u) {
      goto done;
    }
    pos += (size_t)chunk_len + 4u;
  }

  channels = capy_png_channels(color_type);
  if (width == 0u || height == 0u || width > CAPY_PNG_MAX_DIM ||
      height > CAPY_PNG_MAX_DIM || bit_depth != 8u ||
      compression_method != 0u || filter_method != 0u || interlace != 0u ||
      channels == 0 || idat_len == 0u || !ihdr_seen) {
    goto done;
  }

  {
    size_t row_bytes = (size_t)width * (size_t)channels;
    size_t stride = row_bytes + 1u;
    size_t raw_size = stride * (size_t)height;
    size_t inflated = raw_size;
    if (row_bytes / (size_t)channels != (size_t)width ||
        stride <= row_bytes || raw_size / stride != (size_t)height ||
        raw_size > CAPY_PNG_MAX_RAW) {
      goto done;
    }
    raw = (uint8_t *)allocator->alloc(raw_size, allocator->user_data);
    recon = (uint8_t *)allocator->alloc(row_bytes, allocator->user_data);
    prev_recon = (uint8_t *)allocator->alloc(row_bytes, allocator->user_data);
    pixels = (uint32_t *)allocator->alloc(
        (size_t)width * (size_t)height * sizeof(uint32_t),
        allocator->user_data);
    if (!raw || !recon || !prev_recon || !pixels) {
      goto done;
    }
    capy_png_zero(raw, raw_size);
    capy_png_zero(prev_recon, row_bytes);
    if (inflater->inflate(raw, &inflated, idat, idat_len,
                          inflater->user_data) != 0 || inflated != raw_size) {
      goto done;
    }
    for (uint32_t y = 0u; y < height; ++y) {
      const uint8_t *row = raw + (size_t)y * stride;
      const uint8_t *prev = y > 0u ? prev_recon : 0;
      if (capy_png_reconstruct_row(recon, row, prev, row_bytes, channels) != 0) {
        goto done;
      }
      for (uint32_t x = 0u; x < width; ++x) {
        capy_png_write_pixel(pixels, y * width + x, recon, x, channels);
      }
      capy_png_copy(prev_recon, recon, row_bytes);
    }
  }

  out->width = width;
  out->height = height;
  out->pixels = pixels;
  out->allocator = *allocator;
  pixels = 0;
  ok = 1;

done:
  capy_png_free_temp(allocator, idat);
  capy_png_free_temp(allocator, raw);
  capy_png_free_temp(allocator, recon);
  capy_png_free_temp(allocator, prev_recon);
  capy_png_free_temp(allocator, pixels);
  if (!ok) {
    capy_png_rgba32_reset(out);
  }
  return ok ? 0 : -1;
}

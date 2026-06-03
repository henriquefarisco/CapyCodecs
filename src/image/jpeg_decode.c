#include "capy_image.h"

#define CAPY_JPEG_MAX_COMPS 3

#define CAPY_JPEG_SOI 0xD8
#define CAPY_JPEG_EOI 0xD9
#define CAPY_JPEG_SOF0 0xC0
#define CAPY_JPEG_SOF2 0xC2
#define CAPY_JPEG_DHT 0xC4
#define CAPY_JPEG_DQT 0xDB
#define CAPY_JPEG_SOS 0xDA
#define CAPY_JPEG_DRI 0xDD
#define CAPY_JPEG_RST0 0xD0
#define CAPY_JPEG_RST7 0xD7
#define CAPY_JPEG_APP0 0xE0
#define CAPY_JPEG_APP15 0xEF
#define CAPY_JPEG_COM 0xFE

static const uint8_t capy_jpeg_zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

static const int16_t capy_jpeg_cos[8][8] = {
    { 128, 128, 128, 128, 128, 128, 128, 128 },
    { 126, 106,  71,  25, -25, -71,-106,-126 },
    { 119,  49, -49,-119,-119, -49,  49, 119 },
    { 106, -25,-126, -71,  71, 126,  25,-106 },
    {  91, -91, -91,  91,  91, -91, -91,  91 },
    {  71,-126,  25, 106,-106, -25, 126, -71 },
    {  49,-119, 119, -49, -49, 119,-119,  49 },
    {  25, -71, 106,-126, 126,-106,  71, -25 },
};

#define CAPY_JPEG_C0 91
#define CAPY_JPEG_C1 128

struct capy_jpeg_huff {
  uint8_t bits[17];
  uint8_t vals[256];
  int maxcode[18];
  int delta[17];
  int built;
};

struct capy_jpeg_comp {
  uint8_t id;
  uint8_t h_samp;
  uint8_t v_samp;
  uint8_t qtable_idx;
  uint8_t dc_huff_idx;
  uint8_t ac_huff_idx;
  int dc_pred;
};

struct capy_jpeg_ctx {
  const uint8_t *data;
  size_t len;
  size_t pos;
  const struct capy_image_limits *limits;
  uint32_t width;
  uint32_t height;
  uint8_t precision;
  uint8_t ncomp;
  int16_t qtable[4][64];
  struct capy_jpeg_huff dc_huff[4];
  struct capy_jpeg_huff ac_huff[4];
  struct capy_jpeg_comp comp[CAPY_JPEG_MAX_COMPS];
  uint16_t restart_interval;
  uint32_t bits;
  int bits_left;
  int eof;
};

static void capy_jpeg_zero(void *ptr, size_t len) {
  uint8_t *p = (uint8_t *)ptr;
  while (len--) {
    *p++ = 0;
  }
}

static void capy_jpeg_rgba32_reset(struct capy_image_rgba32 *image) {
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

static void capy_jpeg_free_temp(const struct capy_image_allocator *allocator,
                                void *ptr) {
  if (allocator && allocator->free && ptr) {
    allocator->free(ptr, allocator->user_data);
  }
}

static int capy_jpeg_read_byte(struct capy_jpeg_ctx *ctx) {
  if (ctx->pos >= ctx->len) {
    ctx->eof = 1;
    return 0;
  }
  return ctx->data[ctx->pos++];
}

static uint16_t capy_jpeg_read_u16(struct capy_jpeg_ctx *ctx) {
  uint8_t hi = (uint8_t)capy_jpeg_read_byte(ctx);
  uint8_t lo = (uint8_t)capy_jpeg_read_byte(ctx);
  return (uint16_t)(((uint16_t)hi << 8) | lo);
}

static int capy_jpeg_segment_available(const struct capy_jpeg_ctx *ctx,
                                       int seg_len) {
  if (!ctx || seg_len < 2) {
    return 0;
  }
  return (size_t)(seg_len - 2) <= ctx->len - ctx->pos;
}

static void capy_jpeg_build_huff(struct capy_jpeg_huff *h) {
  int code = 0;
  int val_idx = 0;
  for (int l = 1; l <= 16; ++l) {
    if (h->bits[l] == 0) {
      h->maxcode[l] = -1;
      h->delta[l] = 0;
    } else {
      h->delta[l] = val_idx - code;
      h->maxcode[l] = code + h->bits[l] - 1;
      val_idx += h->bits[l];
      code += h->bits[l];
    }
    code <<= 1;
  }
  h->maxcode[17] = 0xFFFFFF;
  h->built = 1;
}

static void capy_jpeg_refill_bits(struct capy_jpeg_ctx *ctx) {
  while (ctx->bits_left <= 24) {
    if (ctx->pos >= ctx->len) {
      ctx->eof = 1;
      break;
    }
    uint8_t b = ctx->data[ctx->pos++];
    if (b == 0xFFu) {
      uint8_t b2;
      if (ctx->pos >= ctx->len) {
        ctx->eof = 1;
        break;
      }
      b2 = ctx->data[ctx->pos];
      if (b2 == 0x00u) {
        ++ctx->pos;
      } else if (b2 >= CAPY_JPEG_RST0 && b2 <= CAPY_JPEG_RST7) {
        ++ctx->pos;
        continue;
      } else {
        --ctx->pos;
        ctx->eof = 1;
        break;
      }
    }
    ctx->bits = (ctx->bits << 8) | b;
    ctx->bits_left += 8;
  }
}

static int capy_jpeg_get_bits(struct capy_jpeg_ctx *ctx, int n) {
  if (n == 0) {
    return 0;
  }
  capy_jpeg_refill_bits(ctx);
  if (ctx->bits_left < n) {
    return 0;
  }
  ctx->bits_left -= n;
  return (int)((ctx->bits >> ctx->bits_left) & ((1u << n) - 1u));
}

static int capy_jpeg_huff_decode(struct capy_jpeg_ctx *ctx,
                                 struct capy_jpeg_huff *h) {
  int code = 0;
  if (!h->built) {
    return -1;
  }
  for (int l = 1; l <= 16; ++l) {
    code = (code << 1) | capy_jpeg_get_bits(ctx, 1);
    if (code <= h->maxcode[l]) {
      return h->vals[code + h->delta[l]];
    }
  }
  return -1;
}

static int capy_jpeg_receive_extend(struct capy_jpeg_ctx *ctx, int n) {
  int v;
  if (n == 0) {
    return 0;
  }
  v = capy_jpeg_get_bits(ctx, n);
  if (v < (1 << (n - 1))) {
    v -= (1 << n) - 1;
  }
  return v;
}

static void capy_jpeg_idct(int16_t *block, uint8_t *out, int stride) {
  int32_t tmp[64];
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int64_t sum = 0;
      for (int u = 0; u < 8; ++u) {
        int64_t cu_cos = capy_jpeg_cos[u][y];
        int64_t cu = (u == 0) ? CAPY_JPEG_C0 : CAPY_JPEG_C1;
        for (int v = 0; v < 8; ++v) {
          int64_t cv = (v == 0) ? CAPY_JPEG_C0 : CAPY_JPEG_C1;
          int64_t coeff = block[u * 8 + v];
          if (coeff) {
            sum += cu * cv * coeff * cu_cos * capy_jpeg_cos[v][x];
          }
        }
      }
      {
        int32_t val = (int32_t)((sum + (1LL << 29)) >> 30) + 128;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        tmp[y * 8 + x] = val;
      }
    }
  }
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      out[y * stride + x] = (uint8_t)tmp[y * 8 + x];
    }
  }
}

static int capy_jpeg_decode_block(struct capy_jpeg_ctx *ctx, int comp_idx,
                                  uint8_t *plane, int plane_w, int bx,
                                  int by) {
  struct capy_jpeg_comp *c = &ctx->comp[comp_idx];
  struct capy_jpeg_huff *dc_h = &ctx->dc_huff[c->dc_huff_idx];
  struct capy_jpeg_huff *ac_h = &ctx->ac_huff[c->ac_huff_idx];
  int16_t *qt = ctx->qtable[c->qtable_idx];
  int16_t block[64];
  int dc_sym;
  int k = 1;
  int px = bx * 8;
  int py = by * 8;
  uint8_t tmp[64];

  capy_jpeg_zero(block, sizeof(block));
  dc_sym = capy_jpeg_huff_decode(ctx, dc_h);
  if (dc_sym < 0 || dc_sym > 11) {
    return -1;
  }
  c->dc_pred += capy_jpeg_receive_extend(ctx, dc_sym);
  block[0] = (int16_t)(c->dc_pred * qt[0]);

  while (k < 64) {
    int ac_sym = capy_jpeg_huff_decode(ctx, ac_h);
    int run;
    int mag;
    if (ac_sym < 0) {
      return -1;
    }
    if (ac_sym == 0x00) {
      break;
    }
    run = (ac_sym >> 4) & 0x0F;
    mag = ac_sym & 0x0F;
    if (mag == 0) {
      if (run == 15) {
        k += 16;
        continue;
      }
      break;
    }
    k += run;
    if (k >= 64) {
      break;
    }
    {
      int val = capy_jpeg_receive_extend(ctx, mag);
      block[capy_jpeg_zigzag[k]] =
          (int16_t)(val * qt[capy_jpeg_zigzag[k]]);
    }
    ++k;
  }

  if (px >= (int)ctx->width || py >= (int)ctx->height) {
    return 0;
  }
  capy_jpeg_idct(block, tmp, 8);
  for (int row = 0; row < 8; ++row) {
    int ry = py + row;
    if (ry >= (int)ctx->height) {
      break;
    }
    for (int col = 0; col < 8; ++col) {
      int cx = px + col;
      if (cx >= plane_w) {
        break;
      }
      plane[ry * plane_w + cx] = tmp[row * 8 + col];
    }
  }
  return 0;
}

static int capy_jpeg_parse_dqt(struct capy_jpeg_ctx *ctx, int seg_len) {
  int remaining = seg_len - 2;
  while (remaining >= 65) {
    int info = capy_jpeg_read_byte(ctx);
    int prec = (info >> 4) & 0x0F;
    int tbl = info & 0x0F;
    --remaining;
    if (tbl >= 4) {
      return -1;
    }
    if (prec == 0) {
      if (remaining < 64) {
        return -1;
      }
      for (int i = 0; i < 64; ++i) {
        ctx->qtable[tbl][i] = (int16_t)capy_jpeg_read_byte(ctx);
      }
      remaining -= 64;
    } else if (prec == 1) {
      if (remaining < 128) {
        return -1;
      }
      for (int i = 0; i < 64; ++i) {
        uint16_t v = capy_jpeg_read_u16(ctx);
        ctx->qtable[tbl][i] = (int16_t)(v > 32767u ? 32767u : v);
      }
      remaining -= 128;
    } else {
      return -1;
    }
  }
  return remaining == 0 ? 0 : -1;
}

static int capy_jpeg_parse_dht(struct capy_jpeg_ctx *ctx, int seg_len) {
  int remaining = seg_len - 2;
  while (remaining > 0) {
    int info;
    int cls;
    int tbl;
    int total = 0;
    struct capy_jpeg_huff *h;
    if (remaining < 17) {
      return -1;
    }
    info = capy_jpeg_read_byte(ctx);
    cls = (info >> 4) & 0x01;
    tbl = info & 0x0F;
    --remaining;
    if (tbl >= 4) {
      return -1;
    }
    h = cls ? &ctx->ac_huff[tbl] : &ctx->dc_huff[tbl];
    capy_jpeg_zero(h, sizeof(*h));
    for (int i = 1; i <= 16; ++i) {
      h->bits[i] = (uint8_t)capy_jpeg_read_byte(ctx);
      total += h->bits[i];
      --remaining;
    }
    if (total > 256 || remaining < total) {
      return -1;
    }
    for (int i = 0; i < total; ++i) {
      h->vals[i] = (uint8_t)capy_jpeg_read_byte(ctx);
      --remaining;
    }
    capy_jpeg_build_huff(h);
  }
  return remaining == 0 ? 0 : -1;
}

static int capy_jpeg_parse_sof0(struct capy_jpeg_ctx *ctx, int seg_len) {
  ctx->precision = (uint8_t)capy_jpeg_read_byte(ctx);
  ctx->height = capy_jpeg_read_u16(ctx);
  ctx->width = capy_jpeg_read_u16(ctx);
  ctx->ncomp = (uint8_t)capy_jpeg_read_byte(ctx);
  if (ctx->width > ctx->limits->max_width ||
      ctx->height > ctx->limits->max_height) {
    return -2;
  }
  if (ctx->precision != 8 || ctx->width == 0 || ctx->height == 0 ||
      (ctx->ncomp != 1 && ctx->ncomp != 3) ||
      seg_len != 8 + (int)ctx->ncomp * 3) {
    return -1;
  }
  for (int i = 0; i < (int)ctx->ncomp; ++i) {
    uint8_t samp;
    ctx->comp[i].id = (uint8_t)capy_jpeg_read_byte(ctx);
    samp = (uint8_t)capy_jpeg_read_byte(ctx);
    ctx->comp[i].h_samp = (samp >> 4) & 0x0F;
    ctx->comp[i].v_samp = samp & 0x0F;
    ctx->comp[i].qtable_idx = (uint8_t)capy_jpeg_read_byte(ctx);
    ctx->comp[i].dc_pred = 0;
    if (ctx->comp[i].h_samp == 0 || ctx->comp[i].h_samp > 4 ||
        ctx->comp[i].v_samp == 0 || ctx->comp[i].v_samp > 4 ||
        ctx->comp[i].qtable_idx >= 4) {
      return -1;
    }
  }
  return 0;
}

static int capy_jpeg_parse_sos(struct capy_jpeg_ctx *ctx, uint8_t **planes,
                               int *plane_ws) {
  int seg_len = capy_jpeg_read_u16(ctx);
  int ns;
  int max_h = 0;
  int max_v = 0;
  int mcu_w;
  int mcu_h;
  int mcus_x;
  int mcus_y;
  int restart_count = 0;
  if (seg_len < 6) {
    return -1;
  }
  ns = capy_jpeg_read_byte(ctx);
  if (seg_len != 6 + ns * 2) {
    return -1;
  }
  if (ns != (int)ctx->ncomp) {
    return -1;
  }
  for (int i = 0; i < ns; ++i) {
    int id = capy_jpeg_read_byte(ctx);
    int tbl = capy_jpeg_read_byte(ctx);
    int found = 0;
    for (int c = 0; c < (int)ctx->ncomp; ++c) {
      if (ctx->comp[c].id == id) {
        ctx->comp[c].dc_huff_idx = (uint8_t)((tbl >> 4) & 0x0F);
        ctx->comp[c].ac_huff_idx = (uint8_t)(tbl & 0x0F);
        if (ctx->comp[c].dc_huff_idx >= 4 ||
            ctx->comp[c].ac_huff_idx >= 4) {
          return -1;
        }
        found = 1;
        break;
      }
    }
    if (!found) {
      return -1;
    }
  }
  capy_jpeg_read_byte(ctx);
  capy_jpeg_read_byte(ctx);
  capy_jpeg_read_byte(ctx);
  for (int c = 0; c < (int)ctx->ncomp; ++c) {
    ctx->comp[c].dc_pred = 0;
  }
  ctx->bits = 0;
  ctx->bits_left = 0;
  for (int c = 0; c < (int)ctx->ncomp; ++c) {
    if (ctx->comp[c].h_samp > max_h) max_h = ctx->comp[c].h_samp;
    if (ctx->comp[c].v_samp > max_v) max_v = ctx->comp[c].v_samp;
  }
  if (max_h == 0 || max_v == 0) {
    return -1;
  }
  mcu_w = max_h * 8;
  mcu_h = max_v * 8;
  mcus_x = ((int)ctx->width + mcu_w - 1) / mcu_w;
  mcus_y = ((int)ctx->height + mcu_h - 1) / mcu_h;
  for (int my = 0; my < mcus_y; ++my) {
    for (int mx = 0; mx < mcus_x; ++mx) {
      if (ctx->restart_interval > 0 &&
          restart_count == (int)ctx->restart_interval) {
        ctx->bits = 0;
        ctx->bits_left = 0;
        while (ctx->pos + 1 < ctx->len) {
          if (ctx->data[ctx->pos] == 0xFFu &&
              ctx->data[ctx->pos + 1u] >= CAPY_JPEG_RST0 &&
              ctx->data[ctx->pos + 1u] <= CAPY_JPEG_RST7) {
            ctx->pos += 2u;
            break;
          }
          ++ctx->pos;
        }
        for (int c = 0; c < (int)ctx->ncomp; ++c) {
          ctx->comp[c].dc_pred = 0;
        }
        restart_count = 0;
      }
      for (int ci = 0; ci < (int)ctx->ncomp; ++ci) {
        struct capy_jpeg_comp *comp = &ctx->comp[ci];
        int hs = comp->h_samp;
        int vs = comp->v_samp;
        int pw = plane_ws[ci];
        for (int dv = 0; dv < vs; ++dv) {
          for (int dh = 0; dh < hs; ++dh) {
            int bx = mx * hs + dh;
            int by = my * vs + dv;
            if (capy_jpeg_decode_block(ctx, ci, planes[ci], pw, bx, by) != 0) {
              return -1;
            }
          }
        }
      }
      ++restart_count;
      if (ctx->eof) {
        return 0;
      }
    }
  }
  return 0;
}

static uint8_t capy_jpeg_clamp(int v) {
  if (v < 0) return 0u;
  if (v > 255) return 255u;
  return (uint8_t)v;
}

static uint32_t capy_jpeg_ycbcr_to_argb(uint8_t y_in, uint8_t cb_in,
                                        uint8_t cr_in) {
  int y = (int)y_in;
  int cb = (int)cb_in - 128;
  int cr = (int)cr_in - 128;
  uint8_t r = capy_jpeg_clamp(y + ((1436 * cr) >> 10));
  uint8_t g = capy_jpeg_clamp(y - ((352 * cb + 731 * cr) >> 10));
  uint8_t b = capy_jpeg_clamp(y + ((1815 * cb) >> 10));
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) |
         (uint32_t)b;
}

static int capy_jpeg_alloc_planes(struct capy_jpeg_ctx *ctx,
                                  const struct capy_image_allocator *allocator,
                                  uint8_t **planes, int *plane_ws) {
  int max_h = 0;
  int max_v = 0;
  for (int c = 0; c < (int)ctx->ncomp; ++c) {
    if (ctx->comp[c].h_samp > max_h) max_h = ctx->comp[c].h_samp;
    if (ctx->comp[c].v_samp > max_v) max_v = ctx->comp[c].v_samp;
  }
  if (max_h == 0 || max_v == 0) {
    return CAPY_IMAGE_ERR_CORRUPT_DATA;
  }
  for (int c = 0; c < (int)ctx->ncomp; ++c) {
    int mcu_w = ctx->comp[c].h_samp * 8;
    int mcu_h = ctx->comp[c].v_samp * 8;
    int scaled_w = ((int)ctx->width * ctx->comp[c].h_samp + max_h - 1) / max_h;
    int scaled_h = ((int)ctx->height * ctx->comp[c].v_samp + max_v - 1) / max_v;
    int pw = (scaled_w + mcu_w - 1) / mcu_w * mcu_w;
    int ph = (scaled_h + mcu_h - 1) / mcu_h * mcu_h;
    size_t bytes;
    if (pw <= 0 || ph <= 0) {
      return CAPY_IMAGE_ERR_CORRUPT_DATA;
    }
    bytes = (size_t)pw * (size_t)ph;
    if (bytes / (size_t)pw != (size_t)ph ||
        bytes > ctx->limits->max_temporary_bytes) {
      return CAPY_IMAGE_ERR_RESOURCE_LIMIT;
    }
    plane_ws[c] = pw;
    planes[c] = (uint8_t *)allocator->alloc(bytes, allocator->user_data);
    if (!planes[c]) {
      return CAPY_IMAGE_ERR_OUT_OF_MEMORY;
    }
    capy_jpeg_zero(planes[c], bytes);
  }
  return CAPY_IMAGE_OK;
}

static void capy_jpeg_assemble_grayscale(struct capy_jpeg_ctx *ctx,
                                         uint8_t **planes, int *plane_ws,
                                         uint32_t *pixels) {
  for (uint32_t py = 0; py < ctx->height; ++py) {
    for (uint32_t px = 0; px < ctx->width; ++px) {
      uint8_t y = planes[0][py * (uint32_t)plane_ws[0] + px];
      pixels[py * ctx->width + px] =
          0xFF000000u | ((uint32_t)y << 16) | ((uint32_t)y << 8) | y;
    }
  }
}

static void capy_jpeg_assemble_ycbcr(struct capy_jpeg_ctx *ctx,
                                     uint8_t **planes, int *plane_ws,
                                     uint32_t *pixels) {
  int max_h = 0;
  int max_v = 0;
  for (int c = 0; c < 3; ++c) {
    if (ctx->comp[c].h_samp > max_h) max_h = ctx->comp[c].h_samp;
    if (ctx->comp[c].v_samp > max_v) max_v = ctx->comp[c].v_samp;
  }
  for (uint32_t py = 0; py < ctx->height; ++py) {
    for (uint32_t px = 0; px < ctx->width; ++px) {
      uint8_t y = planes[0][py * (uint32_t)plane_ws[0] + px];
      uint32_t cb_x = px * (uint32_t)ctx->comp[1].h_samp / (uint32_t)max_h;
      uint32_t cb_y = py * (uint32_t)ctx->comp[1].v_samp / (uint32_t)max_v;
      uint32_t cr_x = px * (uint32_t)ctx->comp[2].h_samp / (uint32_t)max_h;
      uint32_t cr_y = py * (uint32_t)ctx->comp[2].v_samp / (uint32_t)max_v;
      uint8_t cb = planes[1][cb_y * (uint32_t)plane_ws[1] + cb_x];
      uint8_t cr = planes[2][cr_y * (uint32_t)plane_ws[2] + cr_x];
      pixels[py * ctx->width + px] = capy_jpeg_ycbcr_to_argb(y, cb, cr);
    }
  }
}

int capy_jpeg_decode_memory_limited(const uint8_t *data, size_t size,
                                    const struct capy_image_allocator *allocator,
                                    const struct capy_image_limits *limits,
                                    struct capy_image_rgba32 *out) {
  struct capy_jpeg_ctx ctx;
  struct capy_image_limits eff;
  uint8_t *planes[CAPY_JPEG_MAX_COMPS] = {0, 0, 0};
  int plane_ws[CAPY_JPEG_MAX_COMPS] = {0, 0, 0};
  uint32_t *pixels = 0;
  int got_sof = 0;
  int got_sos = 0;
  int ok = 0;
  int result = CAPY_IMAGE_ERR_CORRUPT_DATA;

  if (!out) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  capy_jpeg_rgba32_reset(out);
  if (!data || !allocator || !allocator->alloc || !allocator->free) {
    return CAPY_IMAGE_ERR_INVALID_ARGUMENT;
  }
  if (limits) {
    eff = *limits;
  } else {
    capy_image_default_limits(&eff);
  }
  if (size < 2u) {
    return CAPY_IMAGE_ERR_TRUNCATED_DATA;
  }
  capy_jpeg_zero(&ctx, sizeof(ctx));
  ctx.data = data;
  ctx.len = size;
  ctx.limits = &eff;
  if (data[0] != 0xFFu || data[1] != CAPY_JPEG_SOI) {
    return CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
  }
  ctx.pos = 2u;

  while (ctx.pos + 1u < ctx.len && !ctx.eof) {
    uint8_t marker;
    if (ctx.data[ctx.pos] != 0xFFu) {
      ++ctx.pos;
      continue;
    }
    while (ctx.pos < ctx.len && ctx.data[ctx.pos] == 0xFFu) {
      ++ctx.pos;
    }
    if (ctx.pos >= ctx.len) {
      break;
    }
    marker = ctx.data[ctx.pos++];
    if (marker == 0x00u || (marker >= CAPY_JPEG_RST0 && marker <= CAPY_JPEG_RST7)) {
      continue;
    }
    if (marker == CAPY_JPEG_EOI) {
      break;
    }
    if (marker == CAPY_JPEG_SOI) {
      continue;
    }
    if (marker == CAPY_JPEG_SOF0) {
      int seg_len = (int)capy_jpeg_read_u16(&ctx);
      if (got_sof) {
        result = CAPY_IMAGE_ERR_CORRUPT_DATA;
        goto done;
      }
      if (!capy_jpeg_segment_available(&ctx, seg_len)) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      {
        int sof_result = capy_jpeg_parse_sof0(&ctx, seg_len);
        if (sof_result == -2) {
          result = CAPY_IMAGE_ERR_RESOURCE_LIMIT;
          goto done;
        }
        if (sof_result != 0) {
          result = CAPY_IMAGE_ERR_CORRUPT_DATA;
          goto done;
        }
      }
      got_sof = 1;
      result = capy_jpeg_alloc_planes(&ctx, allocator, planes, plane_ws);
      if (result != CAPY_IMAGE_OK) {
        goto done;
      }
    } else if (marker == CAPY_JPEG_DQT) {
      int seg_len = (int)capy_jpeg_read_u16(&ctx);
      if (!capy_jpeg_segment_available(&ctx, seg_len)) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      if (capy_jpeg_parse_dqt(&ctx, seg_len) != 0) {
        result = CAPY_IMAGE_ERR_CORRUPT_DATA;
        goto done;
      }
    } else if (marker == CAPY_JPEG_DHT) {
      int seg_len = (int)capy_jpeg_read_u16(&ctx);
      if (!capy_jpeg_segment_available(&ctx, seg_len)) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      if (capy_jpeg_parse_dht(&ctx, seg_len) != 0) {
        result = CAPY_IMAGE_ERR_CORRUPT_DATA;
        goto done;
      }
    } else if (marker == CAPY_JPEG_DRI) {
      int seg_len = (int)capy_jpeg_read_u16(&ctx);
      if (!capy_jpeg_segment_available(&ctx, seg_len)) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      if (seg_len != 4) {
        result = CAPY_IMAGE_ERR_CORRUPT_DATA;
        goto done;
      }
      ctx.restart_interval = capy_jpeg_read_u16(&ctx);
    } else if (marker == CAPY_JPEG_SOS) {
      int seg_len;
      if (ctx.pos + 2u > ctx.len) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      seg_len = ((int)ctx.data[ctx.pos] << 8) | (int)ctx.data[ctx.pos + 1u];
      if (!got_sof || seg_len < 2) {
        result = CAPY_IMAGE_ERR_CORRUPT_DATA;
        goto done;
      }
      if ((size_t)seg_len > ctx.len - ctx.pos) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      if (capy_jpeg_parse_sos(&ctx, planes, plane_ws) != 0) {
        result = CAPY_IMAGE_ERR_CORRUPT_DATA;
        goto done;
      }
      got_sos = 1;
      break;
    } else if (marker == CAPY_JPEG_SOF2 ||
               (marker >= 0xC0u && marker <= 0xCFu)) {
      result = CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
      goto done;
    } else if ((marker >= CAPY_JPEG_APP0 && marker <= CAPY_JPEG_APP15) ||
               marker == CAPY_JPEG_COM) {
      int seg_len = (int)capy_jpeg_read_u16(&ctx);
      if (!capy_jpeg_segment_available(&ctx, seg_len)) {
        result = CAPY_IMAGE_ERR_TRUNCATED_DATA;
        goto done;
      }
      ctx.pos += (size_t)(seg_len - 2);
    } else {
      result = CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT;
      goto done;
    }
  }

  if (!got_sof || !got_sos || !planes[0]) {
    result = CAPY_IMAGE_ERR_CORRUPT_DATA;
    goto done;
  }
  {
    size_t out_bytes =
        (size_t)ctx.width * (size_t)ctx.height * sizeof(uint32_t);
    if (out_bytes / sizeof(uint32_t) / (size_t)ctx.width !=
            (size_t)ctx.height ||
        out_bytes > eff.max_output_bytes) {
      result = CAPY_IMAGE_ERR_RESOURCE_LIMIT;
      goto done;
    }
    pixels = (uint32_t *)allocator->alloc(out_bytes, allocator->user_data);
  }
  if (!pixels) {
    result = CAPY_IMAGE_ERR_OUT_OF_MEMORY;
    goto done;
  }
  if (ctx.ncomp == 1) {
    capy_jpeg_assemble_grayscale(&ctx, planes, plane_ws, pixels);
  } else {
    capy_jpeg_assemble_ycbcr(&ctx, planes, plane_ws, pixels);
  }
  out->width = ctx.width;
  out->height = ctx.height;
  out->pixels = pixels;
  out->allocator = *allocator;
  pixels = 0;
  ok = 1;

done:
  for (int c = 0; c < CAPY_JPEG_MAX_COMPS; ++c) {
    capy_jpeg_free_temp(allocator, planes[c]);
  }
  capy_jpeg_free_temp(allocator, pixels);
  if (!ok) {
    capy_jpeg_rgba32_reset(out);
  }
  return ok ? CAPY_IMAGE_OK : result;
}

int capy_jpeg_decode_memory(const uint8_t *data, size_t size,
                            const struct capy_image_allocator *allocator,
                            struct capy_image_rgba32 *out) {
  struct capy_image_limits limits;
  capy_image_default_limits(&limits);
  return capy_jpeg_decode_memory_limited(data, size, allocator, &limits, out);
}

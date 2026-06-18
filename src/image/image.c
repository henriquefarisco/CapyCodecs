#include "capy_image.h"

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

uint32_t capy_image_abi_version(void) { return CAPY_IMAGE_ABI_VERSION; }

uint32_t capy_image_codec_features(void) {
  return CAPY_IMAGE_FEATURE_BMP_DECODE | CAPY_IMAGE_FEATURE_PNG_DECODE |
         CAPY_IMAGE_FEATURE_JPEG_DECODE | CAPY_IMAGE_FEATURE_ARGB32_OUTPUT |
         CAPY_IMAGE_FEATURE_ALLOCATOR_INJECTION |
         CAPY_IMAGE_FEATURE_PNG_INFLATER_INJECTION |
         CAPY_IMAGE_FEATURE_PER_CALL_LIMITS | CAPY_IMAGE_FEATURE_DETECT |
         CAPY_IMAGE_FEATURE_GENERIC_DECODE | CAPY_IMAGE_FEATURE_METADATA |
         CAPY_IMAGE_FEATURE_QOI_DECODE | CAPY_IMAGE_FEATURE_STRERROR |
         CAPY_IMAGE_FEATURE_FORMAT_NAME;
}

/* Short ASCII name for a detected format. All enum values listed (no
   `default:`) so a future format trips -Wswitch; the trailing return covers
   any out-of-range integer cast so the result is never NULL. */
const char *capy_image_format_name(enum capy_image_format format) {
  switch (format) {
  case CAPY_IMAGE_FORMAT_UNKNOWN:
    return "unknown";
  case CAPY_IMAGE_FORMAT_BMP:
    return "BMP";
  case CAPY_IMAGE_FORMAT_PNG:
    return "PNG";
  case CAPY_IMAGE_FORMAT_JPEG:
    return "JPEG";
  case CAPY_IMAGE_FORMAT_QOI:
    return "QOI";
  }
  return "unknown";
}

/* Human-readable description for each capy_image_error code. All enum values
   are listed with no `default:` so a future code trips -Wswitch; the trailing
   return covers any out-of-range integer cast so the result is never NULL. */
const char *capy_image_strerror(enum capy_image_error err) {
  switch (err) {
  case CAPY_IMAGE_OK:
    return "no error";
  case CAPY_IMAGE_ERR_INVALID_ARGUMENT:
    return "invalid argument";
  case CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT:
    return "unsupported image format";
  case CAPY_IMAGE_ERR_CORRUPT_DATA:
    return "corrupt image data";
  case CAPY_IMAGE_ERR_TRUNCATED_DATA:
    return "truncated image data";
  case CAPY_IMAGE_ERR_OUT_OF_MEMORY:
    return "out of memory";
  case CAPY_IMAGE_ERR_RESOURCE_LIMIT:
    return "image exceeds resource limit";
  case CAPY_IMAGE_ERR_INFLATER_FAILED:
    return "image inflater failed";
  }
  return "unknown image error";
}

void capy_image_default_limits(struct capy_image_limits *limits) {
  if (!limits) {
    return;
  }
  limits->max_width = CAPY_IMAGE_MAX_WIDTH;
  limits->max_height = CAPY_IMAGE_MAX_HEIGHT;
  limits->max_output_bytes = (size_t)CAPY_IMAGE_MAX_WIDTH *
                             (size_t)CAPY_IMAGE_MAX_HEIGHT *
                             sizeof(uint32_t);
  /* Worst-case temporary buffer is a PNG raw scanline pass: one filter byte
     per row plus four bytes per pixel across the maximum image. This stays at
     or above max_output_bytes so callers that decode a max-size RGBA PNG are
     not rejected by the temporary budget. */
  limits->max_temporary_bytes =
      ((size_t)CAPY_IMAGE_MAX_WIDTH * sizeof(uint32_t) + 1u) *
      (size_t)CAPY_IMAGE_MAX_HEIGHT;
}

void capy_image_rgba32_free(struct capy_image_rgba32 *image) {
  if (!image) {
    return;
  }
  if (image->pixels && image->allocator.free) {
    image->allocator.free(image->pixels, image->allocator.user_data);
  }
  capy_image_rgba32_reset(image);
}

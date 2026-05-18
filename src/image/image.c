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

void capy_image_rgba32_free(struct capy_image_rgba32 *image) {
  if (!image) {
    return;
  }
  if (image->pixels && image->allocator.free) {
    image->allocator.free(image->pixels, image->allocator.user_data);
  }
  capy_image_rgba32_reset(image);
}

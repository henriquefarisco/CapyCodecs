#include "test_image_common.h"

void test_jpeg_invalid_inputs_fail_closed(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  uint8_t invalid_jpeg[2] = {0u, 0u};
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  TEST_EXPECT(capy_jpeg_decode_memory(invalid_jpeg, sizeof(invalid_jpeg),
                                      &allocator, &image) ==
              CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  TEST_EXPECT(image.width == 0u && image.height == 0u && image.pixels == 0);
}

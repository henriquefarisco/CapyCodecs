#include "test_image_common.h"

void test_bmp_invalid_inputs_fail_closed(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  TEST_EXPECT(capy_bmp_decode_memory(0, 0u, &allocator, &image) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  TEST_EXPECT(image.width == 0u && image.height == 0u && image.pixels == 0);
}

void test_bmp_decode_rgb_1x1(void) {
  static const uint8_t bmp[] = {
      0x42u, 0x4Du, 0x3Au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
      0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u,
      0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x33u, 0x22u,
      0x11u, 0x00u};
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_bmp_decode_memory(bmp, sizeof(bmp), &allocator, &image) ==
              CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 1u);
  TEST_EXPECT(image.height == 1u);
  TEST_EXPECT(image.pixels != 0);
  TEST_EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(heap.free_calls == 1);
}

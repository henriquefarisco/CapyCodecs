#include "test_image_common.h"

void test_png_invalid_inputs_fail_closed(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state inflater_state;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &inflater_state;
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  TEST_EXPECT(capy_png_decode_memory(0, 0u, &allocator, &inflater, &image) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  TEST_EXPECT(image.width == 0u && image.height == 0u && image.pixels == 0);
}

void test_png_decode_rgb_1x1_with_inflater(void) {
  static const uint8_t png[] = {
      0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au,
      0x00u, 0x00u, 0x00u, 0x0Du, 0x49u, 0x48u, 0x44u, 0x52u,
      0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
      0x08u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x49u, 0x44u, 0x41u,
      0x54u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x49u, 0x45u, 0x4Eu, 0x44u,
      0x00u, 0x00u, 0x00u, 0x00u};
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct test_inflater_state inflater_state;
  struct capy_image_inflater inflater;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &inflater_state;
  TEST_EXPECT(capy_png_decode_memory(png, sizeof(png), &allocator, &inflater,
                                     &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(inflater_state.calls == 1);
  TEST_EXPECT(image.width == 1u);
  TEST_EXPECT(image.height == 1u);
  TEST_EXPECT(image.pixels != 0);
  TEST_EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(heap.free_calls == 5);
}

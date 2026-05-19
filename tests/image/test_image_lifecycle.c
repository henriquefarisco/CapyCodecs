#include "test_image_common.h"

void test_free_resets_image(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  image.width = 1u;
  image.height = 1u;
  image.pixels = (uint32_t *)allocator.alloc(sizeof(uint32_t), allocator.user_data);
  image.allocator = allocator;
  TEST_EXPECT(image.pixels != 0);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(heap.free_calls == 1);
  TEST_EXPECT(image.width == 0u);
  TEST_EXPECT(image.height == 0u);
  TEST_EXPECT(image.pixels == 0);
  TEST_EXPECT(image.allocator.alloc == 0);
  TEST_EXPECT(image.allocator.free == 0);
}

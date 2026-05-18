#include "../../src/image/capy_image.h"

#include <stddef.h>
#include <stdint.h>

struct test_heap {
  uint8_t storage[8192];
  size_t used;
  int alloc_calls;
  int free_calls;
  int fail_after;
};

struct test_inflater_state {
  int calls;
};

static int test_failures;

#define TEST_EXPECT(expr)        \
  do {                           \
    if (!(expr)) {               \
      ++test_failures;           \
      return;                    \
    }                            \
  } while (0)

static void test_heap_reset(struct test_heap *heap) {
  if (!heap) {
    return;
  }
  for (size_t i = 0; i < sizeof(heap->storage); ++i) {
    heap->storage[i] = 0;
  }
  heap->used = 0;
  heap->alloc_calls = 0;
  heap->free_calls = 0;
  heap->fail_after = -1;
}

static void *test_alloc(size_t size, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  size_t aligned;
  void *ptr;
  if (!heap || size == 0u) {
    return 0;
  }
  if (heap->fail_after == 0) {
    return 0;
  }
  if (heap->fail_after > 0) {
    --heap->fail_after;
  }
  aligned = (size + 7u) & ~(size_t)7u;
  if (aligned < size || aligned > sizeof(heap->storage) - heap->used) {
    return 0;
  }
  ptr = heap->storage + heap->used;
  heap->used += aligned;
  ++heap->alloc_calls;
  return ptr;
}

static void test_free(void *ptr, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  if (!heap || !ptr) {
    return;
  }
  ++heap->free_calls;
}

static struct capy_image_allocator test_allocator(struct test_heap *heap) {
  struct capy_image_allocator allocator;
  allocator.alloc = test_alloc;
  allocator.free = test_free;
  allocator.user_data = heap;
  return allocator;
}

static int test_inflate_rgb_1x1(uint8_t *dest, size_t *dest_len,
                                const uint8_t *source, size_t source_len,
                                void *user_data) {
  struct test_inflater_state *state = (struct test_inflater_state *)user_data;
  (void)source;
  (void)source_len;
  if (!dest || !dest_len || *dest_len < 4u || !state) {
    return -1;
  }
  dest[0] = 0u;
  dest[1] = 0x11u;
  dest[2] = 0x22u;
  dest[3] = 0x33u;
  *dest_len = 4u;
  ++state->calls;
  return 0;
}

static void test_free_resets_image(void) {
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

static void test_invalid_inputs_fail_closed(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state inflater_state;
  struct capy_image_rgba32 image;
  uint8_t invalid_jpeg[2] = {0u, 0u};
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &inflater_state;
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  TEST_EXPECT(capy_bmp_decode_memory(0, 0u, &allocator, &image) != 0);
  TEST_EXPECT(image.width == 0u && image.height == 0u && image.pixels == 0);
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  TEST_EXPECT(capy_png_decode_memory(0, 0u, &allocator, &inflater, &image) != 0);
  TEST_EXPECT(image.width == 0u && image.height == 0u && image.pixels == 0);
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  TEST_EXPECT(capy_jpeg_decode_memory(invalid_jpeg, sizeof(invalid_jpeg),
                                      &allocator, &image) != 0);
  TEST_EXPECT(image.width == 0u && image.height == 0u && image.pixels == 0);
}

static void test_bmp_decode_rgb_1x1(void) {
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
  TEST_EXPECT(capy_bmp_decode_memory(bmp, sizeof(bmp), &allocator, &image) == 0);
  TEST_EXPECT(image.width == 1u);
  TEST_EXPECT(image.height == 1u);
  TEST_EXPECT(image.pixels != 0);
  TEST_EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(heap.free_calls == 1);
}

static void test_png_decode_rgb_1x1_with_inflater(void) {
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
                                     &image) == 0);
  TEST_EXPECT(inflater_state.calls == 1);
  TEST_EXPECT(image.width == 1u);
  TEST_EXPECT(image.height == 1u);
  TEST_EXPECT(image.pixels != 0);
  TEST_EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(heap.free_calls == 5);
}

int main(void) {
  test_free_resets_image();
  test_invalid_inputs_fail_closed();
  test_bmp_decode_rgb_1x1();
  test_png_decode_rgb_1x1_with_inflater();
  return test_failures == 0 ? 0 : 1;
}

#include "test_image_common.h"
#include "../fixtures/image/golden_image_fixtures.h"

static void test_poison_image(struct capy_image_rgba32 *image,
                              struct capy_image_allocator allocator) {
  image->width = 99u;
  image->height = 88u;
  image->pixels = (uint32_t *)1;
  image->allocator = allocator;
}

static void test_expect_reset(const struct capy_image_rgba32 *image) {
  TEST_EXPECT(image != 0);
  TEST_EXPECT(image->width == 0u);
  TEST_EXPECT(image->height == 0u);
  TEST_EXPECT(image->pixels == 0);
  TEST_EXPECT(image->allocator.alloc == 0);
  TEST_EXPECT(image->allocator.free == 0);
}

static int test_inflate_returns_error(uint8_t *dest, size_t *dest_len,
                                      const uint8_t *source,
                                      size_t source_len, void *user_data) {
  struct test_inflater_state *state = (struct test_inflater_state *)user_data;
  (void)dest;
  (void)dest_len;
  (void)source;
  (void)source_len;
  if (state) {
    ++state->calls;
  }
  return -1;
}

static int test_inflate_short_output(uint8_t *dest, size_t *dest_len,
                                     const uint8_t *source,
                                     size_t source_len, void *user_data) {
  struct test_inflater_state *state = (struct test_inflater_state *)user_data;
  (void)source;
  (void)source_len;
  if (!dest || !dest_len || *dest_len == 0u || !state) {
    return -1;
  }
  dest[0] = 0u;
  *dest_len = 1u;
  ++state->calls;
  return 0;
}

static void test_png_inflater_failure_case(capy_image_inflate_fn inflate) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state inflater_state;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = inflate;
  inflater.user_data = &inflater_state;
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_png_decode_memory(test_fixture_png_1x1_rgb,
                                     sizeof(test_fixture_png_1x1_rgb),
                                     &allocator, &inflater,
                                     &image) == CAPY_IMAGE_ERR_INFLATER_FAILED);
  TEST_EXPECT(inflater_state.calls == 1);
  test_expect_reset(&image);
  TEST_EXPECT(heap.alloc_calls == 5);
  TEST_EXPECT(heap.free_calls == heap.alloc_calls);
}

void test_png_inflater_failures(void) {
  test_png_inflater_failure_case(test_inflate_returns_error);
  test_png_inflater_failure_case(test_inflate_short_output);
}

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

static int test_inflate_rgba_1x1(uint8_t *dest, size_t *dest_len,
                                 const uint8_t *source, size_t source_len,
                                 void *user_data) {
  struct test_inflater_state *state = (struct test_inflater_state *)user_data;
  (void)source;
  (void)source_len;
  if (!dest || !dest_len || *dest_len < 5u || !state) {
    return -1;
  }
  dest[0] = 0u;
  dest[1] = 0x11u;
  dest[2] = 0x22u;
  dest[3] = 0x33u;
  dest[4] = 0x44u;
  *dest_len = 5u;
  ++state->calls;
  return 0;
}

static int test_inflate_grayscale_1x1(uint8_t *dest, size_t *dest_len,
                                      const uint8_t *source,
                                      size_t source_len, void *user_data) {
  struct test_inflater_state *state = (struct test_inflater_state *)user_data;
  (void)source;
  (void)source_len;
  if (!dest || !dest_len || *dest_len < 2u || !state) {
    return -1;
  }
  dest[0] = 0u;
  dest[1] = 0x55u;
  *dest_len = 2u;
  ++state->calls;
  return 0;
}

static size_t test_bmp_success_alloc_count(const uint8_t *data, size_t size) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  if (capy_bmp_decode_memory(data, size, &allocator, &image) !=
      CAPY_IMAGE_OK) {
    ++test_failures;
    return 0u;
  }
  capy_image_rgba32_free(&image);
  return (size_t)heap.alloc_calls;
}

static size_t test_png_success_alloc_count(const uint8_t *data, size_t size,
                                           capy_image_inflate_fn inflate) {
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
  if (capy_png_decode_memory(data, size, &allocator, &inflater, &image) !=
      CAPY_IMAGE_OK) {
    ++test_failures;
    return 0u;
  }
  capy_image_rgba32_free(&image);
  return (size_t)heap.alloc_calls;
}

static size_t test_jpeg_success_alloc_count(const uint8_t *data, size_t size) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  if (capy_jpeg_decode_memory(data, size, &allocator, &image) !=
      CAPY_IMAGE_OK) {
    ++test_failures;
    return 0u;
  }
  capy_image_rgba32_free(&image);
  return (size_t)heap.alloc_calls;
}

static void test_bmp_alloc_failure_matrix_case(const uint8_t *data,
                                               size_t size) {
  size_t alloc_count = test_bmp_success_alloc_count(data, size);
  TEST_EXPECT(alloc_count > 0u);
  for (size_t fail_index = 0u; fail_index < alloc_count; ++fail_index) {
    struct test_heap heap;
    struct capy_image_allocator allocator;
    struct capy_image_rgba32 image;
    test_heap_reset(&heap);
    heap.fail_after = (int)fail_index;
    allocator = test_allocator(&heap);
    test_poison_image(&image, allocator);
    TEST_EXPECT(capy_bmp_decode_memory(data, size, &allocator, &image) ==
                CAPY_IMAGE_ERR_OUT_OF_MEMORY);
    test_expect_reset(&image);
  }
}

static void test_png_alloc_failure_matrix_case(const uint8_t *data,
                                               size_t size,
                                               capy_image_inflate_fn inflate) {
  size_t alloc_count = test_png_success_alloc_count(data, size, inflate);
  TEST_EXPECT(alloc_count > 0u);
  for (size_t fail_index = 0u; fail_index < alloc_count; ++fail_index) {
    struct test_heap heap;
    struct capy_image_allocator allocator;
    struct capy_image_inflater inflater;
    struct test_inflater_state inflater_state;
    struct capy_image_rgba32 image;
    test_heap_reset(&heap);
    heap.fail_after = (int)fail_index;
    allocator = test_allocator(&heap);
    inflater_state.calls = 0;
    inflater.inflate = inflate;
    inflater.user_data = &inflater_state;
    test_poison_image(&image, allocator);
    TEST_EXPECT(capy_png_decode_memory(data, size, &allocator, &inflater,
                                       &image) == CAPY_IMAGE_ERR_OUT_OF_MEMORY);
    test_expect_reset(&image);
    TEST_EXPECT(heap.free_calls == heap.alloc_calls);
  }
}

static void test_jpeg_alloc_failure_matrix_case(const uint8_t *data,
                                                size_t size) {
  size_t alloc_count = test_jpeg_success_alloc_count(data, size);
  TEST_EXPECT(alloc_count > 0u);
  for (size_t fail_index = 0u; fail_index < alloc_count; ++fail_index) {
    struct test_heap heap;
    struct capy_image_allocator allocator;
    struct capy_image_rgba32 image;
    test_heap_reset(&heap);
    heap.fail_after = (int)fail_index;
    allocator = test_allocator(&heap);
    test_poison_image(&image, allocator);
    TEST_EXPECT(capy_jpeg_decode_memory(data, size, &allocator, &image) ==
                CAPY_IMAGE_ERR_OUT_OF_MEMORY);
    test_expect_reset(&image);
    TEST_EXPECT(heap.free_calls == heap.alloc_calls);
  }
}

void test_allocator_failure_matrix(void) {
  test_bmp_alloc_failure_matrix_case(test_fixture_bmp_1x1_24,
                                     sizeof(test_fixture_bmp_1x1_24));
  test_bmp_alloc_failure_matrix_case(test_fixture_bmp_2x2_24,
                                     sizeof(test_fixture_bmp_2x2_24));
  test_bmp_alloc_failure_matrix_case(test_fixture_bmp_1x1_32,
                                     sizeof(test_fixture_bmp_1x1_32));
  test_png_alloc_failure_matrix_case(test_fixture_png_1x1_rgb,
                                     sizeof(test_fixture_png_1x1_rgb),
                                     test_inflate_rgb_1x1);
  test_png_alloc_failure_matrix_case(test_fixture_png_1x1_rgba,
                                     sizeof(test_fixture_png_1x1_rgba),
                                     test_inflate_rgba_1x1);
  test_png_alloc_failure_matrix_case(test_fixture_png_1x1_grayscale,
                                     sizeof(test_fixture_png_1x1_grayscale),
                                     test_inflate_grayscale_1x1);
  test_jpeg_alloc_failure_matrix_case(test_fixture_jpeg_1x1_grayscale,
                                      sizeof(test_fixture_jpeg_1x1_grayscale));
  test_jpeg_alloc_failure_matrix_case(test_fixture_jpeg_1x1_rgb,
                                      sizeof(test_fixture_jpeg_1x1_rgb));
}

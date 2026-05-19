#include "test_image_common.h"
#include "../fixtures/image/negative_image_fixtures.h"

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

static void test_negative_bmp_case(const uint8_t *data, size_t size,
                                   int expected) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_bmp_decode_memory(data, size, &allocator, &image) == expected);
  test_expect_reset(&image);
}

static void test_negative_png_case(const uint8_t *data, size_t size,
                                   int expected) {
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
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_png_decode_memory(data, size, &allocator, &inflater,
                                     &image) == expected);
  test_expect_reset(&image);
}

static void test_negative_jpeg_case(const uint8_t *data, size_t size,
                                    int expected) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_jpeg_decode_memory(data, size, &allocator, &image) == expected);
  test_expect_reset(&image);
}

void test_negative_bmp_fixtures(void) {
  test_negative_bmp_case(test_fixture_bmp_truncated_header,
                         sizeof(test_fixture_bmp_truncated_header),
                         CAPY_IMAGE_ERR_TRUNCATED_DATA);
  test_negative_bmp_case(test_fixture_bmp_invalid_magic,
                         sizeof(test_fixture_bmp_invalid_magic),
                         CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_negative_bmp_case(test_fixture_bmp_unsupported_bpp,
                         sizeof(test_fixture_bmp_unsupported_bpp),
                         CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_negative_bmp_case(test_fixture_bmp_truncated_pixels,
                         sizeof(test_fixture_bmp_truncated_pixels),
                         CAPY_IMAGE_ERR_TRUNCATED_DATA);
}

void test_negative_png_fixtures(void) {
  test_negative_png_case(test_fixture_png_truncated_signature,
                         sizeof(test_fixture_png_truncated_signature),
                         CAPY_IMAGE_ERR_TRUNCATED_DATA);
  test_negative_png_case(test_fixture_png_invalid_signature,
                         sizeof(test_fixture_png_invalid_signature),
                         CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_negative_png_case(test_fixture_png_truncated_chunk,
                         sizeof(test_fixture_png_truncated_chunk),
                         CAPY_IMAGE_ERR_TRUNCATED_DATA);
  test_negative_png_case(test_fixture_png_unsupported_color,
                         sizeof(test_fixture_png_unsupported_color),
                         CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
}

void test_negative_jpeg_fixtures(void) {
  test_negative_jpeg_case(test_fixture_jpeg_truncated_soi,
                          sizeof(test_fixture_jpeg_truncated_soi),
                          CAPY_IMAGE_ERR_TRUNCATED_DATA);
  test_negative_jpeg_case(test_fixture_jpeg_invalid_magic,
                          sizeof(test_fixture_jpeg_invalid_magic),
                          CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_negative_jpeg_case(test_fixture_jpeg_unsupported_progressive,
                          sizeof(test_fixture_jpeg_unsupported_progressive),
                          CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_negative_jpeg_case(test_fixture_jpeg_truncated_app,
                          sizeof(test_fixture_jpeg_truncated_app),
                          CAPY_IMAGE_ERR_TRUNCATED_DATA);
}

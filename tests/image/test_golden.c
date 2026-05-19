#include "test_image_common.h"
#include "../fixtures/image/golden_image_fixtures.h"

static void test_expect_image_hash(struct capy_image_rgba32 *image,
                                   uint32_t width, uint32_t height,
                                   uint32_t hash) {
  TEST_EXPECT(image != 0);
  TEST_EXPECT(image->width == width);
  TEST_EXPECT(image->height == height);
  TEST_EXPECT(image->pixels != 0);
  TEST_EXPECT(test_hash_pixels_argb32(image->pixels,
                                      (size_t)width * (size_t)height) == hash);
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

void test_golden_bmp_fixtures(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_bmp_decode_memory(test_fixture_bmp_1x1_24,
                                     sizeof(test_fixture_bmp_1x1_24),
                                     &allocator, &image) == CAPY_IMAGE_OK);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_FF112233);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(capy_bmp_decode_memory(test_fixture_bmp_2x2_24,
                                     sizeof(test_fixture_bmp_2x2_24),
                                     &allocator, &image) == CAPY_IMAGE_OK);
  test_expect_image_hash(&image, 2u, 2u, TEST_FIXTURE_HASH_BMP_2X2_24);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(capy_bmp_decode_memory(test_fixture_bmp_1x1_32,
                                     sizeof(test_fixture_bmp_1x1_32),
                                     &allocator, &image) == CAPY_IMAGE_OK);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_FF223344);
  capy_image_rgba32_free(&image);
}

void test_golden_png_fixtures(void) {
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
  TEST_EXPECT(capy_png_decode_memory(test_fixture_png_1x1_rgb,
                                     sizeof(test_fixture_png_1x1_rgb),
                                     &allocator, &inflater,
                                     &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(inflater_state.calls == 1);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_FF112233);
  capy_image_rgba32_free(&image);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgba_1x1;
  TEST_EXPECT(capy_png_decode_memory(test_fixture_png_1x1_rgba,
                                     sizeof(test_fixture_png_1x1_rgba),
                                     &allocator, &inflater,
                                     &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(inflater_state.calls == 1);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_44112233);
  capy_image_rgba32_free(&image);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_grayscale_1x1;
  TEST_EXPECT(capy_png_decode_memory(test_fixture_png_1x1_grayscale,
                                     sizeof(test_fixture_png_1x1_grayscale),
                                     &allocator, &inflater,
                                     &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(inflater_state.calls == 1);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_FF555555);
  capy_image_rgba32_free(&image);
}

void test_golden_jpeg_fixtures(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_jpeg_decode_memory(test_fixture_jpeg_1x1_grayscale,
                                      sizeof(test_fixture_jpeg_1x1_grayscale),
                                      &allocator, &image) == CAPY_IMAGE_OK);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_FF808080);
  capy_image_rgba32_free(&image);
  TEST_EXPECT(capy_jpeg_decode_memory(test_fixture_jpeg_1x1_rgb,
                                      sizeof(test_fixture_jpeg_1x1_rgb),
                                      &allocator, &image) == CAPY_IMAGE_OK);
  test_expect_image_hash(&image, 1u, 1u, TEST_FIXTURE_HASH_ARGB_FF808080);
  capy_image_rgba32_free(&image);
}

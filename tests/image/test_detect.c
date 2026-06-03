#include "test_image_common.h"
#include "../fixtures/image/golden_image_fixtures.h"
#include "../fixtures/image/negative_image_fixtures.h"

static void test_expect_detect(const uint8_t *data, size_t size, int expected_rc,
                               enum capy_image_format expected_format) {
  enum capy_image_format format = CAPY_IMAGE_FORMAT_UNKNOWN;
  TEST_EXPECT(capy_image_detect_memory(data, size, &format) == expected_rc);
  TEST_EXPECT(format == expected_format);
}

static void test_poison_image(struct capy_image_rgba32 *image,
                              struct capy_image_allocator allocator) {
  image->width = 99u;
  image->height = 88u;
  image->pixels = (uint32_t *)1;
  image->allocator = allocator;
}

static void test_expect_reset(const struct capy_image_rgba32 *image) {
  TEST_EXPECT(image->width == 0u);
  TEST_EXPECT(image->height == 0u);
  TEST_EXPECT(image->pixels == 0);
  TEST_EXPECT(image->allocator.alloc == 0);
  TEST_EXPECT(image->allocator.free == 0);
}

void test_detect_formats(void) {
  enum capy_image_format format = CAPY_IMAGE_FORMAT_UNKNOWN;
  static const uint8_t short_bmp_prefix[1] = {0x42u};

  test_expect_detect(test_fixture_bmp_1x1_24, sizeof(test_fixture_bmp_1x1_24),
                     CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_BMP);
  test_expect_detect(test_fixture_png_1x1_rgb, sizeof(test_fixture_png_1x1_rgb),
                     CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_PNG);
  test_expect_detect(test_fixture_jpeg_1x1_rgb,
                     sizeof(test_fixture_jpeg_1x1_rgb), CAPY_IMAGE_OK,
                     CAPY_IMAGE_FORMAT_JPEG);
  test_expect_detect(test_fixture_qoi_1x1_rgb,
                     sizeof(test_fixture_qoi_1x1_rgb), CAPY_IMAGE_OK,
                     CAPY_IMAGE_FORMAT_QOI);

  /* Unknown magic and too-short prefixes fail closed as unsupported. */
  test_expect_detect(test_fixture_bmp_invalid_magic,
                     sizeof(test_fixture_bmp_invalid_magic),
                     CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT,
                     CAPY_IMAGE_FORMAT_UNKNOWN);
  test_expect_detect(test_fixture_png_invalid_signature,
                     sizeof(test_fixture_png_invalid_signature),
                     CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT,
                     CAPY_IMAGE_FORMAT_UNKNOWN);
  test_expect_detect(short_bmp_prefix, sizeof(short_bmp_prefix),
                     CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT,
                     CAPY_IMAGE_FORMAT_UNKNOWN);

  /* NULL arguments are rejected without dereferencing the missing pointer. */
  TEST_EXPECT(capy_image_detect_memory(0, 8u, &format) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  TEST_EXPECT(capy_image_detect_memory(test_fixture_bmp_1x1_24,
                                       sizeof(test_fixture_bmp_1x1_24), 0) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
}

void test_generic_decode(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state inflater_state;
  struct capy_image_rgba32 image;

  /* BMP dispatch: no inflater required, NULL limits use defaults. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_image_decode_memory(test_fixture_bmp_1x1_24,
                                       sizeof(test_fixture_bmp_1x1_24),
                                       &allocator, 0, 0,
                                       &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 1u && image.height == 1u && image.pixels != 0);
  TEST_EXPECT(test_hash_pixels_argb32(image.pixels, 1u) ==
              TEST_FIXTURE_HASH_ARGB_FF112233);
  capy_image_rgba32_free(&image);

  /* PNG dispatch: inflater threaded through, NULL limits use defaults. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &inflater_state;
  TEST_EXPECT(capy_image_decode_memory(test_fixture_png_1x1_rgb,
                                       sizeof(test_fixture_png_1x1_rgb),
                                       &allocator, &inflater, 0,
                                       &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(inflater_state.calls == 1);
  TEST_EXPECT(image.width == 1u && image.height == 1u);
  TEST_EXPECT(test_hash_pixels_argb32(image.pixels, 1u) ==
              TEST_FIXTURE_HASH_ARGB_FF112233);
  capy_image_rgba32_free(&image);

  /* JPEG dispatch: no inflater required. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_image_decode_memory(test_fixture_jpeg_1x1_grayscale,
                                       sizeof(test_fixture_jpeg_1x1_grayscale),
                                       &allocator, 0, 0,
                                       &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 1u && image.height == 1u);
  TEST_EXPECT(test_hash_pixels_argb32(image.pixels, 1u) ==
              TEST_FIXTURE_HASH_ARGB_FF808080);
  capy_image_rgba32_free(&image);

  /* Unsupported magic fails closed and resets the output. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_image_decode_memory(test_fixture_bmp_invalid_magic,
                                       sizeof(test_fixture_bmp_invalid_magic),
                                       &allocator, 0, 0, &image) ==
              CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_expect_reset(&image);

  /* PNG dispatch without an inflater fails closed through the decoder. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_image_decode_memory(test_fixture_png_1x1_rgb,
                                       sizeof(test_fixture_png_1x1_rgb),
                                       &allocator, 0, 0, &image) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  test_expect_reset(&image);
}

#include "test_image_common.h"
#include "../fixtures/image/golden_image_fixtures.h"
#include "../fixtures/image/negative_image_fixtures.h"

static void test_expect_meta(const uint8_t *data, size_t size, int rc,
                             enum capy_image_format format, uint32_t width,
                             uint32_t height, uint32_t channels, uint32_t bits,
                             uint32_t has_alpha) {
  struct capy_image_metadata meta;
  meta.format = CAPY_IMAGE_FORMAT_JPEG;
  meta.width = 123u;
  meta.height = 456u;
  meta.channels = 9u;
  meta.bits_per_channel = 9u;
  meta.has_alpha = 9u;
  TEST_EXPECT(capy_image_query_memory(data, size, &meta) == rc);
  TEST_EXPECT(meta.format == format);
  TEST_EXPECT(meta.width == width);
  TEST_EXPECT(meta.height == height);
  TEST_EXPECT(meta.channels == channels);
  TEST_EXPECT(meta.bits_per_channel == bits);
  TEST_EXPECT(meta.has_alpha == has_alpha);
}

void test_metadata_query(void) {
  static const uint8_t progressive_jpeg[] = {
      0xFFu, 0xD8u, 0xFFu, 0xC2u, 0x00u, 0x0Bu, 0x08u, 0x00u,
      0x01u, 0x00u, 0x01u, 0x01u, 0x01u, 0x11u, 0x00u};

  /* Supported headers report source channels, bit depth and output alpha. */
  test_expect_meta(test_fixture_bmp_1x1_24, sizeof(test_fixture_bmp_1x1_24),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_BMP, 1u, 1u, 3u, 8u, 0u);
  test_expect_meta(test_fixture_bmp_2x2_24, sizeof(test_fixture_bmp_2x2_24),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_BMP, 2u, 2u, 3u, 8u, 0u);
  test_expect_meta(test_fixture_bmp_1x1_32, sizeof(test_fixture_bmp_1x1_32),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_BMP, 1u, 1u, 4u, 8u, 0u);
  test_expect_meta(test_fixture_png_1x1_rgb, sizeof(test_fixture_png_1x1_rgb),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_PNG, 1u, 1u, 3u, 8u, 0u);
  test_expect_meta(test_fixture_png_1x1_rgba, sizeof(test_fixture_png_1x1_rgba),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_PNG, 1u, 1u, 4u, 8u, 1u);
  test_expect_meta(test_fixture_png_1x1_grayscale,
                   sizeof(test_fixture_png_1x1_grayscale), CAPY_IMAGE_OK,
                   CAPY_IMAGE_FORMAT_PNG, 1u, 1u, 1u, 8u, 0u);
  test_expect_meta(test_fixture_jpeg_1x1_grayscale,
                   sizeof(test_fixture_jpeg_1x1_grayscale), CAPY_IMAGE_OK,
                   CAPY_IMAGE_FORMAT_JPEG, 1u, 1u, 1u, 8u, 0u);
  test_expect_meta(test_fixture_jpeg_1x1_rgb, sizeof(test_fixture_jpeg_1x1_rgb),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_JPEG, 1u, 1u, 3u, 8u, 0u);
  test_expect_meta(test_fixture_qoi_1x1_rgb, sizeof(test_fixture_qoi_1x1_rgb),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_QOI, 1u, 1u, 3u, 8u, 0u);
  test_expect_meta(test_fixture_qoi_ops_6x1, sizeof(test_fixture_qoi_ops_6x1),
                   CAPY_IMAGE_OK, CAPY_IMAGE_FORMAT_QOI, 6u, 1u, 4u, 8u, 1u);

  {
    /* ICO wrapping a 4x4 32bpp BMP sub-image: BITMAPINFOHEADER height is
       doubled (8) for the AND mask, so the reported height is 4. */
    static const uint8_t ico_bmp_4x4_32[] = {
        0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u,
        0x04u, 0x04u, 0x00u, 0x00u, 0x01u, 0x00u, 0x20u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x16u, 0x00u, 0x00u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u,
        0x08u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x20u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
    static const uint8_t ico_truncated[] = {0x00u, 0x00u, 0x01u, 0x00u};
    test_expect_meta(ico_bmp_4x4_32, sizeof(ico_bmp_4x4_32), CAPY_IMAGE_OK,
                     CAPY_IMAGE_FORMAT_ICO, 4u, 4u, 4u, 8u, 1u);
    test_expect_meta(ico_truncated, sizeof(ico_truncated),
                     CAPY_IMAGE_ERR_TRUNCATED_DATA, CAPY_IMAGE_FORMAT_UNKNOWN,
                     0u, 0u, 0u, 0u, 0u);
  }

  /* Unsupported / malformed headers fail closed with a zeroed metadata. */
  test_expect_meta(test_fixture_bmp_invalid_magic,
                   sizeof(test_fixture_bmp_invalid_magic),
                   CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT, CAPY_IMAGE_FORMAT_UNKNOWN,
                   0u, 0u, 0u, 0u, 0u);
  test_expect_meta(test_fixture_png_unsupported_color,
                   sizeof(test_fixture_png_unsupported_color),
                   CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT, CAPY_IMAGE_FORMAT_UNKNOWN,
                   0u, 0u, 0u, 0u, 0u);
  test_expect_meta(test_fixture_bmp_truncated_header,
                   sizeof(test_fixture_bmp_truncated_header),
                   CAPY_IMAGE_ERR_TRUNCATED_DATA, CAPY_IMAGE_FORMAT_UNKNOWN, 0u,
                   0u, 0u, 0u, 0u);
  test_expect_meta(progressive_jpeg, sizeof(progressive_jpeg),
                   CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT, CAPY_IMAGE_FORMAT_UNKNOWN,
                   0u, 0u, 0u, 0u, 0u);

  /* NULL arguments are rejected without dereferencing the missing pointer. */
  {
    struct capy_image_metadata meta;
    TEST_EXPECT(capy_image_query_memory(0, 8u, &meta) ==
                CAPY_IMAGE_ERR_INVALID_ARGUMENT);
    TEST_EXPECT(capy_image_query_memory(test_fixture_bmp_1x1_24,
                                        sizeof(test_fixture_bmp_1x1_24), 0) ==
                CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  }
}

void test_metadata_decode_consistency(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_metadata meta;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_image_query_memory(test_fixture_bmp_2x2_24,
                                      sizeof(test_fixture_bmp_2x2_24),
                                      &meta) == CAPY_IMAGE_OK);
  TEST_EXPECT(capy_bmp_decode_memory(test_fixture_bmp_2x2_24,
                                     sizeof(test_fixture_bmp_2x2_24),
                                     &allocator, &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(meta.width == image.width);
  TEST_EXPECT(meta.height == image.height);
  capy_image_rgba32_free(&image);
}

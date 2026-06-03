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
  TEST_EXPECT(image->width == 0u);
  TEST_EXPECT(image->height == 0u);
  TEST_EXPECT(image->pixels == 0);
  TEST_EXPECT(image->allocator.alloc == 0);
  TEST_EXPECT(image->allocator.free == 0);
}

void test_qoi_decode(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  static const uint32_t expected_ops[6] = {0xFF0A141Eu, 0x800A141Eu,
                                           0x800A141Eu, 0x800B141Du,
                                           0x800E191Bu, 0xFF0A141Eu};
  uint32_t i;

  /* 1x1 RGB: single QOI_OP_RGB, opaque output. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_qoi_decode_memory(test_fixture_qoi_1x1_rgb,
                                     sizeof(test_fixture_qoi_1x1_rgb),
                                     &allocator, &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 1u && image.height == 1u && image.pixels != 0);
  TEST_EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);

  /* 6x1 multi-op: verify every pixel produced by each QOI operation. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_qoi_decode_memory(test_fixture_qoi_ops_6x1,
                                     sizeof(test_fixture_qoi_ops_6x1),
                                     &allocator, &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 6u && image.height == 1u && image.pixels != 0);
  for (i = 0u; i < 6u; ++i) {
    TEST_EXPECT(image.pixels[i] == expected_ops[i]);
  }
  capy_image_rgba32_free(&image);

  /* Same buffer through the generic dispatcher (no inflater needed). */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  TEST_EXPECT(capy_image_decode_memory(test_fixture_qoi_ops_6x1,
                                       sizeof(test_fixture_qoi_ops_6x1),
                                       &allocator, 0, 0,
                                       &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 6u && image.height == 1u);
  TEST_EXPECT(image.pixels[3] == 0x800B141Du);
  capy_image_rgba32_free(&image);
}

void test_qoi_failures(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  static const uint8_t truncated_header[] = {0x71u, 0x6Fu, 0x69u, 0x66u};
  static const uint8_t over_limit[] = {
      0x71u, 0x6Fu, 0x69u, 0x66u, 0x00u, 0x00u, 0x13u, 0x88u,
      0x00u, 0x00u, 0x00u, 0x01u, 0x03u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u};
  static const uint8_t truncated_stream[] = {
      0x71u, 0x6Fu, 0x69u, 0x66u, 0x00u, 0x00u, 0x00u, 0x02u,
      0x00u, 0x00u, 0x00u, 0x01u, 0x03u, 0x00u, 0xFEu, 0x11u,
      0x22u, 0x33u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x01u};
  uint8_t bad_marker[sizeof(test_fixture_qoi_1x1_rgb)];
  size_t k;

  /* Truncated header (below the 14+8 minimum). */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_qoi_decode_memory(truncated_header, sizeof(truncated_header),
                                     &allocator, &image) ==
              CAPY_IMAGE_ERR_TRUNCATED_DATA);
  test_expect_reset(&image);

  /* Wrong magic is unsupported. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_qoi_decode_memory(test_fixture_bmp_1x1_24,
                                     sizeof(test_fixture_bmp_1x1_24),
                                     &allocator, &image) ==
              CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT);
  test_expect_reset(&image);

  /* Corrupt end marker fails closed. */
  for (k = 0u; k < sizeof(test_fixture_qoi_1x1_rgb); ++k) {
    bad_marker[k] = test_fixture_qoi_1x1_rgb[k];
  }
  bad_marker[sizeof(bad_marker) - 1u] = 0x00u;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_qoi_decode_memory(bad_marker, sizeof(bad_marker), &allocator,
                                     &image) == CAPY_IMAGE_ERR_CORRUPT_DATA);
  test_expect_reset(&image);

  /* Dimensions above the default limit are rejected before allocating. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_qoi_decode_memory(over_limit, sizeof(over_limit), &allocator,
                                     &image) == CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  test_expect_reset(&image);
  TEST_EXPECT(heap.alloc_calls == 0);

  /* Pixel stream ends before all pixels are produced. */
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_qoi_decode_memory(truncated_stream, sizeof(truncated_stream),
                                     &allocator, &image) ==
              CAPY_IMAGE_ERR_TRUNCATED_DATA);
  test_expect_reset(&image);
  TEST_EXPECT(heap.free_calls == heap.alloc_calls);

  /* Allocator failure fails closed. */
  test_heap_reset(&heap);
  heap.fail_after = 0;
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_qoi_decode_memory(test_fixture_qoi_1x1_rgb,
                                     sizeof(test_fixture_qoi_1x1_rgb),
                                     &allocator, &image) ==
              CAPY_IMAGE_ERR_OUT_OF_MEMORY);
  test_expect_reset(&image);
}

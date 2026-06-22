#include "test_image_common.h"

static void test_ico_poison(struct capy_image_rgba32 *image,
                            struct capy_image_allocator allocator) {
  image->width = 99u;
  image->height = 88u;
  image->pixels = (uint32_t *)1;
  image->allocator = allocator;
}

void test_ico_invalid_inputs_fail_closed(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state ist;
  struct capy_image_rgba32 image;
  static const uint8_t ico_hdr[] = {
      0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u
  };

  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  ist.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &ist;
  test_ico_poison(&image, allocator);
  TEST_EXPECT(capy_ico_decode_memory(0, 6u, &allocator, &inflater, &image) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  TEST_EXPECT(image.width == 0u && image.pixels == 0);
  test_ico_poison(&image, allocator);
  TEST_EXPECT(capy_ico_decode_memory(ico_hdr, sizeof(ico_hdr), 0, &inflater,
                                     &image) == CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  TEST_EXPECT(image.width == 0u && image.pixels == 0);
  test_ico_poison(&image, allocator);
  TEST_EXPECT(capy_ico_decode_memory(ico_hdr, sizeof(ico_hdr), &allocator, 0,
                                     &image) == CAPY_IMAGE_ERR_INVALID_ARGUMENT);
  TEST_EXPECT(image.width == 0u && image.pixels == 0);
  TEST_EXPECT(capy_ico_decode_memory(ico_hdr, sizeof(ico_hdr), &allocator,
                                     &inflater, 0) ==
              CAPY_IMAGE_ERR_INVALID_ARGUMENT);
}

void test_ico_decode_bmp_matches_standalone(void) {
  static const uint8_t ico_bmp[] = {
      0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x02u, 0x02u, 0x00u, 0x00u, 0x01u, 0x00u,
      0x20u, 0x00u, 0x40u, 0x00u, 0x00u, 0x00u, 0x16u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
      0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u,
      0x20u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x10u, 0x20u, 0x30u, 0xFFu, 0x40u, 0x50u, 0x60u, 0xFFu, 0x70u, 0x80u,
      0x90u, 0xFFu, 0xA0u, 0xB0u, 0xC0u, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u
  };

  static const uint8_t bmp[] = {
      0x42u, 0x4Du, 0x46u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x36u, 0x00u,
      0x00u, 0x00u, 0x28u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
      0x00u, 0x00u, 0x01u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x20u, 0x30u, 0xFFu, 0x40u, 0x50u,
      0x60u, 0xFFu, 0x70u, 0x80u, 0x90u, 0xFFu, 0xA0u, 0xB0u, 0xC0u, 0xFFu
  };

  struct test_heap heap_ico;
  struct test_heap heap_bmp;
  struct capy_image_allocator alloc_ico;
  struct capy_image_allocator alloc_bmp;
  struct capy_image_inflater inflater;
  struct test_inflater_state ist;
  struct capy_image_rgba32 img_ico;
  struct capy_image_rgba32 img_bmp;
  test_heap_reset(&heap_ico);
  test_heap_reset(&heap_bmp);
  alloc_ico = test_allocator(&heap_ico);
  alloc_bmp = test_allocator(&heap_bmp);
  ist.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &ist;
  TEST_EXPECT(capy_ico_decode_memory(ico_bmp, sizeof(ico_bmp), &alloc_ico,
                                     &inflater, &img_ico) == CAPY_IMAGE_OK);
  TEST_EXPECT(capy_bmp_decode_memory(bmp, sizeof(bmp), &alloc_bmp, &img_bmp) ==
              CAPY_IMAGE_OK);
  TEST_EXPECT(img_ico.width == 2u && img_ico.height == 2u);
  TEST_EXPECT(img_ico.width == img_bmp.width &&
              img_ico.height == img_bmp.height);
  TEST_EXPECT(test_hash_pixels_argb32(img_ico.pixels, 4u) ==
              test_hash_pixels_argb32(img_bmp.pixels, 4u));
  capy_image_rgba32_free(&img_ico);
  capy_image_rgba32_free(&img_bmp);
}

void test_ico_decode_png_subimage(void) {
  static const uint8_t ico_png[] = {
      0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u,
      0x20u, 0x00u, 0x3Au, 0x00u, 0x00u, 0x00u, 0x16u, 0x00u, 0x00u, 0x00u, 0x89u, 0x50u,
      0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au, 0x00u, 0x00u, 0x00u, 0x0Du, 0x49u, 0x48u,
      0x44u, 0x52u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x08u, 0x02u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x49u,
      0x44u, 0x41u, 0x54u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x49u, 0x45u, 0x4Eu, 0x44u, 0x00u, 0x00u, 0x00u, 0x00u
  };

  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state ist;
  struct capy_image_rgba32 image;
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  ist.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &ist;
  TEST_EXPECT(capy_ico_decode_memory(ico_png, sizeof(ico_png), &allocator,
                                     &inflater, &image) == CAPY_IMAGE_OK);
  TEST_EXPECT(image.width == 1u && image.height == 1u && image.pixels != 0);
  TEST_EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);
}

void test_ico_negatives(void) {
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state ist;
  struct capy_image_rgba32 image;
  static const uint8_t trunc[] = {0x00u, 0x00u, 0x01u, 0x00u};
  static const uint8_t zero_count[] = {0x00u, 0x00u, 0x01u, 0x00u,
                                       0x00u, 0x00u};
  static const uint8_t bad_off[] = {
      0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x02u, 0x02u, 0x00u, 0x00u, 0x01u, 0x00u,
      0x20u, 0x00u, 0x28u, 0x00u, 0x00u, 0x00u, 0xFFu, 0x00u, 0x00u, 0x00u
  };

  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  ist.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &ist;
  test_ico_poison(&image, allocator);
  TEST_EXPECT(capy_ico_decode_memory(trunc, sizeof(trunc), &allocator,
                                     &inflater, &image) ==
              CAPY_IMAGE_ERR_TRUNCATED_DATA);
  TEST_EXPECT(image.width == 0u && image.pixels == 0);
  TEST_EXPECT(capy_ico_decode_memory(zero_count, sizeof(zero_count), &allocator,
                                     &inflater, &image) ==
              CAPY_IMAGE_ERR_CORRUPT_DATA);
  TEST_EXPECT(capy_ico_decode_memory(bad_off, sizeof(bad_off), &allocator,
                                     &inflater, &image) ==
              CAPY_IMAGE_ERR_TRUNCATED_DATA);
}

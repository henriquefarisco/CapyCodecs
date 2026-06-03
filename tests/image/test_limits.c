#include "test_image_common.h"

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

static void test_u32le(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void test_u16le(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void test_u32be(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)((v >> 24) & 0xFFu);
  p[1] = (uint8_t)((v >> 16) & 0xFFu);
  p[2] = (uint8_t)((v >> 8) & 0xFFu);
  p[3] = (uint8_t)(v & 0xFFu);
}

static void test_make_bmp_header(uint8_t *bmp, uint32_t width,
                                 uint32_t height) {
  for (size_t i = 0u; i < 54u; ++i) {
    bmp[i] = 0u;
  }
  bmp[0] = 0x42u;
  bmp[1] = 0x4Du;
  test_u32le(bmp + 2u, 54u);
  test_u32le(bmp + 10u, 54u);
  test_u32le(bmp + 14u, 40u);
  test_u32le(bmp + 18u, width);
  test_u32le(bmp + 22u, height);
  test_u16le(bmp + 26u, 1u);
  test_u16le(bmp + 28u, 24u);
}

static size_t test_make_png_header(uint8_t *png, uint32_t width,
                                   uint32_t height) {
  static const uint8_t sig[8] = {0x89u, 0x50u, 0x4Eu, 0x47u,
                                 0x0Du, 0x0Au, 0x1Au, 0x0Au};
  size_t pos = 0u;
  for (size_t i = 0u; i < sizeof(sig); ++i) {
    png[pos++] = sig[i];
  }
  test_u32be(png + pos, 13u);
  pos += 4u;
  png[pos++] = 0x49u;
  png[pos++] = 0x48u;
  png[pos++] = 0x44u;
  png[pos++] = 0x52u;
  test_u32be(png + pos, width);
  pos += 4u;
  test_u32be(png + pos, height);
  pos += 4u;
  png[pos++] = 8u;
  png[pos++] = 2u;
  png[pos++] = 0u;
  png[pos++] = 0u;
  png[pos++] = 0u;
  test_u32be(png + pos, 0u);
  pos += 4u;
  test_u32be(png + pos, 1u);
  pos += 4u;
  png[pos++] = 0x49u;
  png[pos++] = 0x44u;
  png[pos++] = 0x41u;
  png[pos++] = 0x54u;
  png[pos++] = 0u;
  test_u32be(png + pos, 0u);
  pos += 4u;
  test_u32be(png + pos, 0u);
  pos += 4u;
  png[pos++] = 0x49u;
  png[pos++] = 0x45u;
  png[pos++] = 0x4Eu;
  png[pos++] = 0x44u;
  test_u32be(png + pos, 0u);
  pos += 4u;
  return pos;
}

static size_t test_make_jpeg_sof0(uint8_t *jpeg, uint16_t width,
                                  uint16_t height) {
  size_t pos = 0u;
  jpeg[pos++] = 0xFFu;
  jpeg[pos++] = 0xD8u;
  jpeg[pos++] = 0xFFu;
  jpeg[pos++] = 0xC0u;
  jpeg[pos++] = 0x00u;
  jpeg[pos++] = 0x11u;
  jpeg[pos++] = 0x08u;
  jpeg[pos++] = (uint8_t)(height >> 8);
  jpeg[pos++] = (uint8_t)(height & 0xFFu);
  jpeg[pos++] = (uint8_t)(width >> 8);
  jpeg[pos++] = (uint8_t)(width & 0xFFu);
  jpeg[pos++] = 0x03u;
  jpeg[pos++] = 0x01u;
  jpeg[pos++] = 0x11u;
  jpeg[pos++] = 0x00u;
  jpeg[pos++] = 0x02u;
  jpeg[pos++] = 0x11u;
  jpeg[pos++] = 0x00u;
  jpeg[pos++] = 0x03u;
  jpeg[pos++] = 0x11u;
  jpeg[pos++] = 0x00u;
  return pos;
}

static void test_bmp_resource_limit_case(uint32_t width, uint32_t height) {
  uint8_t bmp[54];
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_make_bmp_header(bmp, width, height);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_bmp_decode_memory(bmp, sizeof(bmp), &allocator, &image) ==
              CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  test_expect_reset(&image);
  TEST_EXPECT(heap.alloc_calls == 0);
}

static void test_png_resource_limit_case(uint32_t width, uint32_t height) {
  uint8_t png[64];
  size_t png_size;
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state inflater_state;
  struct capy_image_rgba32 image;
  png_size = test_make_png_header(png, width, height);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &inflater_state;
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_png_decode_memory(png, png_size, &allocator, &inflater,
                                     &image) == CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  TEST_EXPECT(inflater_state.calls == 0);
  test_expect_reset(&image);
  TEST_EXPECT(heap.free_calls == heap.alloc_calls);
}

static void test_jpeg_resource_limit_case(uint16_t width, uint16_t height) {
  uint8_t jpeg[32];
  size_t jpeg_size;
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  jpeg_size = test_make_jpeg_sof0(jpeg, width, height);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_jpeg_decode_memory(jpeg, jpeg_size, &allocator, &image) ==
              CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  test_expect_reset(&image);
  TEST_EXPECT(heap.alloc_calls == 0);
}

void test_resource_limits(void) {
  test_bmp_resource_limit_case(CAPY_IMAGE_MAX_WIDTH + 1u, 1u);
  test_bmp_resource_limit_case(1u, CAPY_IMAGE_MAX_HEIGHT + 1u);
  test_png_resource_limit_case(CAPY_IMAGE_MAX_WIDTH + 1u, 1u);
  test_png_resource_limit_case(1u, CAPY_IMAGE_MAX_HEIGHT + 1u);
  test_jpeg_resource_limit_case((uint16_t)(CAPY_IMAGE_MAX_WIDTH + 1u), 1u);
  test_jpeg_resource_limit_case(1u, (uint16_t)(CAPY_IMAGE_MAX_HEIGHT + 1u));
}

static void test_bmp_per_call_case(uint32_t width, uint32_t height,
                                   uint32_t max_w, uint32_t max_h,
                                   int expect_limit) {
  uint8_t bmp[54];
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_limits limits;
  struct capy_image_rgba32 image;
  int rc;
  test_make_bmp_header(bmp, width, height);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  capy_image_default_limits(&limits);
  limits.max_width = max_w;
  limits.max_height = max_h;
  test_poison_image(&image, allocator);
  rc = capy_bmp_decode_memory_limited(bmp, sizeof(bmp), &allocator, &limits,
                                      &image);
  if (expect_limit) {
    TEST_EXPECT(rc == CAPY_IMAGE_ERR_RESOURCE_LIMIT);
    TEST_EXPECT(heap.alloc_calls == 0);
    test_expect_reset(&image);
  } else {
    TEST_EXPECT(rc != CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  }
  TEST_EXPECT(heap.free_calls == heap.alloc_calls);
}

static void test_png_per_call_case(uint32_t width, uint32_t height,
                                   uint32_t max_w, uint32_t max_h,
                                   int expect_limit) {
  uint8_t png[64];
  size_t png_size;
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_inflater inflater;
  struct test_inflater_state inflater_state;
  struct capy_image_limits limits;
  struct capy_image_rgba32 image;
  int rc;
  png_size = test_make_png_header(png, width, height);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  inflater_state.calls = 0;
  inflater.inflate = test_inflate_rgb_1x1;
  inflater.user_data = &inflater_state;
  capy_image_default_limits(&limits);
  limits.max_width = max_w;
  limits.max_height = max_h;
  test_poison_image(&image, allocator);
  rc = capy_png_decode_memory_limited(png, png_size, &allocator, &inflater,
                                      &limits, &image);
  if (expect_limit) {
    TEST_EXPECT(rc == CAPY_IMAGE_ERR_RESOURCE_LIMIT);
    test_expect_reset(&image);
  } else {
    TEST_EXPECT(rc != CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  }
  TEST_EXPECT(heap.free_calls == heap.alloc_calls);
}

static void test_jpeg_per_call_case(uint16_t width, uint16_t height,
                                    uint32_t max_w, uint32_t max_h,
                                    int expect_limit) {
  uint8_t jpeg[32];
  size_t jpeg_size;
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_limits limits;
  struct capy_image_rgba32 image;
  int rc;
  jpeg_size = test_make_jpeg_sof0(jpeg, width, height);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  capy_image_default_limits(&limits);
  limits.max_width = max_w;
  limits.max_height = max_h;
  test_poison_image(&image, allocator);
  rc = capy_jpeg_decode_memory_limited(jpeg, jpeg_size, &allocator, &limits,
                                       &image);
  if (expect_limit) {
    TEST_EXPECT(rc == CAPY_IMAGE_ERR_RESOURCE_LIMIT);
    TEST_EXPECT(heap.alloc_calls == 0);
    test_expect_reset(&image);
  } else {
    TEST_EXPECT(rc != CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  }
  TEST_EXPECT(heap.free_calls == heap.alloc_calls);
}

static void test_bmp_null_limits_uses_defaults(void) {
  uint8_t bmp[54];
  struct test_heap heap;
  struct capy_image_allocator allocator;
  struct capy_image_rgba32 image;
  test_make_bmp_header(bmp, CAPY_IMAGE_MAX_WIDTH + 1u, 1u);
  test_heap_reset(&heap);
  allocator = test_allocator(&heap);
  test_poison_image(&image, allocator);
  TEST_EXPECT(capy_bmp_decode_memory_limited(bmp, sizeof(bmp), &allocator, 0,
                                             &image) ==
              CAPY_IMAGE_ERR_RESOURCE_LIMIT);
  test_expect_reset(&image);
  TEST_EXPECT(heap.alloc_calls == 0);
}

void test_per_call_limits(void) {
  /* A tight per-call limit rejects an image the defaults would accept. */
  test_bmp_per_call_case(2u, 2u, 1u, 16u, 1);
  test_png_per_call_case(2000u, 1u, 512u, 512u, 1);
  test_jpeg_per_call_case(2000u, 1u, 512u, 512u, 1);
  /* A relaxed limit accepts dimensions above the former PNG 1024 hard cap. */
  test_bmp_per_call_case(2u, 2u, 4u, 4u, 0);
  test_png_per_call_case(2000u, 1u, 4096u, 4096u, 0);
  test_jpeg_per_call_case(2000u, 1u, 4096u, 4096u, 0);
  /* NULL limits fall back to the documented defaults. */
  test_bmp_null_limits_uses_defaults();
}

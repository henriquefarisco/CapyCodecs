#ifndef CAPY_CODECS_TEST_IMAGE_COMMON_H
#define CAPY_CODECS_TEST_IMAGE_COMMON_H

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

extern int test_failures;

#define TEST_EXPECT(expr)        \
  do {                           \
    if (!(expr)) {               \
      ++test_failures;           \
      return;                    \
    }                            \
  } while (0)

void test_heap_reset(struct test_heap *heap);
struct capy_image_allocator test_allocator(struct test_heap *heap);
uint32_t test_hash_pixels_argb32(const uint32_t *pixels, size_t count);
int test_inflate_rgb_1x1(uint8_t *dest, size_t *dest_len,
                         const uint8_t *source, size_t source_len,
                         void *user_data);
void test_image_abi_contract(void);
void test_free_resets_image(void);
void test_bmp_invalid_inputs_fail_closed(void);
void test_png_invalid_inputs_fail_closed(void);
void test_jpeg_invalid_inputs_fail_closed(void);
void test_bmp_decode_rgb_1x1(void);
void test_png_decode_rgb_1x1_with_inflater(void);
void test_golden_bmp_fixtures(void);
void test_golden_png_fixtures(void);
void test_golden_jpeg_fixtures(void);
void test_negative_bmp_fixtures(void);
void test_negative_png_fixtures(void);
void test_negative_jpeg_fixtures(void);
void test_allocator_failure_matrix(void);
void test_png_inflater_failures(void);
void test_resource_limits(void);
void test_per_call_limits(void);
void test_detect_formats(void);
void test_generic_decode(void);
void test_metadata_query(void);
void test_metadata_decode_consistency(void);
void test_qoi_decode(void);
void test_qoi_failures(void);
void test_ico_invalid_inputs_fail_closed(void);
void test_ico_decode_bmp_matches_standalone(void);
void test_ico_decode_png_subimage(void);
void test_ico_decode_bmp_32bpp_alpha(void);
void test_ico_decode_bmp_24bpp_and_mask(void);
void test_ico_negatives(void);

#endif

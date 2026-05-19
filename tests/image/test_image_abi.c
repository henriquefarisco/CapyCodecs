#include "test_image_common.h"

void test_image_abi_contract(void) {
  struct capy_image_limits limits;
  uint32_t features;
  capy_image_default_limits(0);
  capy_image_default_limits(&limits);
  features = capy_image_codec_features();
  TEST_EXPECT(capy_image_abi_version() == CAPY_IMAGE_ABI_VERSION);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_BMP_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_PNG_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_JPEG_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_ARGB32_OUTPUT) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_ALLOCATOR_INJECTION) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_PNG_INFLATER_INJECTION) != 0u);
  TEST_EXPECT(limits.max_width == CAPY_IMAGE_MAX_WIDTH);
  TEST_EXPECT(limits.max_height == CAPY_IMAGE_MAX_HEIGHT);
  TEST_EXPECT(limits.max_output_bytes == (size_t)CAPY_IMAGE_MAX_WIDTH *
                                             (size_t)CAPY_IMAGE_MAX_HEIGHT *
                                             sizeof(uint32_t));
  TEST_EXPECT(limits.max_temporary_bytes >= limits.max_output_bytes);
}

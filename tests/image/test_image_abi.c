#include "test_image_common.h"

void test_image_abi_contract(void) {
  struct capy_image_limits limits;
  uint32_t features;
  capy_image_default_limits(0);
  capy_image_default_limits(&limits);
  features = capy_image_codec_features();
  TEST_EXPECT(capy_image_abi_version() == CAPY_IMAGE_ABI_VERSION);
  TEST_EXPECT(capy_image_abi_version() >= 2u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_BMP_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_PNG_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_JPEG_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_ARGB32_OUTPUT) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_ALLOCATOR_INJECTION) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_PNG_INFLATER_INJECTION) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_PER_CALL_LIMITS) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_DETECT) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_GENERIC_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_METADATA) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_QOI_DECODE) != 0u);
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_STRERROR) != 0u);
  {
    /* capy_image_strerror: non-NULL + non-empty for every defined code and
       for an out-of-range cast; distinct codes give distinct messages. */
    enum capy_image_error codes[] = {
        CAPY_IMAGE_OK, CAPY_IMAGE_ERR_INVALID_ARGUMENT,
        CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT, CAPY_IMAGE_ERR_CORRUPT_DATA,
        CAPY_IMAGE_ERR_TRUNCATED_DATA, CAPY_IMAGE_ERR_OUT_OF_MEMORY,
        CAPY_IMAGE_ERR_RESOURCE_LIMIT, CAPY_IMAGE_ERR_INFLATER_FAILED};
    size_t i;
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
      const char *s = capy_image_strerror(codes[i]);
      TEST_EXPECT(s != 0 && s[0] != '\0');
    }
    TEST_EXPECT(capy_image_strerror((enum capy_image_error)123) != 0);
    TEST_EXPECT(capy_image_strerror((enum capy_image_error)123)[0] != '\0');
    TEST_EXPECT(capy_image_strerror(CAPY_IMAGE_OK)[0] !=
                capy_image_strerror(CAPY_IMAGE_ERR_CORRUPT_DATA)[0]);
  }
  TEST_EXPECT((features & CAPY_IMAGE_FEATURE_FORMAT_NAME) != 0u);
  {
    /* capy_image_format_name: non-NULL + non-empty for every format and for
       an out-of-range cast; distinct formats give distinct names. */
    enum capy_image_format formats[] = {
        CAPY_IMAGE_FORMAT_UNKNOWN, CAPY_IMAGE_FORMAT_BMP, CAPY_IMAGE_FORMAT_PNG,
        CAPY_IMAGE_FORMAT_JPEG, CAPY_IMAGE_FORMAT_QOI};
    size_t i;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
      const char *n = capy_image_format_name(formats[i]);
      TEST_EXPECT(n != 0 && n[0] != '\0');
    }
    TEST_EXPECT(capy_image_format_name((enum capy_image_format)99) != 0);
    TEST_EXPECT(capy_image_format_name(CAPY_IMAGE_FORMAT_PNG)[0] !=
                capy_image_format_name(CAPY_IMAGE_FORMAT_JPEG)[0]);
  }
  TEST_EXPECT(limits.max_width == CAPY_IMAGE_MAX_WIDTH);
  TEST_EXPECT(limits.max_height == CAPY_IMAGE_MAX_HEIGHT);
  TEST_EXPECT(limits.max_output_bytes == (size_t)CAPY_IMAGE_MAX_WIDTH *
                                             (size_t)CAPY_IMAGE_MAX_HEIGHT *
                                             sizeof(uint32_t));
  TEST_EXPECT(limits.max_temporary_bytes >= limits.max_output_bytes);
}

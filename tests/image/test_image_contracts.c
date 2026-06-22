#include "test_image_common.h"

int test_failures;

int main(void) {
  test_image_abi_contract();
  test_free_resets_image();
  test_bmp_invalid_inputs_fail_closed();
  test_png_invalid_inputs_fail_closed();
  test_jpeg_invalid_inputs_fail_closed();
  test_bmp_decode_rgb_1x1();
  test_png_decode_rgb_1x1_with_inflater();
  test_golden_bmp_fixtures();
  test_golden_png_fixtures();
  test_golden_jpeg_fixtures();
  test_negative_bmp_fixtures();
  test_negative_png_fixtures();
  test_negative_jpeg_fixtures();
  test_allocator_failure_matrix();
  test_png_inflater_failures();
  test_resource_limits();
  test_per_call_limits();
  test_detect_formats();
  test_generic_decode();
  test_metadata_query();
  test_metadata_decode_consistency();
  test_qoi_decode();
  test_qoi_failures();
  test_ico_invalid_inputs_fail_closed();
  test_ico_decode_bmp_matches_standalone();
  test_ico_decode_png_subimage();
  test_ico_negatives();
  return test_failures == 0 ? 0 : 1;
}

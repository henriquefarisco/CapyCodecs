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
  return test_failures == 0 ? 0 : 1;
}

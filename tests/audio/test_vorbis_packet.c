#include "vorbis_packet.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct capy_vorbis_bits reader(const uint8_t *data, size_t bits) {
  struct capy_vorbis_bits value;
  assert(capy_vorbis_bits_init(&value, data, 1, 1) == 0);
  value.bit_count = bits;
  return value;
}
static int window_zero(const struct capy_vorbis_packet_window *value) {
  return !value->block_size && !value->left_start && !value->left_end &&
         !value->right_start && !value->right_end && !value->mode &&
         !value->mapping && !value->blockflag && !value->previous_window &&
         !value->next_window;
}

int main(void) {
  static struct capy_vorbis_setup_workspace setup;
  struct capy_vorbis_setup_summary summary = {0};
  struct capy_vorbis_packet_window window;
  const uint8_t long_packet[] = {0x0a}; /* audio, mode 1, prev short, next long */
  summary.modes = 2; summary.mappings = 2;
  setup.modes[0].mapping = 0;
  setup.modes[1].blockflag = 1; setup.modes[1].mapping = 1;
  struct capy_vorbis_bits bits = reader(long_packet, 4);
  assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 256, 1024,
                                        &window) == 0);
  assert(window.mode == 1 && window.mapping == 1 && window.blockflag);
  assert(!window.previous_window && window.next_window);
  assert(window.block_size == 1024 && window.left_start == 192 &&
         window.left_end == 320 && window.right_start == 512 &&
         window.right_end == 1024 && bits.position == 4);

  const uint8_t short_packet[] = {0};
  bits = reader(short_packet, 2);
  assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 256, 1024,
                                        &window) == 0);
  assert(!window.mode && !window.blockflag && window.block_size == 256);
  assert(window.left_start == 0 && window.left_end == 128 &&
         window.right_start == 128 && window.right_end == 256);

  for (size_t cut = 0; cut < 4; ++cut) {
    bits = reader(long_packet, cut);
    memset(&window, 0x5a, sizeof(window));
    assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 256, 1024,
                                          &window) == CAPY_AUDIO_ERR_TRUNCATED_DATA);
    assert(window_zero(&window));
  }
  const uint8_t header_packet[] = {1};
  bits = reader(header_packet, 1);
  assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 256, 1024,
                                        &window) == CAPY_AUDIO_ERR_CORRUPT_DATA);

  const uint8_t bad_mode[] = {6}; /* audio + mode number 3 */
  summary.modes = 3;
  bits = reader(bad_mode, 3);
  assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 256, 1024,
                                        &window) == CAPY_AUDIO_ERR_CORRUPT_DATA);
  summary.modes = 1; summary.mappings = 1;
  bits = reader(short_packet, 1);
  assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 256, 256,
                                        &window) == 0);
  assert(bits.position == 1 && window.block_size == 256);
  assert(capy_vorbis_packet_window_read(&bits, &summary, &setup, 63, 1024,
                                        &window) == CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  puts("[vorbis-packet] mode selection, window bounds and truncation passed");
  return 0;
}

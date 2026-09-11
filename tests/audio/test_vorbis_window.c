#include "vorbis_window.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct capy_vorbis_packet_window shape(unsigned n, unsigned short_n,
                                               int previous, int next) {
  struct capy_vorbis_packet_window value = {0};
  value.block_size = (uint16_t)n;
  if (n != short_n && !previous) {
    value.left_start = (uint16_t)(n / 4u - short_n / 4u);
    value.left_end = (uint16_t)(n / 4u + short_n / 4u);
  } else value.left_end = (uint16_t)(n / 2u);
  if (n != short_n && !next) {
    value.right_start = (uint16_t)(3u * n / 4u - short_n / 4u);
    value.right_end = (uint16_t)(3u * n / 4u + short_n / 4u);
  } else {
    value.right_start = (uint16_t)(n / 2u);
    value.right_end = (uint16_t)n;
  }
  return value;
}

int main(void) {
  float input[1024], output[1024], scratch[1024];
  for (unsigned i = 0; i < 1024; ++i) input[i] = 1.0f;
  const unsigned flags[][2] = {{0,0},{0,1},{1,0},{1,1}};
  for (unsigned f = 0; f < 4; ++f) {
    struct capy_vorbis_packet_window window = shape(1024, 256,
                                                     flags[f][0], flags[f][1]);
    assert(capy_vorbis_window_apply(&window, input, 1024, 4.0f,
                                    output, 1024, scratch, 1024) == 0);
    for (unsigned i = 0; i < window.left_start; ++i) assert(output[i] == 0.0f);
    for (unsigned i = window.left_end; i < window.right_start; ++i)
      assert(output[i] == 1.0f);
    for (unsigned i = window.right_end; i < 1024; ++i) assert(output[i] == 0.0f);
    unsigned left_n = window.left_end - window.left_start;
    unsigned right_n = window.right_end - window.right_start;
    for (unsigned i = 0; i + 1u < left_n; ++i)
      assert(output[window.left_start + i] <= output[window.left_start + i + 1u]);
    for (unsigned i = 0; i + 1u < right_n; ++i)
      assert(output[window.right_start + i] >= output[window.right_start + i + 1u]);
    assert(output[window.left_start] > 0.0f && output[window.left_end - 1u] <= 1.0f);
    assert(output[window.right_start] <= 1.0f && output[window.right_end - 1u] > 0.0f);
    if (left_n == right_n)
      assert(fabsf(output[window.left_start] - output[window.right_end - 1u]) < 1e-7f);
    assert(left_n && right_n);
  }

  float previous[1024], current[1024], sum[512], sum_scratch[512];
  for (unsigned i = 0; i < 1024; ++i) { previous[i] = 1.0f; current[i] = 2.0f; }
  size_t produced = 0;
  const unsigned sizes[][2] = {{256,256},{256,1024},{1024,256},{1024,1024}};
  for (unsigned s = 0; s < 4; ++s) {
    size_t pn = sizes[s][0], cn = sizes[s][1], expected = pn/4u + cn/4u;
    assert(capy_vorbis_overlap_add(previous,pn,current,cn,4.0f,
                                   sum,512,sum_scratch,512,&produced) == 0);
    assert(produced == expected);
    for (size_t i = 0; i < produced; ++i) {
      size_t pi = pn/2u+i;
      int64_t ci = (int64_t)(cn/4u)-(int64_t)(pn/4u)+(int64_t)i;
      float want = (pi<pn ? 1.0f : 0.0f) +
                   (ci>=0 && (size_t)ci<cn/2u ? 2.0f : 0.0f);
      assert(sum[i] == want);
    }
  }

  struct capy_vorbis_packet_window window = shape(256,256,1,1);
  for (unsigned i=0;i<256;++i) output[i]=7.0f;
  input[17]=NAN;
  assert(capy_vorbis_window_apply(&window,input,256,4.0f,output,256,scratch,256)
         == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  for (unsigned i=0;i<256;++i) assert(output[i]==7.0f);
  input[17]=1.0f;
  previous[0]=INFINITY; memset(sum,0x5a,sizeof(sum));
  assert(capy_vorbis_overlap_add(previous,256,current,256,4.0f,
      sum,512,sum_scratch,512,&produced)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(!produced); previous[0]=1.0f;

  puts("[vorbis-window] four transitions, overlap alignment and atomic failures passed");
  return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "vorbis_floor1.h"
#include "vorbis_vq.h"
#include "vorbis_window.h"
#include "vorbis_mdct.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
static volatile float sink;
static double now(clockid_t clock) {
  struct timespec t;
  assert(clock_gettime(clock,&t)==0);
  return (double)t.tv_sec+(double)t.tv_nsec*1e-9;
}
int main(void) {
  uint16_t x[65]={0,4096}; uint32_t y[65]={32,63};
  uint8_t curve[4096]; float vector[256];
  static float window_input[8192], window_output[8192], window_scratch[8192];
  static float mdct_trig[10240],mdct_input[4096],mdct_output[8192],mdct_scratch[8192];
  static int32_t mdct_bitrev[2048]; struct capy_vorbis_mdct_plan mdct;
  for (unsigned i=0;i<8192;++i) window_input[i]=1.0f;
  const struct capy_vorbis_packet_window window={8192,0,4096,4096,8192,0,0,1,1,1};
  assert(capy_vorbis_mdct_plan_init(8192,mdct_trig,10240,mdct_bitrev,2048,&mdct)==0);
  for (unsigned i=0;i<4096;++i) mdct_input[i]=(float)((int)(i%17)-8)*.01f;
  for (unsigned i=2;i<65;++i) { x[i]=(uint16_t)((i-1)*61); y[i]=i%7; }
  struct capy_vorbis_floor1_plan floor;
  assert(capy_vorbis_floor1_prepare(x,65,1,&floor)==0);
  const uint8_t packet[]={1};
  struct capy_vorbis_book book={0};
  book.entries=1; book.dimensions=256; book.lookup_type=1; book.lookup_values=1;
  book.value_bits=4; book.delta_raw=0x60100000u; book.sequence=1;
  const char *names[]={"float_common","float_underflow","vq_256_sequence","floor1_65x4096","window_8192","mdct_8192"};
  puts("case,batch,iterations,wall_ns_per_call,cpu_ns_per_call");
  for (unsigned workload=0;workload<6;++workload) {
    unsigned iterations=workload<2 ? 2000 : workload==2 ? 200 : workload==3 ? 100 : 20;
    /* Five unreported warmup batches, followed by 101 retained samples. */
    for (int batch=-5;batch<101;++batch) {
      double wall=now(CLOCK_MONOTONIC), cpu=now(CLOCK_PROCESS_CPUTIME_ID);
      for (unsigned i=0;i<iterations;++i) {
        float value;
        if (workload<2) {
          assert(capy_vorbis_float_unpack(workload ? 1u : 0x60100000u,&value)==0);
          sink+=value;
        } else if (workload==2) {
          assert(capy_vorbis_vq_expand(&book,packet,1,0,256,vector,256)==0);
          sink+=vector[255];
        } else if (workload==3) {
          assert(capy_vorbis_floor1_curve(&floor,y,65,4096,curve,4096)==0);
          sink+=curve[4095];
        } else if (workload==4) {
          assert(capy_vorbis_window_apply(&window,window_input,8192,2.0f,
                     window_output,8192,window_scratch,8192)==0);
          sink+=window_output[4095];
        } else {
          assert(capy_vorbis_mdct_backward(&mdct,mdct_input,4096,1000.0f,
                     mdct_output,8192,mdct_scratch,8192)==0);
          sink+=mdct_output[4095];
        }
      }
      cpu=now(CLOCK_PROCESS_CPUTIME_ID)-cpu;
      wall=now(CLOCK_MONOTONIC)-wall;
      if (batch>=0) printf("%s,%d,%u,%.3f,%.3f\n",names[workload],batch,iterations,
                           wall*1e9/iterations,cpu*1e9/iterations);
    }
  }
  return sink<0; /* Retain observable work, without printing timing noise. */
}

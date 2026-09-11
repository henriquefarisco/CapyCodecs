#include "vorbis_mdct.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static float trig[10240],input[4096],output[8192],scratch[8192];
static int32_t bitrev[2048];

static void output_value(float value) {
  for (unsigned i=0;i<8192;++i) output[i]=value;
}

int main(void) {
  struct capy_vorbis_mdct_plan plan;
  for (unsigned exponent=6;exponent<=13;++exponent) {
    unsigned n=1u<<exponent;
    assert(capy_vorbis_mdct_plan_init(n,trig,n+n/4u,bitrev,n/4u,&plan)==0);
    assert(plan.n==n && plan.log2n==exponent && plan.trig_count==n+n/4u &&
           plan.bitrev_count==n/4u && plan.trig==trig && plan.bitrev==bitrev);
    for (unsigned i=0;i<n/2u;++i) input[i]=0.0f;
    input[n/8u]=1.0f;
    assert(capy_vorbis_mdct_backward(&plan,input,n/2u,4.0f,
                                     output,n,scratch,n)==0);
    for (unsigned i=0;i<n;++i) assert(isfinite(output[i]) && fabsf(output[i])<=1.0f);
  }

  assert(capy_vorbis_mdct_plan_init(63,trig,10240,bitrev,2048,&plan)
         == CAPY_AUDIO_ERR_INVALID_ARGUMENT && !plan.n);
  assert(capy_vorbis_mdct_plan_init(8192,trig,10239,bitrev,2048,&plan)
         == CAPY_AUDIO_ERR_RESOURCE_LIMIT && !plan.n);
  assert(capy_vorbis_mdct_plan_init(8192,trig,10240,bitrev,2047,&plan)
         == CAPY_AUDIO_ERR_RESOURCE_LIMIT && !plan.n);
  assert(capy_vorbis_mdct_plan_init(8192,trig,10240,bitrev,2048,&plan)==0);

  for (unsigned i=0;i<4096;++i) input[i]=0.0f;
  input[3]=NAN; output_value(7.0f);
  assert(capy_vorbis_mdct_backward(&plan,input,4096,4.0f,output,8192,scratch,8192)
         == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  for (unsigned i=0;i<8192;++i) assert(output[i]==7.0f);
  input[3]=0.0f;
  assert(capy_vorbis_mdct_backward(&plan,input,4095,4.0f,output,8192,scratch,8192)
         == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_vorbis_mdct_backward(&plan,input,4096,4.0f,output,8191,scratch,8192)
         == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  plan.trig_count--;
  assert(capy_vorbis_mdct_backward(&plan,input,4096,4.0f,output,8192,scratch,8192)
         == CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_mdct_plan_init(64,trig,80,(int32_t *)(void *)trig,16,&plan)
         == CAPY_AUDIO_ERR_INVALID_ARGUMENT && !plan.n);
  assert(capy_vorbis_mdct_plan_init(64,trig,80,bitrev,16,&plan)==0);
  assert(capy_vorbis_mdct_backward(&plan,input,32,4.0f,output,64,output,64)
         == CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  bitrev[0]=(int32_t)plan.n;
  assert(capy_vorbis_mdct_backward(&plan,input,32,4.0f,output,64,scratch,64)
         == CAPY_AUDIO_ERR_INVALID_ARGUMENT);

  puts("[vorbis-mdct] legal plans, maximum transform, bounds and atomic failures passed");
  return 0;
}

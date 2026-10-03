/********************************************************************
 * Backward MDCT structure adapted from libvorbis 1.3.7 mdct.c.     *
 * Copyright 1994-2009 Xiph.Org Foundation. BSD-style license.      *
 * CapyOS adaptation: caller storage, freestanding trig generation, *
 * bounded validation and atomic output.                            *
 ********************************************************************/
#include "vorbis_mdct.h"

#include <float.h>

#ifdef __FAST_MATH__
#error "Vorbis MDCT requires finite checks; disable fast-math"
#endif

#define C_PI3_8 0.38268343236508977175F
#define C_PI2_8 0.70710678118654752441F
#define C_PI1_8 0.92387953251128675613F
#define PI 3.14159265358979323846264338327950288
#define PI_2 1.57079632679489661923132169163975144

static int valid_n(unsigned n) {
  return n >= 64u && n <= CAPY_VORBIS_BLOCK_SAMPLES && !(n & (n - 1u));
}

static int regions_overlap(const void *a,size_t a_bytes,
                           const void *b,size_t b_bytes) {
  uintptr_t as=(uintptr_t)a,bs=(uintptr_t)b;
  if (a_bytes>UINTPTR_MAX-as || b_bytes>UINTPTR_MAX-bs) return 1;
  return as<bs+b_bytes && bs<as+a_bytes;
}

static double sin_half(double x) {
  double x2=x*x, p=1.0/355687428096000.0;
  p=-1.0/1307674368000.0+x2*p;
  p=1.0/6227020800.0+x2*p;
  p=-1.0/39916800.0+x2*p;
  p=1.0/362880.0+x2*p;
  p=-1.0/5040.0+x2*p;
  p=1.0/120.0+x2*p;
  p=-1.0/6.0+x2*p;
  return x*(1.0+x2*p);
}

static double sine(double x) {
  return x <= PI_2 ? sin_half(x) : sin_half(PI-x);
}

static double cosine(double x) {
  return x <= PI_2 ? sin_half(PI_2-x) : -sin_half(x-PI_2);
}

int capy_vorbis_mdct_plan_init(unsigned n, float *trig, size_t trig_capacity,
    int32_t *bitrev, size_t bitrev_capacity,
    struct capy_vorbis_mdct_plan *out) {
  struct capy_vorbis_mdct_plan plan={0};
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out=plan;
  if (!valid_n(n) || !trig || !bitrev) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (trig_capacity < n+n/4u || bitrev_capacity < n/4u)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (regions_overlap(trig,(n+n/4u)*sizeof(*trig),bitrev,(n/4u)*sizeof(*bitrev)) ||
      regions_overlap(out,sizeof(*out),trig,(n+n/4u)*sizeof(*trig)) ||
      regions_overlap(out,sizeof(*out),bitrev,(n/4u)*sizeof(*bitrev)))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  unsigned log2n=0;
  for (unsigned value=n; value>1u; value>>=1) ++log2n;
  unsigned n2=n>>1;
  for (unsigned i=0;i<n/4u;++i) {
    double a=PI/(double)n*(double)(4u*i);
    double b=PI/(2.0*(double)n)*(double)(2u*i+1u);
    trig[i*2u]=(float)cosine(a);
    trig[i*2u+1u]=(float)-sine(a);
    trig[n2+i*2u]=(float)cosine(b);
    trig[n2+i*2u+1u]=(float)sine(b);
  }
  for (unsigned i=0;i<n/8u;++i) {
    double a=PI/(double)n*(double)(4u*i+2u);
    trig[n+i*2u]=(float)(cosine(a)*0.5);
    trig[n+i*2u+1u]=(float)(-sine(a)*0.5);
  }
  unsigned mask=(1u<<(log2n-1u))-1u, msb=1u<<(log2n-2u);
  for (unsigned i=0;i<n/8u;++i) {
    unsigned acc=0;
    for (unsigned j=0; msb>>j; ++j) if ((msb>>j)&i) acc|=1u<<j;
    unsigned reversed=(~acc)&mask;
    bitrev[i*2u]=reversed ? (int32_t)(reversed-1u) : -1;
    bitrev[i*2u+1u]=(int32_t)acc;
  }
  plan.n=n; plan.log2n=log2n; plan.trig=trig; plan.bitrev=bitrev;
  plan.trig_count=n+n/4u; plan.bitrev_count=n/4u;
  *out=plan;
  return CAPY_AUDIO_OK;
}

static void butterfly8(float *x) {
  float r0=x[6]+x[2], r1=x[6]-x[2], r2=x[4]+x[0], r3=x[4]-x[0];
  x[6]=r0+r2; x[4]=r0-r2; r0=x[5]-x[1]; r2=x[7]-x[3];
  x[0]=r1+r0; x[2]=r1-r0; r0=x[5]+x[1]; r1=x[7]+x[3];
  x[3]=r2+r3; x[1]=r2-r3; x[7]=r1+r0; x[5]=r1-r0;
}

static void butterfly16(float *x) {
  float r0=x[1]-x[9],r1=x[0]-x[8]; x[8]+=x[0];x[9]+=x[1];
  x[0]=(r0+r1)*C_PI2_8;x[1]=(r0-r1)*C_PI2_8;
  r0=x[3]-x[11];r1=x[10]-x[2];x[10]+=x[2];x[11]+=x[3];x[2]=r0;x[3]=r1;
  r0=x[12]-x[4];r1=x[13]-x[5];x[12]+=x[4];x[13]+=x[5];
  x[4]=(r0-r1)*C_PI2_8;x[5]=(r0+r1)*C_PI2_8;
  r0=x[14]-x[6];r1=x[15]-x[7];x[14]+=x[6];x[15]+=x[7];x[6]=r0;x[7]=r1;
  butterfly8(x);butterfly8(x+8);
}

static void butterfly32(float *x) {
  float r0=x[30]-x[14],r1=x[31]-x[15];x[30]+=x[14];x[31]+=x[15];x[14]=r0;x[15]=r1;
  r0=x[28]-x[12];r1=x[29]-x[13];x[28]+=x[12];x[29]+=x[13];
  x[12]=r0*C_PI1_8-r1*C_PI3_8;x[13]=r0*C_PI3_8+r1*C_PI1_8;
  r0=x[26]-x[10];r1=x[27]-x[11];x[26]+=x[10];x[27]+=x[11];
  x[10]=(r0-r1)*C_PI2_8;x[11]=(r0+r1)*C_PI2_8;
  r0=x[24]-x[8];r1=x[25]-x[9];x[24]+=x[8];x[25]+=x[9];
  x[8]=r0*C_PI3_8-r1*C_PI1_8;x[9]=r1*C_PI3_8+r0*C_PI1_8;
  r0=x[22]-x[6];r1=x[7]-x[23];x[22]+=x[6];x[23]+=x[7];x[6]=r1;x[7]=r0;
  r0=x[4]-x[20];r1=x[5]-x[21];x[20]+=x[4];x[21]+=x[5];
  x[4]=r1*C_PI1_8+r0*C_PI3_8;x[5]=r1*C_PI3_8-r0*C_PI1_8;
  r0=x[2]-x[18];r1=x[3]-x[19];x[18]+=x[2];x[19]+=x[3];
  x[2]=(r1+r0)*C_PI2_8;x[3]=(r1-r0)*C_PI2_8;
  r0=x[0]-x[16];r1=x[1]-x[17];x[16]+=x[0];x[17]+=x[1];
  x[0]=r1*C_PI3_8+r0*C_PI1_8;x[1]=r1*C_PI1_8-r0*C_PI3_8;
  butterfly16(x);butterfly16(x+16);
}

static void butterfly_first(float *t,float *x,int points) {
  float *x1=x+points-8,*x2=x+(points>>1)-8;
  for (int block=0;block<points/16;++block) {
    float r0=x1[6]-x2[6],r1=x1[7]-x2[7];x1[6]+=x2[6];x1[7]+=x2[7];x2[6]=r1*t[1]+r0*t[0];x2[7]=r1*t[0]-r0*t[1];
    r0=x1[4]-x2[4];r1=x1[5]-x2[5];x1[4]+=x2[4];x1[5]+=x2[5];x2[4]=r1*t[5]+r0*t[4];x2[5]=r1*t[4]-r0*t[5];
    r0=x1[2]-x2[2];r1=x1[3]-x2[3];x1[2]+=x2[2];x1[3]+=x2[3];x2[2]=r1*t[9]+r0*t[8];x2[3]=r1*t[8]-r0*t[9];
    r0=x1[0]-x2[0];r1=x1[1]-x2[1];x1[0]+=x2[0];x1[1]+=x2[1];x2[0]=r1*t[13]+r0*t[12];x2[1]=r1*t[12]-r0*t[13];
    if (block+1<points/16) { x1-=8;x2-=8;t+=16; }
  }
}

static void butterfly_generic(float *t,float *x,int points,int interval) {
  float *x1=x+points-8,*x2=x+(points>>1)-8;
  for (int block=0;block<points/16;++block) {
    float r0=x1[6]-x2[6],r1=x1[7]-x2[7];x1[6]+=x2[6];x1[7]+=x2[7];x2[6]=r1*t[1]+r0*t[0];x2[7]=r1*t[0]-r0*t[1];t+=interval;
    r0=x1[4]-x2[4];r1=x1[5]-x2[5];x1[4]+=x2[4];x1[5]+=x2[5];x2[4]=r1*t[1]+r0*t[0];x2[5]=r1*t[0]-r0*t[1];t+=interval;
    r0=x1[2]-x2[2];r1=x1[3]-x2[3];x1[2]+=x2[2];x1[3]+=x2[3];x2[2]=r1*t[1]+r0*t[0];x2[3]=r1*t[0]-r0*t[1];t+=interval;
    r0=x1[0]-x2[0];r1=x1[1]-x2[1];x1[0]+=x2[0];x1[1]+=x2[1];x2[0]=r1*t[1]+r0*t[0];x2[1]=r1*t[0]-r0*t[1];t+=interval;
    if (block+1<points/16) { x1-=8;x2-=8; }
  }
}

static void butterflies(const struct capy_vorbis_mdct_plan *p,float *x,int points) {
  int stages=(int)p->log2n-5;
  if (--stages>0) butterfly_first(p->trig,x,points);
  for (int i=1;--stages>0;++i)
    for (int j=0;j<(1<<i);++j) butterfly_generic(p->trig,x+(points>>i)*j,points>>i,4<<i);
  for (int j=0;j<points;j+=32) butterfly32(x+j);
}

static void bitreverse(const struct capy_vorbis_mdct_plan *p,float *x) {
  int n=(int)p->n;const int32_t *bit=p->bitrev;float *w0=x,*w1=x+(n>>1);x=w1;float *t=p->trig+n;
  do {
    float *x0=x+bit[0],*x1=x+bit[1];float r0=x0[1]-x1[1],r1=x0[0]+x1[0];
    float r2=r1*t[0]+r0*t[1],r3=r1*t[1]-r0*t[0];w1-=4;r0=(x0[1]+x1[1])*.5f;r1=(x0[0]-x1[0])*.5f;
    w0[0]=r0+r2;w1[2]=r0-r2;w0[1]=r1+r3;w1[3]=r3-r1;
    x0=x+bit[2];x1=x+bit[3];r0=x0[1]-x1[1];r1=x0[0]+x1[0];r2=r1*t[2]+r0*t[3];r3=r1*t[3]-r0*t[2];
    r0=(x0[1]+x1[1])*.5f;r1=(x0[0]-x1[0])*.5f;w0[2]=r0+r2;w1[0]=r0-r2;w0[3]=r1+r3;w1[1]=r3-r1;
    t+=4;bit+=4;w0+=4;
  } while(w0<w1);
}

static void backward_work(const struct capy_vorbis_mdct_plan *p,const float *in,float *out) {
  int n=(int)p->n,n2=n>>1,n4=n>>2;const float *ix=in+n2-7,*t=p->trig+n4;float *ox=out+n2+n4;
  for (int block=0;block<n/16;++block) {ox-=4;ox[0]=-ix[2]*t[3]-ix[0]*t[2];ox[1]=ix[0]*t[3]-ix[2]*t[2];ox[2]=-ix[6]*t[1]-ix[4]*t[0];ox[3]=ix[4]*t[1]-ix[6]*t[0];if(block+1<n/16){ix-=8;t+=4;}}
  ix=in+n2-8;ox=out+n2+n4;t=p->trig+n4;
  for (int block=0;block<n/16;++block) {t-=4;ox[0]=ix[4]*t[3]+ix[6]*t[2];ox[1]=ix[4]*t[2]-ix[6]*t[3];ox[2]=ix[0]*t[1]+ix[2]*t[0];ox[3]=ix[0]*t[0]-ix[2]*t[1];if(block+1<n/16){ix-=8;ox+=4;}}
  butterflies(p,out+n2,n2);bitreverse(p,out);
  float *ox1=out+n2+n4,*ox2=ox1;float *work=out;t=p->trig+n2;
  do {ox1-=4;ox1[3]=work[0]*t[1]-work[1]*t[0];ox2[0]=-(work[0]*t[0]+work[1]*t[1]);ox1[2]=work[2]*t[3]-work[3]*t[2];ox2[1]=-(work[2]*t[2]+work[3]*t[3]);ox1[1]=work[4]*t[5]-work[5]*t[4];ox2[2]=-(work[4]*t[4]+work[5]*t[5]);ox1[0]=work[6]*t[7]-work[7]*t[6];ox2[3]=-(work[6]*t[6]+work[7]*t[7]);ox2+=4;work+=8;t+=8;} while(work<ox1);
  work=out+n2+n4;ox1=out+n4;ox2=ox1;
  do {ox1-=4;work-=4;ox2[0]=-(ox1[3]=work[3]);ox2[1]=-(ox1[2]=work[2]);ox2[2]=-(ox1[1]=work[1]);ox2[3]=-(ox1[0]=work[0]);ox2+=4;} while(ox2<work);
  work=out+n2+n4;ox1=out+n2+n4;ox2=out+n2;
  do {ox1-=4;ox1[0]=work[3];ox1[1]=work[2];ox1[2]=work[1];ox1[3]=work[0];work+=4;} while(ox1>ox2);
}

int capy_vorbis_mdct_backward(const struct capy_vorbis_mdct_plan *p,
    const float *input,size_t input_count,float max_abs,float *out,
    size_t out_capacity,float *scratch,size_t scratch_capacity) {
  if (!p || !input || !out || !scratch || !valid_n(p->n) ||
      p->log2n<6u || p->log2n>13u || !p->trig || !p->bitrev ||
      p->trig_count!=p->n+p->n/4u || p->bitrev_count!=p->n/4u ||
      !(max_abs>0.0f && max_abs<=FLT_MAX)) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (input_count<p->n/2u || out_capacity<p->n || scratch_capacity<p->n)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if ((1u<<p->log2n)!=p->n ||
      regions_overlap(input,p->n/2u*sizeof(*input),out,p->n*sizeof(*out)) ||
      regions_overlap(input,p->n/2u*sizeof(*input),scratch,p->n*sizeof(*scratch)) ||
      regions_overlap(out,p->n*sizeof(*out),scratch,p->n*sizeof(*scratch)) ||
      regions_overlap(p->trig,p->trig_count*sizeof(*p->trig),input,p->n/2u*sizeof(*input)) ||
      regions_overlap(p->trig,p->trig_count*sizeof(*p->trig),out,p->n*sizeof(*out)) ||
      regions_overlap(p->trig,p->trig_count*sizeof(*p->trig),scratch,p->n*sizeof(*scratch)) ||
      regions_overlap(p->bitrev,p->bitrev_count*sizeof(*p->bitrev),input,p->n/2u*sizeof(*input)) ||
      regions_overlap(p->bitrev,p->bitrev_count*sizeof(*p->bitrev),out,p->n*sizeof(*out)) ||
      regions_overlap(p->bitrev,p->bitrev_count*sizeof(*p->bitrev),scratch,p->n*sizeof(*scratch)))
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  for (size_t i=0;i<p->trig_count;++i)
    if (!(p->trig[i]<=1.0f && p->trig[i]>=-1.0f)) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  for (size_t i=0;i<p->bitrev_count;++i)
    if (p->bitrev[i]<-1 || p->bitrev[i]>(int32_t)(p->n/2u-2u))
      return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  for (unsigned i=0;i<p->n/2u;++i)
    if (!(input[i]<=max_abs && input[i]>=-max_abs)) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  backward_work(p,input,scratch);
  for (unsigned i=0;i<p->n;++i)
    if (!(scratch[i]<=max_abs && scratch[i]>=-max_abs)) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  for (unsigned i=0;i<p->n;++i) out[i]=scratch[i];
  return CAPY_AUDIO_OK;
}

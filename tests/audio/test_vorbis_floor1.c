#include "vorbis_floor1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed=0x10f10001u;
static uint32_t random_word(void) {
  seed=seed*1664525u+1013904223u; return seed;
}
int main(void) {
  struct capy_vorbis_floor1_plan p;
  uint16_t x[65]={0,16,8,4,12};
  uint32_t y[65]={0,255,0,0,0};
  uint8_t out[4097];
  assert(capy_vorbis_floor1_prepare(x,5,1,&p)==0);
  assert(p.low[2]==0 && p.high[2]==1 && p.low[3]==0 && p.high[3]==2);
  assert(capy_vorbis_floor1_curve(&p,y,5,20,out,sizeof(out))==0);
  for (unsigned i=0;i<16;++i) assert(out[i]==255*i/16);
  for (unsigned i=16;i<20;++i) assert(out[i]==255);
  y[0]=255; y[1]=0;
  assert(capy_vorbis_floor1_curve(&p,y,5,7,out,sizeof(out))==0);
  for (unsigned i=0;i<7;++i) assert(out[i]==255-255*i/16);
  memset(out,0xa5,sizeof(out)); y[4]=UINT32_MAX;
  assert(capy_vorbis_floor1_curve(&p,y,5,20,out,sizeof(out))==CAPY_AUDIO_ERR_CORRUPT_DATA);
  for (unsigned i=0;i<sizeof(out);++i) assert(out[i]==0xa5);
  y[4]=0;
  assert(capy_vorbis_floor1_curve(&p,y,5,4097,out,sizeof(out))==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_vorbis_floor1_curve(&p,y,5,20,out,19)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(capy_vorbis_floor1_curve(&p,y,4,20,out,20)==CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  x[4]=4;
  assert(capy_vorbis_floor1_prepare(x,5,1,&p)==CAPY_AUDIO_ERR_CORRUPT_DATA && p.count==0);
  x[1]=15;
  assert(capy_vorbis_floor1_prepare(x,2,1,&p)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  x[1]=32768;
  for (unsigned i=2;i<65;++i) { x[i]=(uint16_t)(i-1); y[i]=0; }
  assert(capy_vorbis_floor1_prepare(x,65,1,&p)==0);
  assert(capy_vorbis_floor1_curve(&p,y,65,4096,out,4096)==0);
  assert(out[4096]==0xa5);
  /* Reproducible adversarial plan/scalar/capacity mutations. All failures
   * preserve the whole output, and successes never overwrite the guard. */
  struct capy_vorbis_floor1_plan valid=p;
  for (unsigned trial=0;trial<50000;++trial) {
    p=valid;
    size_t count=2+random_word()%64, bins=1+random_word()%4097;
    p.count=(uint8_t)count;
    for (size_t i=0;i<count;++i) y[i]=random_word()%300;
    unsigned index=random_word()%65;
    switch (trial%5) {
      case 0: p.low[index]=(uint8_t)random_word(); break;
      case 1: p.high[index]=(uint8_t)random_word(); break;
      case 2: p.order[index]=(uint8_t)random_word(); break;
      case 3: p.x[index]=(uint16_t)random_word(); break;
      default: p.multiplier=(uint8_t)random_word(); break;
    }
    memset(out,0xa5,sizeof(out));
    int rc=capy_vorbis_floor1_curve(&p,y,count,bins,out,4096);
    assert(out[4096]==0xa5);
    if (rc) for (size_t i=0;i<sizeof(out);++i) assert(out[i]==0xa5);
  }
  puts("[vorbis-floor1] bounds, slopes, tail, truncation, atomic output and 50000 mutations passed");
  return 0;
}

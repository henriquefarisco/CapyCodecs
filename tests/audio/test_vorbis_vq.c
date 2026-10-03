#include "vorbis_vq.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
int main(void) {
  float value=99, output[4]={99,99,99,99};
  assert(capy_vorbis_float_unpack(0,&value)==0 && value==0);
  assert(capy_vorbis_float_unpack(0x60100000u,&value)==0 && value==1);
  assert(capy_vorbis_float_unpack(0xe0380000u,&value)==0 && value==-3);
  assert(capy_vorbis_float_unpack(0x7fffffffu,&value)==CAPY_AUDIO_ERR_RESOURCE_LIMIT && value==0);
  assert(capy_vorbis_float_unpack(1,&value)==0 && value==0); /* underflow */
  const uint8_t packet[]={0x10,0x32,0x54,0x76};
  struct capy_vorbis_book book={0};
  book.entries=4; book.dimensions=2; book.lookup_type=1; book.lookup_values=2;
  book.value_bits=4; book.minimum_raw=0xe0100000u; book.delta_raw=0x60100000u;
  assert(capy_vorbis_vq_expand(&book,packet,sizeof(packet),2,100,output,4)==0);
  assert(output[0]==-1 && output[1]==0 && output[2]==99);
  book.sequence=1;
  assert(capy_vorbis_vq_expand(&book,packet,sizeof(packet),2,100,output,4)==0);
  assert(output[0]==-1 && output[1]==-1);
  book.lookup_type=2; book.lookup_values=8;
  assert(capy_vorbis_vq_expand(&book,packet,sizeof(packet),3,100,output,4)==0);
  assert(output[0]==5 && output[1]==11);
  output[0]=output[1]=99;
  assert(capy_vorbis_vq_expand(&book,packet,sizeof(packet),3,6,output,4)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(output[0]==99 && output[1]==99); /* second value fails; no partial commit */
  assert(capy_vorbis_vq_expand(&book,packet,3,0,100,output,4)==CAPY_AUDIO_ERR_TRUNCATED_DATA);
  assert(capy_vorbis_vq_expand(&book,packet,4,4,100,output,4)==CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_vq_expand(&book,packet,4,0,100,output,1)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  book.lookup_bit_offset=SIZE_MAX;
  assert(capy_vorbis_vq_expand(&book,packet,4,0,100,output,4)==CAPY_AUDIO_ERR_TRUNCATED_DATA);
  book.lookup_bit_offset=0; book.delta_raw=0x7fffffffu;
  assert(capy_vorbis_vq_expand(&book,packet,4,0,FLT_MAX,output,4)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(output[0]==99 && output[1]==99);
  book.delta_raw=0x60100000u;
  assert(capy_vorbis_vq_expand(&book,packet,4,0,NAN,output,4)==CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_vq_expand(&book,packet,4,0,INFINITY,output,4)==CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_vq_expand(&book,packet,4,0,0,output,4)==CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  /* Identical lookup payload starting at an unaligned bit offset. */
  const uint8_t shifted[]={0x80,0x90,0xa1,0xb2,0x03};
  book.lookup_bit_offset=3;
  assert(capy_vorbis_vq_expand(&book,shifted,sizeof(shifted),3,100,output,4)==0);
  assert(output[0]==5 && output[1]==11);
  /* Maximum dimension with radix one: no overflowing growing divisor. */
  float large[256];
  book.entries=1; book.dimensions=256; book.lookup_type=1; book.lookup_values=1;
  book.lookup_bit_offset=0; book.minimum_raw=0; book.sequence=1;
  const uint8_t one[]={1};
  assert(capy_vorbis_vq_expand(&book,one,1,0,256,large,256)==0);
  for (size_t i=0;i<256;++i) assert(large[i]==(float)(i+1));
  for (size_t i=0;i<256;++i) large[i]=99;
  assert(capy_vorbis_vq_expand(&book,one,1,0,255,large,256)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  for (size_t i=0;i<256;++i) assert(large[i]==99);
  puts("[vorbis-vq] float/lookup/sequence/range/truncation/atomic output passed");
  return 0;
}

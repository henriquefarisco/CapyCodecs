#include "vorbis_residue.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static struct capy_vorbis_bits reader(const uint8_t *data,size_t count) {
  struct capy_vorbis_bits b;
  assert(capy_vorbis_bits_init(&b,data,1,1)==0); b.bit_count=count; return b;
}
int main(void) {
  const uint8_t lengths[]={1,1}, setup[]={0x21,0x43}, audio[]={2};
  struct capy_vorbis_huffman_node nodes[3]; struct capy_vorbis_huffman tree;
  assert(capy_vorbis_huffman_build(lengths,2,nodes,3,&tree)==0);
  struct capy_vorbis_book book={0};
  book.entries=2; book.dimensions=2; book.lookup_type=2; book.lookup_values=4;
  book.value_bits=4; book.delta_raw=0x60100000u;
  float out[6], scratch[6]; struct capy_vorbis_residue_result result;
  for (unsigned type=0;type<2;++type) {
    for (unsigned i=0;i<6;++i) out[i]=10;
    struct capy_vorbis_bits b=reader(audio,2);
    assert(capy_vorbis_residue_partition(type,&book,&tree,setup,2,&b,4,2,100,out,6,scratch,6,&result)==0);
    const float expected[2][4]={{11,13,12,14},{11,12,13,14}};
    for (unsigned i=0;i<4;++i) assert(out[i]==expected[type][i]);
    assert(out[4]==10 && out[5]==10 && result.vectors==2 && !result.exhausted && b.position==2);
    for (size_t cut=0;cut<2;++cut) {
      for (unsigned i=0;i<6;++i) out[i]=10;
      b=reader(audio,cut);
      assert(capy_vorbis_residue_partition(type,&book,&tree,setup,2,&b,4,2,100,out,6,scratch,6,&result)==0);
      assert(result.exhausted && result.vectors==cut && b.error==CAPY_AUDIO_ERR_TRUNCATED_DATA);
      for (unsigned i=0;i<4;++i) {
        float value=10;
        if (cut && i==0) value=11;
        if (cut && i==(type ? 1u : 2u)) value=12;
        assert(out[i]==value);
      }
    }
  }
  struct capy_vorbis_bits b=reader(audio,2);
  for (unsigned i=0;i<6;++i) out[i]=10;
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,2,&b,4,2,13,out,6,scratch,6,&result)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(!result.vectors && !result.exhausted);
  for (unsigned i=0;i<6;++i) assert(out[i]==10);
  b=reader(audio,2);
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,1,&b,4,2,100,out,6,scratch,6,&result)==CAPY_AUDIO_ERR_TRUNCATED_DATA);
  assert(!result.exhausted && !b.error && out[0]==10);
  b=reader(audio,2);
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,2,&b,4,1,100,out,6,scratch,6,&result)==CAPY_AUDIO_ERR_RESOURCE_LIMIT && !b.position);
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,2,&b,4,2,NAN,out,6,scratch,6,&result)==CAPY_AUDIO_ERR_INVALID_ARGUMENT);
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,2,&b,4,2,100,out,6,scratch,3,&result)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  out[2]=INFINITY;
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,2,&b,4,2,100,out,6,scratch,6,&result)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  out[2]=10;
  b=reader(audio,2);
  assert(capy_vorbis_residue_partition(1,&book,&tree,setup,2,&b,3,2,100,out,6,scratch,6,&result)==0);
  assert(out[0]==11 && out[1]==12 && out[2]==13 && out[3]==10);
  for (unsigned i=0;i<6;++i) out[i]=10;
  b=reader(audio,2);
  assert(capy_vorbis_residue_partition(0,&book,&tree,setup,2,&b,3,1,100,out,6,scratch,6,&result)==0);
  assert(out[0]==11 && out[1]==12 && out[2]==10 && result.vectors==1);
  b=reader(audio,2); b.error=CAPY_AUDIO_ERR_CORRUPT_DATA;
  assert(capy_vorbis_residue_partition(0,&book,&tree,setup,2,&b,1,0,100,out,6,scratch,6,&result)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  uint32_t seed=0x10ae5101u;
  for (unsigned trial=0;trial<10000;++trial) {
    struct capy_vorbis_book changed=book;
    seed=seed*1664525u+1013904223u;
    changed.dimensions=seed%258;
    changed.value_bits=(seed>>12)%18;
    changed.lookup_values=(seed>>20)%6;
    changed.lookup_type=(seed>>28)%4;
    b=reader(audio,trial%3);
    for (unsigned i=0;i<6;++i) out[i]=10;
    int rc=capy_vorbis_residue_partition(trial%2,&changed,&tree,setup,2,&b,5,5,100,out,6,scratch,6,&result);
    assert(out[5]==10);
    if (rc) {
      assert(!result.vectors && !result.exhausted);
      for (unsigned i=0;i<6;++i) assert(out[i]==10);
    } else for (unsigned i=0;i<5;++i) assert(isfinite(out[i]) && fabsf(out[i])<=100);
  }
  /* Maximum scalar/symbol budget with a single-symbol book. Host static test
   * storage is intentional; production obtains these buffers from the caller. */
  static uint8_t large_audio[4096];
  static float large_out[32769], large_scratch[32768];
  const uint8_t one_length[]={1}, one_setup[]={1};
  struct capy_vorbis_huffman_node one_node;
  assert(capy_vorbis_huffman_build(one_length,1,&one_node,1,&tree)==0);
  book.entries=book.dimensions=book.lookup_values=1; book.lookup_type=1;
  large_out[32768]=-123;
  assert(capy_vorbis_bits_init(&b,large_audio,sizeof(large_audio),sizeof(large_audio))==0);
  assert(capy_vorbis_residue_partition(1,&book,&tree,one_setup,1,&b,32768,32768,1,
      large_out,32768,large_scratch,32768,&result)==0);
  assert(result.vectors==32768 && !result.exhausted && b.position==32768);
  for (unsigned i=0;i<32768;++i) assert(large_out[i]==1);
  assert(large_out[32768]==-123);
  puts("[vorbis-residue] layouts, addition, tails, work budgets, exhaustion and atomic failures passed");
  return 0;
}

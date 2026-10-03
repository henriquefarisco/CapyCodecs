#include "vorbis_huffman.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct capy_vorbis_huffman_node nodes[131];
static uint8_t packet[512];
static size_t position;
static void put(uint32_t code, unsigned length) {
  for (unsigned i=length;i;--i,++position)
    packet[position/8] |= (uint8_t)(((code>>(i-1))&1u)<<(position%8));
}
int main(void) {
  /* Published Xiph example, in entry order (not length-sorted). */
  const uint8_t lengths[] = {2,4,4,4,4,2,3,3};
  const uint32_t codes[] = {0,4,5,6,7,2,6,7};
  struct capy_vorbis_huffman tree;
  struct capy_vorbis_bits bits;
  uint32_t symbol;
  assert(capy_vorbis_huffman_build(lengths,8,nodes,15,&tree)==0 && tree.count==15);
  for (unsigned i=0;i<8;++i) put(codes[i],lengths[i]);
  assert(capy_vorbis_bits_init(&bits,packet,(position+7)/8,sizeof(packet))==0);
  bits.bit_count=position;
  for (unsigned i=0;i<8;++i) {
    assert(capy_vorbis_huffman_decode(&tree,&bits,&symbol)==0 && symbol==i);
  }
  assert(bits.position==position);
  assert(capy_vorbis_huffman_decode(&tree,&bits,&symbol)==CAPY_AUDIO_ERR_TRUNCATED_DATA);
  assert(symbol==UINT32_MAX);
  for (unsigned i=0;i<8;++i) {
    memset(packet,0,sizeof(packet)); position=0; put(codes[i],lengths[i]);
    for (unsigned cut=0;cut<lengths[i];++cut) {
      assert(capy_vorbis_bits_init(&bits,packet,4,4)==0); bits.bit_count=cut;
      assert(capy_vorbis_huffman_decode(&tree,&bits,&symbol)==CAPY_AUDIO_ERR_TRUNCATED_DATA);
      assert(symbol==UINT32_MAX);
    }
  }
  assert(capy_vorbis_huffman_build(lengths,8,nodes,14,&tree)==CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(!tree.nodes && !tree.count);
  const uint8_t sparse[]={0,0,1,0};
  assert(capy_vorbis_huffman_build(sparse,4,nodes,1,&tree)==0);
  for (unsigned bit=0;bit<2;++bit) {
    packet[0]=(uint8_t)bit;
    assert(capy_vorbis_bits_init(&bits,packet,1,1)==0);
    assert(capy_vorbis_huffman_decode(&tree,&bits,&symbol)==0 && symbol==2 && bits.position==1);
  }
  const uint8_t invalid[][4]={{0,0,0,0},{1,1,1,0},{2,2,0,0},{0,2,0,0},{33,0,0,0}};
  for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
    assert(capy_vorbis_huffman_build(invalid[i],4,nodes,131,&tree)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  uint8_t deep[33];
  for (unsigned i=0;i<31;++i) deep[i]=(uint8_t)(i+1);
  deep[31]=deep[32]=32;
  assert(capy_vorbis_huffman_build(deep,33,nodes,65,&tree)==0);
  memset(packet,255,4);
  assert(capy_vorbis_bits_init(&bits,packet,4,4)==0);
  assert(capy_vorbis_huffman_decode(&tree,&bits,&symbol)==0 && symbol==32 && bits.position==32);
  /* Valid length multisets shuffled repeatedly exercise entry-order allocation. */
  uint32_t seed=0x132ab91;
  for (unsigned trial=0;trial<10000;++trial) {
    seed=seed*1664525u+1013904223u;
    unsigned a=seed%33,b=(seed>>8)%33;
    uint8_t temp=deep[a]; deep[a]=deep[b]; deep[b]=temp;
    assert(capy_vorbis_huffman_build(deep,33,nodes,65,&tree)==0);
  }
  puts("[vorbis-huffman] entry-order/bit-consumption/bounds/depth/shuffles passed");
  return 0;
}

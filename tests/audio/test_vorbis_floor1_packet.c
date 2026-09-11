#include "vorbis_floor1_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct writer { uint8_t data[256]; size_t count; };
static void put(struct writer *w, uint32_t value, unsigned width) {
  for (unsigned i=0;i<width;++i,++w->count)
    w->data[w->count/8]|=(uint8_t)(((value>>i)&1u)<<(w->count%8));
}
static struct capy_vorbis_bits reader(const struct writer *w, size_t count) {
  struct capy_vorbis_bits b;
  assert(capy_vorbis_bits_init(&b,w->data,sizeof(w->data),sizeof(w->data))==0);
  b.bit_count=count; return b;
}
int main(void) {
  struct writer setup={0}, audio={0};
  put(&setup,2,5); put(&setup,0,4); put(&setup,0,4); /* two class-0 partitions */
  put(&setup,1,3); put(&setup,1,2); put(&setup,0,8); /* dim2, sub1, master0 */
  put(&setup,2,8); put(&setup,0,8); /* subclass book1 and unused */
  put(&setup,0,2); put(&setup,4,4); /* multiplier1, rangebits4 */
  put(&setup,8,4); put(&setup,4,4); put(&setup,12,4); put(&setup,2,4);
  struct capy_vorbis_floor1_config config;
  struct capy_vorbis_bits bits=reader(&setup,setup.count);
  assert(capy_vorbis_floor1_config_read(&bits,2,&config)==0);
  assert(bits.position==setup.count && config.plan.count==6 && config.plan.x[5]==2);
  for (size_t cut=0;cut<setup.count;++cut) {
    struct capy_vorbis_floor1_config rejected;
    bits=reader(&setup,cut);
    assert(capy_vorbis_floor1_config_read(&bits,2,&rejected)!=0);
    assert(rejected.book_count==0 && rejected.plan.count==0);
  }
  bits=reader(&setup,setup.count);
  assert(capy_vorbis_floor1_config_read(&bits,1,&config)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  bits=reader(&setup,setup.count);
  assert(capy_vorbis_floor1_config_read(&bits,2,&config)==0);
  const uint8_t lengths0[]={1,1}, lengths1[]={2,2,2,2};
  struct capy_vorbis_huffman_node nodes0[3], nodes1[7];
  struct capy_vorbis_huffman books[2];
  assert(capy_vorbis_huffman_build(lengths0,2,nodes0,3,&books[0])==0);
  assert(capy_vorbis_huffman_build(lengths1,4,nodes1,7,&books[1])==0);
  put(&audio,1,1); put(&audio,50,8); put(&audio,150,8);
  put(&audio,1,1); put(&audio,1,2); /* unused, then symbol2 (MSB-first code10) */
  put(&audio,0,1); put(&audio,2,2); put(&audio,3,2); /* symbols1 and3 */
  struct capy_vorbis_floor1_values values;
  bits=reader(&audio,audio.count);
  assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==0);
  const uint32_t expected[]={50,150,0,2,1,3};
  assert(values.present && !values.exhausted && bits.position==audio.count);
  assert(memcmp(values.y,expected,sizeof(expected))==0);
  uint8_t curve[16];
  assert(capy_vorbis_floor1_curve(&config.plan,values.y,6,16,curve,16)==0);
  for (size_t cut=0;cut<audio.count;++cut) {
    bits=reader(&audio,cut);
    memset(&values,0xa5,sizeof(values));
    assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==0);
    assert(!values.present && values.exhausted && bits.error==CAPY_AUDIO_ERR_TRUNCATED_DATA);
    for (unsigned i=0;i<65;++i) assert(values.y[i]==0);
  }
  audio.data[0]&=0xfe;
  bits=reader(&audio,audio.count);
  assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==0);
  assert(!values.present && !values.exhausted && bits.position==1);
  audio.data[0]|=1;
  bits=reader(&audio,audio.count); bits.error=CAPY_AUDIO_ERR_CORRUPT_DATA;
  assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  assert(!values.present && !values.exhausted);
  config.classes[0].books[0]=2; bits=reader(&audio,audio.count);
  assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  assert(bits.position==0 && !values.present);
  config.classes[0].books[0]=1;
  struct writer invalid={0};
  put(&invalid,1,1); put(&invalid,127,7); put(&invalid,0,7);
  config.plan.multiplier=3; bits=reader(&invalid,invalid.count);
  assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  assert(!values.present && !values.exhausted);
  config.plan.multiplier=1;
  uint32_t saved=nodes1[0].child[1]; nodes1[0].child[1]=UINT32_MAX;
  bits=reader(&audio,audio.count);
  assert(capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values)==CAPY_AUDIO_ERR_CORRUPT_DATA);
  assert(!values.present && !values.exhausted);
  nodes1[0].child[1]=saved;
  uint32_t seed=0x10f10005u;
  for (unsigned trial=0;trial<10000;++trial) {
    struct writer mutated=setup;
    seed=seed*1664525u+1013904223u;
    size_t bit=seed%setup.count;
    mutated.data[bit/8]^=(uint8_t)(1u<<(bit%8));
    bits=reader(&mutated,setup.count);
    int rc=capy_vorbis_floor1_config_read(&bits,2,&config);
    if (rc) { assert(!config.book_count); continue; }
    struct writer packet=audio;
    seed=seed*1664525u+1013904223u; bit=seed%audio.count;
    packet.data[bit/8]^=(uint8_t)(1u<<(bit%8));
    bits=reader(&packet,audio.count);
    rc=capy_vorbis_floor1_packet_read(&config,books,2,&bits,&values);
    if (rc || !values.present) {
      for (unsigned i=0;i<65;++i) assert(!values.y[i]);
    } else {
      assert(capy_vorbis_floor1_curve(&config.plan,values.y,config.plan.count,16,curve,16)==0);
    }
  }
  puts("[vorbis-floor1-packet] retained setup, entropy selection, curve handoff and all-bit-prefix exhaustion passed");
  return 0;
}

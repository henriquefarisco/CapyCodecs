#include "vorbis_floor1_packet.h"
#define R(n) capy_vorbis_bits_read(b,(n))
static int bad(const struct capy_vorbis_bits *b) {
  return b->error ? b->error : CAPY_AUDIO_ERR_CORRUPT_DATA;
}
int capy_vorbis_floor1_config_read(struct capy_vorbis_bits *b,
    unsigned book_count, struct capy_vorbis_floor1_config *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out=(struct capy_vorbis_floor1_config){0};
  if (!b || !b->data || b->position>b->bit_count || !book_count || book_count>256)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (b->error) return b->error;
  struct capy_vorbis_floor1_config c={0};
  uint16_t x[65]={0}; unsigned classes=0, count=2;
  c.book_count=(uint16_t)book_count;
  c.partitions=(uint8_t)R(5);
  for (unsigned i=0;i<c.partitions;++i) {
    c.partition_class[i]=(uint8_t)R(4);
    if (classes<=c.partition_class[i]) classes=c.partition_class[i]+1u;
  }
  for (unsigned i=0;i<classes;++i) {
    struct capy_vorbis_floor1_class *cl=&c.classes[i];
    cl->dimensions=(uint8_t)(R(3)+1u); cl->subclasses=(uint8_t)R(2);
    if (cl->subclasses) {
      cl->masterbook=(uint16_t)R(8);
      if (cl->masterbook>=book_count) return bad(b);
    }
    for (unsigned j=0;j<(1u<<cl->subclasses);++j) {
      cl->books[j]=(int16_t)((int)R(8)-1);
      if (cl->books[j]>=0 && (unsigned)cl->books[j]>=book_count) return bad(b);
    }
  }
  unsigned multiplier=R(2)+1u, range_bits=R(4);
  x[1]=(uint16_t)(1u<<range_bits);
  for (unsigned i=0;i<c.partitions;++i) {
    unsigned dim=c.classes[c.partition_class[i]].dimensions;
    if (dim>65u-count) return bad(b);
    for (unsigned j=0;j<dim;++j) x[count++]=(uint16_t)R(range_bits);
  }
  if (b->error) return b->error;
  int rc=capy_vorbis_floor1_prepare(x,count,multiplier,&c.plan);
  if (rc) return rc;
  *out=c;
  return 0;
}
static int packet_error(int rc, struct capy_vorbis_floor1_values *out) {
  if (rc==CAPY_AUDIO_ERR_TRUNCATED_DATA) { out->exhausted=1; return 0; }
  return rc;
}
int capy_vorbis_floor1_packet_read(const struct capy_vorbis_floor1_config *c,
    const struct capy_vorbis_huffman *books, size_t book_count,
    struct capy_vorbis_bits *b, struct capy_vorbis_floor1_values *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out=(struct capy_vorbis_floor1_values){0};
  if (!c || !books || !b || !b->data || b->position>b->bit_count ||
      !book_count || book_count>256 || book_count!=c->book_count ||
      c->partitions>31 || c->plan.count<2 || c->plan.count>65 ||
      c->plan.multiplier<1 || c->plan.multiplier>4)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  /* Bounded descriptor preflight: no out-of-range tree lookup even if a
   * caller accidentally supplies damaged configuration storage. */
  unsigned count=2;
  for (unsigned i=0;i<c->partitions;++i) {
    unsigned index=c->partition_class[i];
    if (index>=16) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    const struct capy_vorbis_floor1_class *cl=&c->classes[index];
    if (!cl->dimensions || cl->dimensions>8 || cl->subclasses>3 ||
        cl->dimensions>65u-count || (cl->subclasses && cl->masterbook>=book_count))
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    count+=cl->dimensions;
    for (unsigned j=0;j<(1u<<cl->subclasses);++j)
      if (cl->books[j]<-1 || (cl->books[j]>=0 && (unsigned)cl->books[j]>=book_count))
        return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  if (count!=c->plan.count) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  if (b->error) return packet_error(b->error,out);
  unsigned present=R(1);
  if (b->error) return packet_error(b->error,out);
  if (!present) return 0;
  static const unsigned ranges[4]={256,128,86,64};
  unsigned range=ranges[c->plan.multiplier-1], width=0;
  for (unsigned value=range-1;value;value>>=1) ++width;
  struct capy_vorbis_floor1_values values={0};
  values.y[0]=R(width); values.y[1]=R(width);
  if (b->error) return packet_error(b->error,out);
  if (values.y[0]>=range || values.y[1]>=range) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  unsigned offset=2;
  for (unsigned i=0;i<c->partitions;++i) {
    const struct capy_vorbis_floor1_class *cl=&c->classes[c->partition_class[i]];
    uint32_t selector=0, mask=(1u<<cl->subclasses)-1u;
    if (cl->subclasses) {
      int rc=capy_vorbis_huffman_decode(&books[cl->masterbook],b,&selector);
      if (rc) return packet_error(rc,out);
    }
    for (unsigned j=0;j<cl->dimensions;++j) {
      int book=cl->books[selector&mask];
      selector>>=cl->subclasses;
      if (book>=0) {
        int rc=capy_vorbis_huffman_decode(&books[book],b,&values.y[offset]);
        if (rc) return packet_error(rc,out);
        if (values.y[offset]>=range) return CAPY_AUDIO_ERR_CORRUPT_DATA;
      }
      ++offset;
    }
  }
  values.present=1; *out=values;
  return 0;
}

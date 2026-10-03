#include "vorbis_floor1.h"

int capy_vorbis_floor1_prepare(const uint16_t *x, size_t count,
    unsigned multiplier, struct capy_vorbis_floor1_plan *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_vorbis_floor1_plan){0};
  if (!x || count < 2 || count > CAPY_VORBIS_FLOOR1_POINTS ||
      multiplier < 1 || multiplier > 4) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (x[0] || !x[1] || x[1] > 32768u || (x[1] & (x[1]-1u)))
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  struct capy_vorbis_floor1_plan p = {0};
  p.count = (uint8_t)count; p.multiplier = (uint8_t)multiplier;
  for (size_t i=0; i<count; ++i) {
    p.x[i]=x[i]; p.order[i]=(uint8_t)i;
    if (i<2) continue;
    if (!x[i] || x[i]>=x[1]) return CAPY_AUDIO_ERR_CORRUPT_DATA;
    unsigned low=0, high=1;
    for (size_t j=0; j<i; ++j) {
      if (x[j]==x[i]) return CAPY_AUDIO_ERR_CORRUPT_DATA;
      if (x[j]<x[i] && x[j]>x[low]) low=(unsigned)j;
      if (x[j]>x[i] && x[j]<x[high]) high=(unsigned)j;
    }
    p.low[i]=(uint8_t)low; p.high[i]=(uint8_t)high;
  }
  /* Setup-only insertion sort; never reorder the prediction sequence. */
  for (size_t i=1; i<count; ++i) {
    uint8_t key=p.order[i]; size_t j=i;
    while (j && p.x[p.order[j-1]]>p.x[key]) {
      p.order[j]=p.order[j-1]; --j;
    }
    p.order[j]=key;
  }
  *out=p;
  return 0;
}

static int point(unsigned x0, int y0, unsigned x1, int y1, unsigned x) {
  int dy=y1-y0;
  unsigned magnitude=(unsigned)(dy<0 ? -dy : dy);
  int offset=(int)(magnitude*(x-x0)/(x1-x0));
  return dy<0 ? y0-offset : y0+offset;
}

static void line(unsigned x0, int y0, unsigned x1, int y1,
                 size_t bins, uint8_t *out) {
  int dy=y1-y0, dx=(int)(x1-x0), base=dy/dx;
  int remainder=(dy<0 ? -dy : dy)-(base<0 ? -base : base)*dx;
  int correction=base+(dy<0 ? -1 : 1), error=0, y=y0;
  size_t end=x1<bins ? x1 : bins;
  for (size_t x=x0; x<end; ++x) {
    out[x]=(uint8_t)y;
    error+=remainder;
    if (error>=dx) { error-=dx; y+=correction; }
    else y+=base;
  }
}

int capy_vorbis_floor1_curve(const struct capy_vorbis_floor1_plan *p,
    const uint32_t *wrapped, size_t count, size_t bins,
    uint8_t *out, size_t capacity) {
  if (!p || !wrapped || !out || count<2 || count>CAPY_VORBIS_FLOOR1_POINTS ||
      count!=p->count || p->multiplier<1 || p->multiplier>4 || !bins)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (bins>CAPY_VORBIS_FLOOR1_BINS || capacity<bins)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  static const int ranges[4]={256,128,86,64};
  int range=ranges[p->multiplier-1], y[CAPY_VORBIS_FLOOR1_POINTS];
  uint8_t active[CAPY_VORBIS_FLOOR1_POINTS]={1,1};
  if (p->x[0] || !p->x[1] || p->x[1]>32768u || p->order[0]!=0)
    return CAPY_AUDIO_ERR_CORRUPT_DATA;
  /* Check all descriptors and scalars before emitting any output. */
  for (size_t i=0; i<count; ++i) {
    if (wrapped[i]>=(uint32_t)range || p->order[i]>=count)
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    if (i && p->x[p->order[i-1]]>=p->x[p->order[i]])
      return CAPY_AUDIO_ERR_CORRUPT_DATA;
    if (i>=2 && (p->low[i]>=i || p->high[i]>=i ||
        p->x[p->low[i]]>=p->x[i] || p->x[p->high[i]]<=p->x[i] ||
        p->x[i]>=p->x[1])) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  y[0]=(int)wrapped[0]; y[1]=(int)wrapped[1];
  for (size_t i=2; i<count; ++i) {
    unsigned low=p->low[i], high=p->high[i];
    int predicted=point(p->x[low],y[low],p->x[high],y[high],p->x[i]);
    int value=(int)wrapped[i], highroom=range-predicted, lowroom=predicted;
    int room=2*(highroom<lowroom ? highroom : lowroom);
    if (!value) y[i]=predicted;
    else {
      active[low]=active[high]=active[i]=1;
      if (value>=room)
        y[i]=highroom>lowroom ? value-lowroom+predicted : predicted-value+highroom-1;
      else y[i]=(value&1) ? predicted-(value+1)/2 : predicted+value/2;
    }
    if (y[i]<0 || y[i]>=range) return CAPY_AUDIO_ERR_CORRUPT_DATA;
  }
  unsigned previous=0;
  for (size_t i=1; i<count; ++i) {
    unsigned next=p->order[i];
    if (!active[next]) continue;
    line(p->x[previous],y[previous]*p->multiplier,p->x[next],
         y[next]*p->multiplier,bins,out);
    previous=next;
    if (p->x[next]>=bins) return 0;
  }
  for (size_t i=p->x[previous]; i<bins; ++i)
    out[i]=(uint8_t)(y[previous]*p->multiplier);
  return 0;
}

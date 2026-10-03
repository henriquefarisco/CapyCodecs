#include "vorbis_residue.h"
#include <float.h>
#ifdef __FAST_MATH__
#error "Vorbis residue requires finite checks; disable fast-math"
#endif
int capy_vorbis_residue_partition(unsigned type,
    const struct capy_vorbis_book *book, const struct capy_vorbis_huffman *tree,
    const uint8_t *setup, size_t setup_size, struct capy_vorbis_bits *audio,
    size_t count, uint32_t max_vectors, float max_abs,
    float *out, size_t capacity, float *scratch, size_t scratch_capacity,
    struct capy_vorbis_residue_result *result) {
  if (!result) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *result=(struct capy_vorbis_residue_result){0};
  if (!book || !tree || !tree->nodes || !tree->count || !setup || !setup_size ||
      !audio || !audio->data || audio->position>audio->bit_count || !out || !scratch ||
      !count || !book->dimensions || book->dimensions>256 || !book->entries ||
      book->entries>65536 || tree->entries!=book->entries ||
      !(max_abs>0 && max_abs<=FLT_MAX)) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (type>1 || book->lookup_type<1 || book->lookup_type>2)
    return CAPY_AUDIO_ERR_UNSUPPORTED_FORMAT;
  if (count>CAPY_VORBIS_RESIDUE_SCALARS || capacity<count || scratch_capacity<count)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  size_t dim=book->dimensions, step=count/dim;
  size_t needed=type ? (count+dim-1)/dim : step;
  if (needed>max_vectors) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  if (audio->error && audio->error!=CAPY_AUDIO_ERR_TRUNCATED_DATA) return audio->error;
  for (size_t i=0;i<count;++i) {
    if (!(out[i]<=max_abs && out[i]>=-max_abs)) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    scratch[i]=out[i];
  }
  struct capy_vorbis_residue_result done={0};
  float vector[256];
  for (size_t i=0;i<needed;++i) {
    uint32_t symbol;
    int rc=capy_vorbis_huffman_decode(tree,audio,&symbol);
    if (rc==CAPY_AUDIO_ERR_TRUNCATED_DATA) { done.exhausted=1; break; }
    if (rc) return rc;
    rc=capy_vorbis_vq_expand(book,setup,setup_size,symbol,max_abs,vector,256);
    if (rc) return rc;
    for (size_t j=0;j<dim;++j) {
      size_t index=type ? i*dim+j : i+j*step;
      if (index>=count) break;
      double sum=(double)scratch[index]+vector[j];
      if (!(sum<=max_abs && sum>=-(double)max_abs)) return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
      scratch[index]=(float)sum;
    }
    ++done.vectors;
  }
  for (size_t i=0;i<count;++i) out[i]=scratch[i];
  *result=done;
  return 0;
}

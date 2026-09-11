#include "ogg_reader.h"
#include "vorbis_audio_packet.h"
#include "vorbis_headers.h"
#include "vorbis_synthesis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(call) do { int check_rc=(call); if (check_rc) { \
  fprintf(stderr,"decode failure line %d: %d\n",__LINE__,check_rc); return 1; \
} } while (0)

static uint8_t *read_file(const char *path,size_t *size) {
  FILE *f=fopen(path,"rb"); if(!f) return 0;
  if(fseek(f,0,SEEK_END)||(*size=(size_t)ftell(f),fseek(f,0,SEEK_SET))) {fclose(f);return 0;}
  uint8_t *p=(uint8_t *)malloc(*size?*size:1); if(!p){fclose(f);return 0;}
  if(fread(p,1,*size,f)!=*size){free(p);fclose(f);return 0;} fclose(f); return p;
}

int main(int argc,char **argv) {
  if(argc!=3){fprintf(stderr,"usage: %s input.ogg output.f32le\n",argv[0]);return 2;}
  size_t input_size=0; uint8_t *input=read_file(argv[1],&input_size);
  const size_t packet_cap=1u<<20; uint8_t *packet=(uint8_t *)malloc(packet_cap);
  if(!input||!packet) return 1;
  struct capy_ogg_reader ogg;
  CHECK(capy_ogg_reader_init(&ogg,input,input_size,input_size,packet_cap,65536));
  size_t packet_size=0; int next=capy_ogg_reader_next(&ogg,packet,packet_cap,&packet_size);
  if(next!=1) return 1;
  struct capy_audio_limits audio_limits={input_size,1u<<28,1u<<26,192000,8,65536};
  struct capy_vorbis_identification id;
  CHECK(capy_vorbis_parse_identification(packet,packet_size,&audio_limits,&id));
  next=capy_ogg_reader_next(&ogg,packet,packet_cap,&packet_size); if(next!=1)return 1;
  struct capy_vorbis_comments comments;
  CHECK(capy_vorbis_parse_comments(packet,packet_size,&audio_limits,&comments));
  next=capy_ogg_reader_next(&ogg,packet,packet_cap,&packet_size); if(next!=1)return 1;
  uint8_t *setup_packet=(uint8_t *)malloc(packet_size); if(!setup_packet)return 1;
  memcpy(setup_packet,packet,packet_size); size_t setup_size=packet_size;

  struct capy_vorbis_setup_workspace *setup=calloc(1,sizeof(*setup));
  struct capy_vorbis_huffman *trees=calloc(256,sizeof(*trees));
  const size_t node_cap=524288;
  struct capy_vorbis_huffman_node *nodes=malloc(node_cap*sizeof(*nodes));
  uint8_t *lengths=malloc(65536); size_t node_count=0;
  struct capy_vorbis_setup_summary summary;
  struct capy_vorbis_setup_limits setup_limits={packet_cap,256,262144,1048576,{65536,64,1048576}};
  if(!setup||!trees||!nodes||!lengths)return 1;
  CHECK(capy_vorbis_setup_prepare(setup_packet,setup_size,id.channels,&setup_limits,
      setup,lengths,65536,trees,256,nodes,node_cap,&node_count,&summary));

  unsigned max_n=id.blocksize_large; size_t bins=max_n/2u,total=(size_t)id.channels*bins;
  float *small_trig=malloc((id.blocksize_small+id.blocksize_small/4u)*sizeof(float));
  int32_t *small_bitrev=malloc(id.blocksize_small/4u*sizeof(int32_t));
  float *large_trig=malloc((id.blocksize_large+id.blocksize_large/4u)*sizeof(float));
  int32_t *large_bitrev=malloc(id.blocksize_large/4u*sizeof(int32_t));
  struct capy_vorbis_mdct_plan plans[2];
  CHECK(capy_vorbis_mdct_plan_init(id.blocksize_small,small_trig,id.blocksize_small+id.blocksize_small/4u,small_bitrev,id.blocksize_small/4u,&plans[0]));
  CHECK(capy_vorbis_mdct_plan_init(id.blocksize_large,large_trig,id.blocksize_large+id.blocksize_large/4u,large_bitrev,id.blocksize_large/4u,&plans[1]));

  float *spectrum=malloc(total*sizeof(float)),*packet_work=malloc(5u*total*sizeof(float));
  uint8_t *classes=malloc(total); struct capy_vorbis_floor1_values *floors=calloc(id.channels,sizeof(*floors));
  struct capy_vorbis_floor1_values *floor_scratch=calloc(id.channels,sizeof(*floor_scratch));
  float *previous=calloc((size_t)id.channels*CAPY_VORBIS_BLOCK_SAMPLES,sizeof(float));
  float *pcm=malloc((size_t)id.channels*max_n*sizeof(float));
  float *pcm_scratch=malloc((size_t)id.channels*max_n*sizeof(float));
  float *synth_work=malloc(((size_t)id.channels*max_n+3u*max_n)*sizeof(float));
  uint8_t *curve=malloc(max_n/2u); size_t output_count=0,output_cap=65536;
  float *output=malloc(output_cap*sizeof(float));
  if(!spectrum||!packet_work||!classes||!floors||!floor_scratch||!previous||
     !pcm||!pcm_scratch||!synth_work||!curve||!output)return 1;
  struct capy_vorbis_synthesis_state state={0}; uint64_t final_granule=UINT64_MAX;
  while((next=capy_ogg_reader_next(&ogg,packet,packet_cap,&packet_size))==1) {
    struct capy_vorbis_audio_packet_result decoded;
    CHECK(capy_vorbis_audio_packet_decode(packet,packet_size,packet_cap,
        setup_packet,setup_size,&summary,setup,trees,summary.books,id.channels,
        id.blocksize_small,id.blocksize_large,1000000,65536.0f,spectrum,total,
        floors,floor_scratch,packet_work,5u*total,classes,total,&decoded));
    struct capy_vorbis_synthesis_result made;
    CHECK(capy_vorbis_synthesis_finish(&summary,setup,&decoded.window,
        &plans[decoded.window.blockflag],floors,id.channels,65536.0f,
        spectrum,total,previous,(size_t)id.channels*CAPY_VORBIS_BLOCK_SAMPLES,
        pcm,(size_t)id.channels*max_n,pcm_scratch,(size_t)id.channels*max_n,
        synth_work,(size_t)id.channels*max_n+3u*max_n,curve,max_n/2u,&state,&made));
    size_t add=made.frames*id.channels;
    if(add>SIZE_MAX-output_count)return 1;
    if(output_count+add>output_cap){while(output_cap<output_count+add)output_cap*=2;output=realloc(output,output_cap*sizeof(float));if(!output)return 1;}
    memcpy(output+output_count,pcm,add*sizeof(float)); output_count+=add;
    if(ogg.packet_has_granule) final_granule=ogg.packet_granule;
  }
  if(next<0)return 1;
  if(final_granule!=UINT64_MAX && final_granule<=SIZE_MAX/id.channels &&
     (size_t)final_granule*id.channels<output_count)
    output_count=(size_t)final_granule*id.channels;
  FILE *out=fopen(argv[2],"wb"); if(!out)return 1;
  if(fwrite(output,sizeof(float),output_count,out)!=output_count)return 1;
  fclose(out); fprintf(stderr,"channels=%u rate=%u samples=%zu nodes=%zu\n",id.channels,id.sample_rate,output_count,node_count);
  return 0;
}

#include "vorbis_setup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t packet[4096], lengths[256];
static struct capy_vorbis_setup_workspace workspace;
static size_t position, time_at, floor_at, residue_at, mapping_at, mode_at, frame_at;
static size_t floor_book_at, point_at, classbook_at, stagebook_at, angle_at, mux_at, mode_mapping_at;
static void put(uint32_t value, unsigned bits) {
  assert(position + bits <= sizeof(packet) * 8u);
  for (unsigned i = 0; i < bits; ++i, ++position)
    packet[position/8] |= (uint8_t)(((value >> i) & 1u) << (position % 8));
}
static void fixture(unsigned books, unsigned floor_type, unsigned residue_type) {
  memset(packet, 0, sizeof(packet)); position = 0;
  const uint8_t signature[] = {5,'v','o','r','b','i','s'};
  for (unsigned i = 0; i < sizeof(signature); ++i) put(signature[i], 8);
  put(books-1, 8);
  for (unsigned i = 0; i < books; ++i) {
    put(0x564342,24); put(1,16); put(1,24);
    put(0,1); put(0,1); put(0,5); /* one active length-one code */
    put(1,4); put(0,32); put(0,32); put(0,4); put(0,1); put(0,1);
  }
  put(0,6); time_at=position; put(0,16);
  put(0,6); floor_at=position; put(floor_type,16);
  if (floor_type == 0) {
    put(1,8); put(48000,16); put(16,16); put(6,6); put(60,8);
    put(0,4); floor_book_at=position; put(0,8);
  } else {
    put(1,5); put(0,4); /* one partition, class zero */
    put(0,3); put(1,2); floor_book_at=position; put(0,8);
    put(0,8); put(1,8); /* unused and book zero subclass */
    put(0,2); put(6,4); point_at=position; put(32,6);
  }
  put(0,6); residue_at=position; put(residue_type,16);
  put(0,24); put(64,24); put(0,24); put(0,6); classbook_at=position; put(0,8);
  put(1,3); put(0,1); stagebook_at=position; put(0,8);
  put(0,6); mapping_at=position; put(0,16);
  put(1,1); put(1,4); /* two submaps */
  put(1,1); put(0,8); put(0,1); angle_at=position; put(1,1);
  put(0,2); mux_at=position; put(0,4); put(1,4);
  for (unsigned i=0;i<2;++i) { put(77,8); put(0,8); put(0,8); }
  put(0,6); mode_at=position; put(0,1); put(0,16); put(0,16); mode_mapping_at=position; put(0,8);
  frame_at=position; put(1,1);
}
static int check(size_t size, struct capy_vorbis_setup_limits limits) {
  struct capy_vorbis_setup_summary summary;
  int rc = capy_vorbis_setup_validate(packet,size,2,&limits,&workspace,
                                     lengths,sizeof(lengths),&summary);
  if (rc) assert(!summary.books && !summary.consumed_bits);
  else assert(summary.books && summary.floors && summary.floors <= 64 &&
              summary.residues && summary.residues <= 64 &&
              summary.mappings && summary.mappings <= 64 &&
              summary.modes && summary.modes <= 64 &&
              summary.consumed_bits <= size * 8u);
  return rc;
}
static void check_retained(unsigned floor_type, unsigned residue_type) {
  assert(workspace.floor_type[0] == floor_type);
  if (floor_type == 0) {
    assert(workspace.floor0[0].order == 1);
    assert(workspace.floor0[0].rate == 48000);
    assert(workspace.floor0[0].bark_map_size == 16);
    assert(workspace.floor0[0].amplitude_bits == 6);
    assert(workspace.floor0[0].amplitude_offset == 60);
    assert(workspace.floor0[0].book_count == 1);
    assert(workspace.floor0[0].books[0] == 0);
  } else {
    assert(workspace.floor1[0].partitions == 1);
    assert(workspace.floor1[0].plan.count == 3);
    assert(workspace.floor1[0].plan.x[2] == 32);
  }
  const struct capy_vorbis_residue_config *residue = &workspace.residues[0];
  assert(residue->type == residue_type && residue->begin == 0);
  assert(residue->end == 64 && residue->partition_size == 1);
  assert(residue->classifications == 1 && residue->classbook == 0);
  assert(residue->cascade[0] == 1 && residue->books[0][0] == 0);
  assert(residue->books[0][1] == -1 && residue->books[63][7] == -1);
  const struct capy_vorbis_mapping *mapping = &workspace.mappings[0];
  assert(mapping->submaps == 2 && mapping->coupling_steps == 1);
  assert(mapping->magnitude[0] == 0 && mapping->angle[0] == 1);
  assert(mapping->mux[0] == 0 && mapping->mux[1] == 1);
  assert(mapping->floor[0] == 0 && mapping->floor[1] == 0);
  assert(mapping->residue[0] == 0 && mapping->residue[1] == 0);
  assert(workspace.modes[0].blockflag == 0 && workspace.modes[0].mapping == 0);
}
int main(void) {
  struct capy_vorbis_setup_limits limits = {4096,256,256,8192,{256,32,8192}};
  for (unsigned floor = 0; floor <= 1; ++floor)
    for (unsigned residue = 0; residue <= 2; ++residue) {
      fixture(2,floor,residue);
      size_t size = (position + 7u) / 8u;
      assert(check(size,limits) == 0);
      check_retained(floor,residue);
      for (size_t n=0;n<size;++n) assert(check(n,limits) < 0);
      const size_t invalid[] = {time_at,floor_at+1,residue_at+2,mapping_at,mode_at+1,frame_at,
        floor_book_at+1,classbook_at+1,stagebook_at+1,angle_at,mux_at+1,mode_mapping_at,
        residue_at+16+7};
      for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        size_t bit=invalid[i]; packet[bit/8] ^= (uint8_t)(1u << (bit%8));
        assert(check(size,limits) < 0);
        packet[bit/8] ^= (uint8_t)(1u << (bit%8));
      }
      if (floor == 1) {
        size_t bit=point_at+5;
        packet[bit/8] ^= (uint8_t)(1u << (bit%8));
        assert(check(size,limits) == CAPY_AUDIO_ERR_CORRUPT_DATA);
        packet[bit/8] ^= (uint8_t)(1u << (bit%8));
      }
      limits.max_total_entries=1;
      assert(check(size,limits) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
      limits.max_total_entries=256; limits.max_total_lookup_values=1;
      assert(check(size,limits) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
      limits.max_total_lookup_values=8192; limits.max_books=1;
      assert(check(size,limits) == CAPY_AUDIO_ERR_RESOURCE_LIMIT);
      limits.max_books=256;
    }
  fixture(2,1,0);
  size_t size=(position+7u)/8u;
  struct capy_vorbis_huffman trees[2];
  struct capy_vorbis_huffman_node nodes[2];
  struct capy_vorbis_setup_summary prepared;
  size_t node_count=99;
  assert(capy_vorbis_setup_prepare(packet,size,2,&limits,&workspace,
      lengths,sizeof(lengths),trees,2,nodes,2,&node_count,&prepared)==0);
  assert(prepared.books==2 && node_count==2);
  for (unsigned i=0;i<2;++i) {
    uint8_t bit=0; uint32_t symbol=UINT32_MAX;
    struct capy_vorbis_bits reader;
    assert(trees[i].nodes==&nodes[i] && trees[i].count==1);
    assert(capy_vorbis_bits_init(&reader,&bit,1,1)==0);
    assert(capy_vorbis_huffman_decode(&trees[i],&reader,&symbol)==0);
    assert(symbol==0);
  }
  node_count=99;
  assert(capy_vorbis_setup_prepare(packet,size,2,&limits,&workspace,
      lengths,sizeof(lengths),trees,2,nodes,1,&node_count,&prepared)==
      CAPY_AUDIO_ERR_RESOURCE_LIMIT);
  assert(node_count==0 && prepared.books==0);
  uint32_t seed=0x9ba24c3u;
  for (unsigned i=0;i<10000;++i) {
    seed=seed*1664525u+1013904223u;
    size_t at=seed%size;
    uint8_t old=packet[at]; packet[at]^=(uint8_t)(1u+(seed>>24));
    (void)check(size,limits);
    packet[at]=old;
  }
  puts("[vorbis-setup] sections/references/aggregate limits/truncation/mutations passed");
  return 0;
}

#include "vorbis_decode.h"
#include "ogg_reader.h"
#include "vorbis_audio_packet.h"
#include "vorbis_headers.h"
#include "vorbis_synthesis.h"

#define PACKET_CAP (1u << 20)
#define NODE_CAP 524288u
#define WORK_CAP (16u << 20)
#define PACKETS_CAP 65536u
#define VECTORS_CAP UINT64_C(16000000)

struct scratch {
  const struct capy_audio_allocator *allocator;
  void *items[24];
  unsigned count;
  size_t remaining;
  int error;
};

static void *reserve(struct scratch *s, size_t bytes) {
  if (!bytes || bytes > s->remaining || s->count >= 24) {
    s->error = CAPY_AUDIO_ERR_RESOURCE_LIMIT;
    return 0;
  }
  void *p = s->allocator->alloc(bytes, s->allocator->user_data);
  if (!p) { s->error = CAPY_AUDIO_ERR_OUT_OF_MEMORY; return 0; }
  s->items[s->count++] = p;
  s->remaining -= bytes;
  for (size_t i = 0; i < bytes; ++i) ((uint8_t *)p)[i] = 0;
  return p;
}

static int packet_next(struct capy_ogg_reader *ogg, uint8_t *packet, size_t *n) {
  int rc = capy_ogg_reader_next(ogg, packet, PACKET_CAP, n);
  return rc == 1 ? 0 : rc < 0 ? rc : CAPY_AUDIO_ERR_TRUNCATED_DATA;
}

int capy_vorbis_decode_memory(const uint8_t *data, size_t size,
    const struct capy_audio_allocator *allocator,
    const struct capy_audio_limits *limits,
    const struct capy_vorbis_decode_budget *budget, struct capy_audio_pcm *out) {
  if (!out) return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  *out = (struct capy_audio_pcm){0};
  if (!data || !allocator || !allocator->alloc || !allocator->free ||
      !limits || !limits->max_input_bytes || !limits->max_output_bytes ||
      !limits->max_frames || !limits->max_channels || !limits->max_sample_rate ||
      !limits->max_chunks || !budget || !budget->max_work_bytes ||
      !budget->max_packets || !budget->max_vectors)
    return CAPY_AUDIO_ERR_INVALID_ARGUMENT;
  if (size > limits->max_input_bytes || size > CAPY_AUDIO_MAX_OUTPUT_BYTES)
    return CAPY_AUDIO_ERR_RESOURCE_LIMIT;
  struct scratch s = {.allocator = allocator,
    .remaining = budget->max_work_bytes < WORK_CAP ? budget->max_work_bytes : WORK_CAP};
  struct capy_ogg_reader ogg, audio_start;
  struct capy_vorbis_identification id;
  struct capy_vorbis_comments comments;
  struct capy_vorbis_setup_summary summary;
  struct capy_vorbis_synthesis_state state = {0};
  struct capy_vorbis_mdct_plan plans[2];
  uint8_t *output = 0;
  uint8_t *packet = reserve(&s, PACKET_CAP);
  size_t packet_size = 0, setup_size = 0, node_count = 0;
  uint64_t final_frames = UINT64_MAX, produced = 0;
  uint32_t packets = 0;
  uint32_t packet_limit = budget->max_packets < PACKETS_CAP ? budget->max_packets : PACKETS_CAP;
  uint64_t vectors = budget->max_vectors < VECTORS_CAP ? budget->max_vectors : VECTORS_CAP;
  int rc = CAPY_AUDIO_ERR_OUT_OF_MEMORY;
#define TRY(call) do { rc = (call); if (rc) goto done; } while (0)
#define ALLOC(type, name, count) \
  type *name = reserve(&s, (size_t)(count) * sizeof(type)); \
  if (!name) { rc = s.error; goto done; }
  if (!packet) { rc = s.error; goto done; }
  TRY(capy_ogg_reader_init(&ogg, data, size, limits->max_input_bytes,
                          PACKET_CAP, limits->max_chunks));
  TRY(packet_next(&ogg, packet, &packet_size));
  TRY(capy_vorbis_parse_identification(packet, packet_size, limits, &id));
  TRY(packet_next(&ogg, packet, &packet_size));
  TRY(capy_vorbis_parse_comments(packet, packet_size, limits, &comments));
  TRY(packet_next(&ogg, packet, &packet_size));
  setup_size = packet_size;
  ALLOC(uint8_t, setup_packet, setup_size);
  for (size_t i = 0; i < setup_size; ++i) setup_packet[i] = packet[i];
  audio_start = ogg;
  /* Validate the entire container and obtain a bounded exact output allocation
   * before entropy work. Chained/multiplexed or missing-EOS streams fail closed. */
  while ((rc = capy_ogg_reader_next(&ogg, packet, PACKET_CAP, &packet_size)) == 1) {
    if (++packets > packet_limit) { rc = CAPY_AUDIO_ERR_RESOURCE_LIMIT; goto done; }
    if (ogg.packet_has_granule) {
      if (final_frames != UINT64_MAX && ogg.packet_granule < final_frames) {
        rc = CAPY_AUDIO_ERR_CORRUPT_DATA; goto done;
      }
      final_frames = ogg.packet_granule;
    }
  }
  if (rc < 0) goto done;
  if (!packets || final_frames == UINT64_MAX || !final_frames ||
      ogg.page_granule == UINT64_MAX || ogg.page_granule != final_frames) {
    rc = CAPY_AUDIO_ERR_CORRUPT_DATA; goto done;
  }
  if (final_frames > limits->max_frames ||
      final_frames > limits->max_output_bytes / (id.channels * 2u) ||
      final_frames > CAPY_AUDIO_MAX_OUTPUT_BYTES / (id.channels * 2u)) {
    rc = CAPY_AUDIO_ERR_RESOURCE_LIMIT; goto done;
  }
  ALLOC(struct capy_vorbis_setup_workspace, setup, 1);
  ALLOC(struct capy_vorbis_huffman, trees, 256);
  ALLOC(struct capy_vorbis_huffman_node, nodes, NODE_CAP);
  ALLOC(uint8_t, lengths, 65536);
  const struct capy_vorbis_setup_limits setup_limits =
    {PACKET_CAP,256,262144,1048576,{65536,64,1048576}};
  TRY(capy_vorbis_setup_prepare(setup_packet, setup_size, id.channels,
      &setup_limits, setup, lengths, 65536, trees, 256, nodes, NODE_CAP,
      &node_count, &summary));
  unsigned n = id.blocksize_large;
  size_t total = (size_t)id.channels * n / 2u;
  ALLOC(float, small_trig, id.blocksize_small + id.blocksize_small / 4u);
  ALLOC(int32_t, small_bitrev, id.blocksize_small / 4u);
  ALLOC(float, large_trig, n + n / 4u);
  ALLOC(int32_t, large_bitrev, n / 4u);
  TRY(capy_vorbis_mdct_plan_init(id.blocksize_small, small_trig,
      id.blocksize_small + id.blocksize_small / 4u, small_bitrev,
      id.blocksize_small / 4u, &plans[0]));
  TRY(capy_vorbis_mdct_plan_init(n, large_trig, n+n/4u, large_bitrev, n/4u, &plans[1]));
  ALLOC(float, spectrum, total);
  ALLOC(float, packet_work, 5u * total);
  ALLOC(uint8_t, classes, total);
  ALLOC(struct capy_vorbis_floor1_values, floors, id.channels);
  ALLOC(struct capy_vorbis_floor1_values, floor_scratch, id.channels);
  ALLOC(float, previous, (size_t)id.channels * CAPY_VORBIS_BLOCK_SAMPLES);
  ALLOC(float, pcm, (size_t)id.channels * n);
  ALLOC(float, pcm_scratch, (size_t)id.channels * n);
  ALLOC(float, synth_work, ((size_t)id.channels + 3u) * n);
  ALLOC(uint8_t, curve, n / 2u);
  size_t output_bytes = (size_t)final_frames * id.channels * 2u;
  output = allocator->alloc(output_bytes, allocator->user_data);
  if (!output) { rc = CAPY_AUDIO_ERR_OUT_OF_MEMORY; goto done; }
  ogg = audio_start;
  while ((rc = capy_ogg_reader_next(&ogg, packet, PACKET_CAP, &packet_size)) == 1) {
    struct capy_vorbis_audio_packet_result decoded;
    struct capy_vorbis_synthesis_result made;
    if (!vectors) { rc = CAPY_AUDIO_ERR_RESOURCE_LIMIT; goto done; }
    uint32_t packet_vectors = vectors > 1000000u ? 1000000u : (uint32_t)vectors;
    TRY(capy_vorbis_audio_packet_decode(packet, packet_size, PACKET_CAP,
        setup_packet, setup_size, &summary, setup, trees, summary.books,
        id.channels, id.blocksize_small, n, packet_vectors, 65536.0f,
        spectrum, total, floors, floor_scratch, packet_work, 5u*total,
        classes, total, &decoded));
    if (decoded.vectors > vectors) { rc = CAPY_AUDIO_ERR_RESOURCE_LIMIT; goto done; }
    vectors -= decoded.vectors;
    TRY(capy_vorbis_synthesis_finish(&summary, setup, &decoded.window,
        &plans[decoded.window.blockflag], floors, id.channels, 65536.0f,
        spectrum, total, previous, (size_t)id.channels*CAPY_VORBIS_BLOCK_SAMPLES,
        pcm, (size_t)id.channels*n, pcm_scratch, (size_t)id.channels*n,
        synth_work, ((size_t)id.channels+3u)*n, curve, n/2u, &state, &made));
    uint64_t available = produced < final_frames ? final_frames - produced : 0;
    size_t frames = made.frames < available ? made.frames : (size_t)available;
    for (size_t i = 0; i < frames * id.channels; ++i) {
      float value = pcm[i];
      /* Synthesis rejects non-finite intermediates. Clamp before conversion. */
      int32_t sample = value >= 1.0f ? 32767 : value <= -1.0f ? -32768 :
                       (int32_t)(value * 32768.0f);
      size_t at = ((size_t)produced * id.channels + i) * 2u;
      output[at] = (uint8_t)sample;
      output[at+1u] = (uint8_t)((uint32_t)sample >> 8);
    }
    produced += made.frames;
  }
  if (rc < 0) goto done;
  if (produced < final_frames || produced - final_frames > n) {
    rc = CAPY_AUDIO_ERR_CORRUPT_DATA; goto done;
  }
  out->metadata = (struct capy_audio_metadata){
    .container = CAPY_AUDIO_CONTAINER_OGG_VORBIS,
    .sample_format = CAPY_AUDIO_SAMPLE_S16_LE, .sample_rate = id.sample_rate,
    .channels = id.channels, .bits_per_sample = 16, .block_align = id.channels*2u,
    .frame_count = final_frames, .pcm_bytes = output_bytes};
  out->samples = output;
  out->allocator = *allocator;
  output = 0;
  rc = 0;
done:
  if (output) allocator->free(output, allocator->user_data);
  while (s.count) allocator->free(s.items[--s.count], allocator->user_data);
  return rc;
#undef TRY
#undef ALLOC
}

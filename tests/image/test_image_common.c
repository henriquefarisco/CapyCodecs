#include "test_image_common.h"

void test_heap_reset(struct test_heap *heap) {
  if (!heap) {
    return;
  }
  for (size_t i = 0; i < sizeof(heap->storage); ++i) {
    heap->storage[i] = 0;
  }
  heap->used = 0;
  heap->alloc_calls = 0;
  heap->free_calls = 0;
  heap->fail_after = -1;
}

static void *test_alloc(size_t size, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  size_t aligned;
  void *ptr;
  if (!heap || size == 0u) {
    return 0;
  }
  if (heap->fail_after == 0) {
    return 0;
  }
  if (heap->fail_after > 0) {
    --heap->fail_after;
  }
  aligned = (size + 7u) & ~(size_t)7u;
  if (aligned < size || aligned > sizeof(heap->storage) - heap->used) {
    return 0;
  }
  ptr = heap->storage + heap->used;
  heap->used += aligned;
  ++heap->alloc_calls;
  return ptr;
}

static void test_free(void *ptr, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  if (!heap || !ptr) {
    return;
  }
  ++heap->free_calls;
}

struct capy_image_allocator test_allocator(struct test_heap *heap) {
  struct capy_image_allocator allocator;
  allocator.alloc = test_alloc;
  allocator.free = test_free;
  allocator.user_data = heap;
  return allocator;
}

uint32_t test_hash_pixels_argb32(const uint32_t *pixels, size_t count) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < count; ++i) {
    uint32_t pixel = pixels[i];
    for (int shift = 24; shift >= 0; shift -= 8) {
      hash ^= (pixel >> (uint32_t)shift) & 0xFFu;
      hash *= 16777619u;
    }
  }
  return hash;
}

int test_inflate_rgb_1x1(uint8_t *dest, size_t *dest_len,
                         const uint8_t *source, size_t source_len,
                         void *user_data) {
  struct test_inflater_state *state = (struct test_inflater_state *)user_data;
  (void)source;
  (void)source_len;
  if (!dest || !dest_len || *dest_len < 4u || !state) {
    return -1;
  }
  dest[0] = 0u;
  dest[1] = 0x11u;
  dest[2] = 0x22u;
  dest[3] = 0x33u;
  *dest_len = 4u;
  ++state->calls;
  return 0;
}

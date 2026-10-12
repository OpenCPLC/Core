// .tests/test_heap.c
// Heap: every block aligned, freed neighbours merge back into one, a full heap empties whole.
// An allocation the heap cannot serve panics, which ends the program under its case.

#include "test.h"
#include "heap.c"

static void blocks_stay_aligned(void)
{
  static const size_t sizes[] = { 1, 3, 7, 9, 13, 17, 5, 31, 2, 11, 23, 4, 19, 6, 29, 8 };
  void *blocks[array_len(sizes)];
  heap_init();
  for(uint16_t i = 0; i < array_len(sizes); i++) {
    blocks[i] = heap_alloc(sizes[i]);
    TEST_Check((uintptr_t)blocks[i] % HEAP_ALIGN == 0);
  }
  for(uint16_t i = 0; i < array_len(sizes); i += 2) heap_free(blocks[i]);
  for(uint16_t i = 0; i < array_len(sizes); i += 2) {
    blocks[i] = heap_alloc(sizes[i] + 3);
    TEST_Check((uintptr_t)blocks[i] % HEAP_ALIGN == 0);
  }
}

static void freed_neighbours_merge(void)
{
  heap_init();
  void *first = heap_alloc(64);
  void *second = heap_alloc(64);
  void *third = heap_alloc(64);
  heap_free(first);
  heap_free(second);
  heap_free(third);
  TEST_Check(heap_alloc(180) == first, "three freed blocks merge into one of 180");
}

static void full_heap_empties_whole(void)
{
  heap_init();
  void *large[15];
  for(uint16_t i = 0; i < array_len(large); i++) large[i] = heap_alloc(512);
  for(uint16_t i = 0; i < array_len(large); i++) heap_free(large[i]);
  TEST_Check(heap_alloc(2048) == large[0], "emptied heap holds one block of 2048");
}

void TEST_Cases(void)
{
  TEST(blocks_stay_aligned);
  TEST(freed_neighbours_merge);
  TEST(full_heap_empties_whole);
}

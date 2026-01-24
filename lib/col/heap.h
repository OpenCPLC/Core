#ifndef HEAP_H_
#define HEAP_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef HEAP_SIZE
  #define HEAP_SIZE 8192
#endif

#ifndef HEAP_ALIGN
  #define HEAP_ALIGN 8
#endif

/**
 * @brief Heap memory block structure.
 * @param size Size of the allocated data area (in bytes), not including the header
 * @param next Pointer to next `heap_block_t` in `FreeList`
 * @param free `true` if free, `false` if in use
 */
typedef struct heap_block {
  size_t size;
  struct heap_block  *next;
  bool free;
} heap_block_t;

void heap_init(void);
void *heap_alloc(size_t size);
void *heap_reloc(void *ptr, size_t size);
void heap_free(void *ptr);

//-------------------------------------------------------------------------------------------------

#ifndef HEAP_NEW_BLOCK
  #define HEAP_NEW_BLOCK 16
#endif

/**
 * @brief Memory stack for garbage collector, one per thread.
 * @param var Pointers to dynamically allocated variables
 * @param count Current number of variables in use
 * @param limit Current allocated capacity of `var` (auto-expands)
 */
typedef struct {
  void **var;
  uint16_t count;
  uint16_t limit;
} heap_new_t;

void *heap_new(size_t size);
void heap_clear(void);

//-------------------------------------------------------------------------------------------------
#endif

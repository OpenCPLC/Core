#ifndef ARY_H
#define ARY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------

/**
 * @brief Universal array/ring buffer structure.
 * @param data Pointer to element buffer.
 * @param head Write index (next free slot).
 * @param tail Read index (oldest element).
 * @param count Current number of elements.
 * @param limit Maximum capacity (elements).
 * @param element_size Size of single element (bytes).
 * @param overwrite When true and full, push overwrites oldest.
 */
typedef struct {
  void *data;
  uint16_t head;
  uint16_t tail;
  uint16_t count;
  uint16_t limit;
  uint16_t element_size;
  bool overwrite;
} ary_t;

//-------------------------------------------------------------------------------------------------

#define ary_new(Name, Type, Limit) \
  Type Name##_data[Limit]; \
  ary_t Name = { .data = Name##_data, .head = 0, .tail = 0, .count = 0, \
    .limit = (Limit), .element_size = sizeof(Type), .overwrite = false }

#define ary_ring(Name, Type, Limit) \
  Type Name##_data[Limit]; \
  ary_t Name = { .data = Name##_data, .head = 0, .tail = 0, .count = 0, \
    .limit = (Limit), .element_size = sizeof(Type), .overwrite = true }

#define ary_init(Name, Type, Limit, ...) \
  Type Name##_data[Limit] = __VA_ARGS__; \
  ary_t Name = { .data = Name##_data, .head = (uint16_t)(sizeof((Type[])__VA_ARGS__) / sizeof(Type)), \
    .tail = 0, .count = (uint16_t)(sizeof((Type[])__VA_ARGS__) / sizeof(Type)), \
    .limit = (Limit), .element_size = sizeof(Type), .overwrite = false }

#define ary_for(Idx, Ary) \
  for(uint16_t Idx = 0; Idx < (Ary)->count; Idx++)

//-------------------------------------------------------------------------------------------------

bool ary_push(ary_t *ary, const void *element);
bool ary_pop(ary_t *ary, void *out);
bool ary_shift(ary_t *ary, void *out);
bool ary_unshift(ary_t *ary, const void *element);
bool ary_peek(const ary_t *ary, void *out);
bool ary_peek_first(const ary_t *ary, void *out);
void *ary_get(const ary_t *ary, uint16_t index);
bool ary_set(ary_t *ary, uint16_t index, const void *element);
bool ary_insert(ary_t *ary, uint16_t index, const void *element);
bool ary_remove(ary_t *ary, uint16_t index, void *out);
bool ary_swap(ary_t *ary, uint16_t i, uint16_t j);
void ary_clear(ary_t *ary);
bool ary_empty(const ary_t *ary);
bool ary_full(const ary_t *ary);
uint16_t ary_free(const ary_t *ary);

//-------------------------------------------------------------------------------------------------
#endif
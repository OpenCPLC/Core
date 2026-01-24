#ifndef QUEUE_H
#define QUEUE_H

#include "ary.h"

//-------------------------------------------------------------------------------------------------

#ifndef QUEUE_USE_HEAP
  #define QUEUE_USE_HEAP 0
#endif

/**
 * @brief Priority queue with optional unique filtering.
 * @param ary Storage backend.
 * @param unique When true, reject duplicates.
 * @param invert When true, pop/peek from tail (largest) instead of head (smallest).
 * @param Equal Equality predicate for unique check. NULL = memcmp.
 * @param Compare Sorting function. NULL = FIFO order.
 */
typedef struct {
  ary_t ary;
  bool unique;
  bool invert;
  bool (*Equal)(const void *a, const void *b);
  int (*Compare)(const void *a, const void *b);
} QUEUE_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Declare queue with storage.
 * @param Name Variable name.
 * @param Type Element type.
 * @param Limit Capacity.
 */
#define QUEUE_New(Name, Type, Limit) \
  Type Name##_data[Limit]; \
  QUEUE_t Name = { \
    .ary = { .data = Name##_data, .head = 0, .tail = 0, .count = 0, \
      .limit = (Limit), .element_size = sizeof(Type), .overwrite = false }, \
    .unique = false, .invert = false, .Equal = NULL, .Compare = NULL }

//-------------------------------------------------------------------------------------------------

bool QUEUE_Push(QUEUE_t *queue, const void *element);
bool QUEUE_Pop(QUEUE_t *queue, void *element);
bool QUEUE_Peek(const QUEUE_t *queue, void *element);
bool QUEUE_IsEmpty(const QUEUE_t *queue);
bool QUEUE_IsFull(const QUEUE_t *queue);
uint16_t QUEUE_Count(const QUEUE_t *queue);
void QUEUE_Clear(QUEUE_t *queue);
int16_t QUEUE_Find(QUEUE_t *queue, const void *key);
bool QUEUE_Remove(QUEUE_t *queue, const void *key, void *out);
bool QUEUE_RemoveAt(QUEUE_t *queue, uint16_t index, void *out);
uint16_t QUEUE_RemoveAll(QUEUE_t *queue, bool (*Match)(const void*, void*), void *ctx);

//-------------------------------------------------------------------------------------------------
#endif
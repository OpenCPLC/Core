// lib/col/queue.h

#ifndef QUEUE_H_
#define QUEUE_H_

#include "ary.h"

//------------------------------------------------------------------------------------------ Config

#ifndef QUEUE_USE_HEAP
  // Ordering: `0` insertion sort, `O(n)` per push; `1` binary heap, `O(log n)`, for large queues
  #define QUEUE_USE_HEAP 0
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Priority queue over an `ary_t`, optionally rejecting duplicates.
 * @param[in] ary Storage backend
 * @param[in] unique When `true` a duplicate push is refused
 * @param[in] invert When `true` pop and peek take the largest instead of the smallest
 * @param[in] Equal Equality predicate for the unique check, `NULL` = `memcmp`
 * @param[in] Compare Ordering function, `NULL` = FIFO
 */
typedef struct {
  ary_t ary;
  bool unique;
  bool invert;
  bool (*Equal)(const void *a, const void *b);
  int (*Compare)(const void *a, const void *b);
} QUEUE_t;

/**
 * @brief Declare a queue with its storage.
 * @param name Variable name
 * @param type Element type
 * @param capacity Maximum elements
 * @param equal Equality function, `NULL` = duplicates allowed
 * @param compare Ordering function, `NULL` = FIFO
 */
#define QUEUE_New(name, type, capacity, equal, compare) \
  type name##_data[capacity]; \
  QUEUE_t name = { \
    .ary = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type) }, \
    .unique = ((equal) != NULL), .Equal = (equal), .Compare = (compare) }

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Push element.
 * @param[in,out] queue Queue
 * @param[in] element Element to add
 * @return `true` if added, `false` if full or a duplicate
 */
bool QUEUE_Push(QUEUE_t *queue, const void *element);

/**
 * @brief Pop the highest priority element.
 * @param[in,out] queue Queue
 * @param[out] element Destination, `NULL` = discard
 * @return `true` if popped, `false` if empty
 */
bool QUEUE_Pop(QUEUE_t *queue, void *element);

/**
 * @brief Copy the highest priority element without removing it.
 * @param[in] queue Queue
 * @param[out] element Destination
 * @return `true` if copied, `false` if empty
 */
bool QUEUE_Peek(const QUEUE_t *queue, void *element);

/**
 * @brief Find the element equal to `key`.
 * @param[in] queue Queue
 * @param[in] key Element to match through `Equal`
 * @return Index, `-1` when absent
 */
int16_t QUEUE_Find(QUEUE_t *queue, const void *key);

/**
 * @brief Remove the element equal to `key`.
 * @param[in,out] queue Queue
 * @param[in] key Element to match through `Equal`
 * @param[out] out Removed element, `NULL` = discard
 * @return `true` if removed
 */
bool QUEUE_Remove(QUEUE_t *queue, const void *key, void *out);

/**
 * @brief Remove the element at `index`.
 * @param[in,out] queue Queue
 * @param[in] index Element index
 * @param[out] out Removed element, `NULL` = discard
 * @return `true` if removed
 */
bool QUEUE_RemoveAt(QUEUE_t *queue, uint16_t index, void *out);

/**
 * @brief Remove every element the predicate accepts.
 * @param[in,out] queue Queue
 * @param[in] Match Predicate called with the element and `ctx`
 * @param[in] ctx User context
 * @return Number removed
 */
uint16_t QUEUE_RemoveAll(QUEUE_t *queue, bool (*Match)(const void *, void *), void *ctx);

bool QUEUE_IsEmpty(const QUEUE_t *queue);
bool QUEUE_IsFull(const QUEUE_t *queue);
uint16_t QUEUE_Count(const QUEUE_t *queue);

// Drop every element
void QUEUE_Clear(QUEUE_t *queue);

//-------------------------------------------------------------------------------------------------
#endif

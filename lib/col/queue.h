/** @file lib/col/queue.h */

#ifndef QUEUE_H_
#define QUEUE_H_

#include "ary.h"

//-------------------------------------------------------------------------------------------------

#ifndef QUEUE_USE_HEAP
/**
 * @brief Heap mode for priority queue.
 * `0` - insertion sort O(n), simple, default.
 * `1` - binary heap O(log n), for large queues.
 */
#define QUEUE_USE_HEAP 0
#endif

/**
 * @brief Priority queue with optional unique filtering.
 * @param[in] ary Storage backend.
 * @param[in] unique When `true`, reject duplicates.
 * @param[in] invert When `true`, pop/peek largest instead of smallest.
 * @param[in] Equal Equality predicate for unique check (`NULL` = memcmp).
 * @param[in] Compare Sorting function (`NULL` = FIFO order).
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
 * @brief Declare queue.
 * @param name Variable name.
 * @param type Element type.
 * @param capacity Maximum elements.
 * @param equal Equality function (`NULL` = allow duplicates).
 * @param compare Comparison function (`NULL` = FIFO order).
 */
#define QUEUE_New(name, type, capacity, equal, compare) \
  type name##_data[capacity]; \
  QUEUE_t name = { \
    .ary = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type) }, \
    .unique = ((equal) != NULL), .Equal = (equal), .Compare = (compare) }

//-------------------------------------------------------------------------------------------------

/**
 * @brief Push element to queue.
 * @param[in,out] queue Queue.
 * @param[in] element Element to add.
 * @return `true` if added, `false` if full or duplicate.
 */
bool QUEUE_Push(QUEUE_t *queue, const void *element);

/**
 * @brief Pop highest priority element.
 * @param[in,out] queue Queue.
 * @param[out] element Destination or `NULL`.
 * @return `true` if popped, `false` if empty.
 */
bool QUEUE_Pop(QUEUE_t *queue, void *element);

/**
 * @brief Peek highest priority element without removing.
 * @param[in] queue Queue.
 * @param[out] element Destination.
 * @return `true` if peeked, `false` if empty.
 */
bool QUEUE_Peek(const QUEUE_t *queue, void *element);

/**
 * @brief Find element matching key via `Equal` function.
 * @param[in] queue Queue.
 * @param[in] key Element to match.
 * @return Index if found, `-1` otherwise.
 */
int16_t QUEUE_Find(QUEUE_t *queue, const void *key);

/**
 * @brief Remove element matching key.
 * @param[in,out] queue Queue.
 * @param[in] key Element to match.
 * @param[out] out Removed element or `NULL`.
 * @return `true` if removed.
 */
bool QUEUE_Remove(QUEUE_t *queue, const void *key, void *out);

/**
 * @brief Remove element at index.
 * @param[in,out] queue Queue.
 * @param[in] index Index.
 * @param[out] out Removed element or `NULL`.
 * @return `true` if removed.
 */
bool QUEUE_RemoveAt(QUEUE_t *queue, uint16_t index, void *out);

/**
 * @brief Remove all elements matching predicate.
 * @param[in,out] queue Queue.
 * @param[in] Match Predicate (`element`, `ctx`) → `bool`.
 * @param[in] ctx User context.
 * @return Number removed.
 */
uint16_t QUEUE_RemoveAll(QUEUE_t *queue, bool (*Match)(const void *, void *), void *ctx);

/**
 * @brief Check if queue is empty.
 * @param[in] queue Queue.
 * @return `true` if empty.
 */
bool QUEUE_IsEmpty(const QUEUE_t *queue);

/**
 * @brief Check if queue is full.
 * @param[in] queue Queue.
 * @return `true` if full.
 */
bool QUEUE_IsFull(const QUEUE_t *queue);

/**
 * @brief Get element count.
 * @param[in] queue Queue.
 * @return Number of elements.
 */
uint16_t QUEUE_Count(const QUEUE_t *queue);

/**
 * @brief Clear queue.
 * @param[in,out] queue Queue.
 */
void QUEUE_Clear(QUEUE_t *queue);

//-------------------------------------------------------------------------------------------------
#endif
#include "queue.h"

//-------------------------------------------------------------------------------------------------

#if QUEUE_USE_HEAP

static void heap_sift_up(QUEUE_t *q, uint16_t i)
{
  while(i > 0) {
    uint16_t p = (i - 1) / 2;
    if(q->Compare(ary_get(&q->ary, i), ary_get(&q->ary, p)) >= 0) break;
    ary_swap(&q->ary, i, p);
    i = p;
  }
}

static void heap_sift_down(QUEUE_t *q, uint16_t i)
{
  uint16_t n = q->ary.count;
  while(1) {
    uint16_t sm = i, l = 2*i+1, r = 2*i+2;
    if(l < n && q->Compare(ary_get(&q->ary, l), ary_get(&q->ary, sm)) < 0) sm = l;
    if(r < n && q->Compare(ary_get(&q->ary, r), ary_get(&q->ary, sm)) < 0) sm = r;
    if(sm == i) break;
    ary_swap(&q->ary, i, sm);
    i = sm;
  }
}

#endif

//-------------------------------------------------------------------------------------------------

/**
 * @brief Find element matching key via Equal function.
 * @param[in] queue Pointer to QUEUE_t.
 * @param[in] key Element to match.
 * @return Index if found, -1 otherwise.
 */
int16_t QUEUE_Find(QUEUE_t *queue, const void *key)
{
  for(uint16_t i = 0; i < queue->ary.count; i++) {
    const void *item = ary_get(&queue->ary, i);
    bool same = queue->Equal ? queue->Equal(item, key) : memcmp(item, key, queue->ary.element_size) == 0;
    if(same) return (int16_t)i;
  }
  return -1;
}

/**
 * @brief Push element to queue.
 * @param[in,out] queue Pointer to QUEUE_t.
 * @param[in] element Pointer to element.
 * @return true if added, false if full or duplicate.
 */
bool QUEUE_Push(QUEUE_t *queue, const void *element)
{
  if(ary_full(&queue->ary)) return false;
  if(queue->unique && QUEUE_Find(queue, element) >= 0) return false;

#if QUEUE_USE_HEAP
  if(queue->Compare) {
    ary_push(&queue->ary, element);
    heap_sift_up(queue, queue->ary.count - 1);
    return true;
  }
#endif

  if(queue->Compare) {
    // Insertion sort
    uint16_t pos = 0;
    for(; pos < queue->ary.count; pos++) {
      if(queue->Compare(element, ary_get(&queue->ary, pos)) < 0) break;
    }
    ary_insert(&queue->ary, pos, element);
  }
  else {
    ary_push(&queue->ary, element);
  }
  return true;
}

/**
 * @brief Pop element from queue.
 * @param[in,out] queue Pointer to QUEUE_t.
 * @param[out] element Destination (can be NULL).
 * @return true if popped, false if empty.
 */
bool QUEUE_Pop(QUEUE_t *queue, void *element)
{
  if(ary_empty(&queue->ary)) return false;

#if QUEUE_USE_HEAP
  if(queue->Compare) {
    if(queue->invert) {
      // Find max in heap (leaves)
      uint16_t max_i = queue->ary.count / 2;
      for(uint16_t i = max_i + 1; i < queue->ary.count; i++) {
        if(queue->Compare(ary_get(&queue->ary, i), ary_get(&queue->ary, max_i)) > 0) max_i = i;
      }
      return QUEUE_RemoveAt(queue, max_i, element);
    }
    if(element) memcpy(element, ary_get(&queue->ary, 0), queue->ary.element_size);
    ary_set(&queue->ary, 0, ary_get(&queue->ary, queue->ary.count - 1));
    ary_pop(&queue->ary, NULL);
    if(queue->ary.count > 0) heap_sift_down(queue, 0);
    return true;
  }
#endif

  // Sorted ascending: index 0 = smallest, last = largest
  if(queue->invert) {
    return ary_pop(&queue->ary, element);
  }
  else {
    return ary_shift(&queue->ary, element);
  }
}

/**
 * @brief Peek element without removing.
 * @param[in] queue Pointer to QUEUE_t.
 * @param[out] element Destination.
 * @return true if peeked, false if empty.
 */
bool QUEUE_Peek(const QUEUE_t *queue, void *element)
{
  if(ary_empty(&queue->ary)) return false;

#if QUEUE_USE_HEAP
  if(queue->Compare && !queue->invert) {
    memcpy(element, ary_get(&queue->ary, 0), queue->ary.element_size);
    return true;
  }
#endif

  if(queue->invert) {
    return ary_peek(&queue->ary, element);
  }
  else {
    return ary_peek_first(&queue->ary, element);
  }
}

/**
 * @brief Remove element at index.
 * @param[in,out] queue Pointer to QUEUE_t.
 * @param[in] index Index.
 * @param[out] out Destination (can be NULL).
 * @return true if removed.
 */
bool QUEUE_RemoveAt(QUEUE_t *queue, uint16_t index, void *out)
{
  if(index >= queue->ary.count) return false;

#if QUEUE_USE_HEAP
  if(queue->Compare) {
    if(out) memcpy(out, ary_get(&queue->ary, index), queue->ary.element_size);
    ary_set(&queue->ary, index, ary_get(&queue->ary, queue->ary.count - 1));
    ary_pop(&queue->ary, NULL);
    if(index < queue->ary.count) {
      heap_sift_down(queue, index);
      heap_sift_up(queue, index);
    }
    return true;
  }
#endif

  return ary_remove(&queue->ary, index, out);
}

/**
 * @brief Remove element matching key.
 * @param[in,out] queue Pointer to QUEUE_t.
 * @param[in] key Element to match.
 * @param[out] out Destination (can be NULL).
 * @return true if removed.
 */
bool QUEUE_Remove(QUEUE_t *queue, const void *key, void *out)
{
  int16_t idx = QUEUE_Find(queue, key);
  if(idx < 0) return false;
  return QUEUE_RemoveAt(queue, (uint16_t)idx, out);
}

/**
 * @brief Remove all elements matching predicate.
 * @param[in,out] queue Pointer to QUEUE_t.
 * @param[in] Match Predicate (element, ctx) -> bool.
 * @param[in] ctx User context.
 * @return Number removed.
 */
uint16_t QUEUE_RemoveAll(QUEUE_t *queue, bool (*Match)(const void*, void*), void *ctx)
{
  uint16_t removed = 0;
  uint16_t i = 0;
  while(i < queue->ary.count) {
    if(Match(ary_get(&queue->ary, i), ctx)) {
      QUEUE_RemoveAt(queue, i, NULL);
      removed++;
    }
    else {
      i++;
    }
  }
  return removed;
}

bool QUEUE_IsEmpty(const QUEUE_t *queue) { return ary_empty(&queue->ary); }
bool QUEUE_IsFull(const QUEUE_t *queue) { return ary_full(&queue->ary); }
uint16_t QUEUE_Count(const QUEUE_t *queue) { return queue->ary.count; }
void QUEUE_Clear(QUEUE_t *queue) { ary_clear(&queue->ary); }

//-------------------------------------------------------------------------------------------------
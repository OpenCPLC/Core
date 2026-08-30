// lib/col/queue.c

#include "queue.h"

//-------------------------------------------------------------------------------------------------
#if(QUEUE_USE_HEAP)

static void heap_sift_up(QUEUE_t *queue, uint16_t i)
{
  while(i > 0) {
    uint16_t p = (i - 1) / 2;
    if(queue->Compare(ary_get(&queue->ary, i), ary_get(&queue->ary, p)) >= 0) break;
    ary_swap(&queue->ary, i, p);
    i = p;
  }
}

static void heap_sift_down(QUEUE_t *queue, uint16_t i)
{
  uint16_t count = ary_count(&queue->ary);
  while(1) {
    uint16_t left = 2 * i + 1, right = 2 * i + 2, top = i;
    if(left < count &&
      queue->Compare(ary_get(&queue->ary, left), ary_get(&queue->ary, top)) < 0) top = left;
    if(right < count &&
      queue->Compare(ary_get(&queue->ary, right), ary_get(&queue->ary, top)) < 0) top = right;
    if(top == i) break;
    ary_swap(&queue->ary, i, top);
    i = top;
  }
}

#endif
//-------------------------------------------------------------------------------------------------

int16_t QUEUE_Find(QUEUE_t *queue, const void *key)
{
  uint16_t count = ary_count(&queue->ary);
  for(uint16_t i = 0; i < count; i++) {
    const void *item = ary_get(&queue->ary, i);
    bool same = queue->Equal ? queue->Equal(item, key) :
      !memcmp(item, key, queue->ary.element_size);
    if(same) return (int16_t)i;
  }
  return -1;
}

bool QUEUE_Push(QUEUE_t *queue, const void *element)
{
  if(ary_full(&queue->ary)) return false;
  if(queue->unique && QUEUE_Find(queue, element) >= 0) return false;
  #if(QUEUE_USE_HEAP)
  if(queue->Compare) {
    ary_push(&queue->ary, element);
    heap_sift_up(queue, ary_count(&queue->ary) - 1);
    return true;
  }
  #endif
  if(queue->Compare) {
    uint16_t pos = 0, count = ary_count(&queue->ary);
    for(; pos < count; pos++) {
      if(queue->Compare(element, ary_get(&queue->ary, pos)) < 0) break;
    }
    ary_insert(&queue->ary, pos, element);
  }
  else {
    ary_push(&queue->ary, element);
  }
  return true;
}

bool QUEUE_Pop(QUEUE_t *queue, void *element)
{
  if(ary_empty(&queue->ary)) return false;
  #if(QUEUE_USE_HEAP)
  if(queue->Compare) {
    uint16_t count = ary_count(&queue->ary);
    if(queue->invert) {
      uint16_t max_i = count / 2;
      for(uint16_t i = max_i + 1; i < count; i++) {
        if(queue->Compare(ary_get(&queue->ary, i), ary_get(&queue->ary, max_i)) > 0) max_i = i;
      }
      return QUEUE_RemoveAt(queue, max_i, element);
    }
    if(element) memcpy(element, ary_get(&queue->ary, 0), queue->ary.element_size);
    ary_set(&queue->ary, 0, ary_get(&queue->ary, count - 1));
    ary_pop(&queue->ary, NULL);
    if(ary_count(&queue->ary) > 0) heap_sift_down(queue, 0);
    return true;
  }
  #endif
  if(queue->invert) return ary_pop(&queue->ary, element);
  return ary_shift(&queue->ary, element);
}

bool QUEUE_Peek(const QUEUE_t *queue, void *element)
{
  if(ary_empty(&queue->ary)) return false;
  #if(QUEUE_USE_HEAP)
  if(queue->Compare) {
    uint16_t idx = 0;
    if(queue->invert) {
      uint16_t count = ary_count(&queue->ary);
      idx = count / 2;
      for(uint16_t i = idx + 1; i < count; i++) {
        if(queue->Compare(ary_get(&queue->ary, i), ary_get(&queue->ary, idx)) > 0) idx = i;
      }
    }
    memcpy(element, ary_get(&queue->ary, idx), queue->ary.element_size);
    return true;
  }
  #endif
  if(queue->invert) return ary_peek(&queue->ary, element);
  return ary_peek_first(&queue->ary, element);
}

bool QUEUE_RemoveAt(QUEUE_t *queue, uint16_t index, void *out)
{
  uint16_t count = ary_count(&queue->ary);
  if(index >= count) return false;
  #if(QUEUE_USE_HEAP)
  if(queue->Compare) {
    if(out) memcpy(out, ary_get(&queue->ary, index), queue->ary.element_size);
    ary_set(&queue->ary, index, ary_get(&queue->ary, count - 1));
    ary_pop(&queue->ary, NULL);
    if(index < ary_count(&queue->ary)) {
      heap_sift_down(queue, index);
      heap_sift_up(queue, index);
    }
    return true;
  }
  #endif
  return ary_remove(&queue->ary, index, out);
}

bool QUEUE_Remove(QUEUE_t *queue, const void *key, void *out)
{
  int16_t idx = QUEUE_Find(queue, key);
  if(idx < 0) return false;
  return QUEUE_RemoveAt(queue, (uint16_t)idx, out);
}

uint16_t QUEUE_RemoveAll(QUEUE_t *queue, bool (*Match)(const void *, void *), void *ctx)
{
  uint16_t removed = 0, i = 0;
  while(i < ary_count(&queue->ary)) {
    if(Match(ary_get(&queue->ary, i), ctx)) {
      QUEUE_RemoveAt(queue, i, NULL);
      removed++;
    }
    else i++;
  }
  return removed;
}

bool QUEUE_IsEmpty(const QUEUE_t *queue) { return ary_empty(&queue->ary); }
bool QUEUE_IsFull(const QUEUE_t *queue) { return ary_full(&queue->ary); }
uint16_t QUEUE_Count(const QUEUE_t *queue) { return ary_count(&queue->ary); }
void QUEUE_Clear(QUEUE_t *queue) { ary_clear(&queue->ary); }

//-------------------------------------------------------------------------------------------------

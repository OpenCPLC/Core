/** @file lib/col/ary.c */

#include "ary.h"

//-------------------------------------------------------------------------------------------------

static inline uint16_t idx_wrap(uint16_t i, uint16_t limit) { return i % limit; }
static inline uint16_t idx_prev(uint16_t i, uint16_t limit) { return i ? i - 1 : limit - 1; }
static inline void *ary_ptr(const ary_t *ary, uint16_t i) { return (uint8_t *)ary->value + (uint32_t)i * ary->element_size; }

//-------------------------------------------------------------------------------------------------

bool ary_push(ary_t *ary, const void *element)
{
  if(ary->count == ary->limit) {
    if(!ary->overwrite) return false;
    ary->_tail = idx_wrap(ary->_tail + 1, ary->limit);
    ary->count--;
  }
  memcpy(ary_ptr(ary, ary->_head), element, ary->element_size);
  ary->_head = idx_wrap(ary->_head + 1, ary->limit);
  ary->count++;
  return true;
}

bool ary_pop(ary_t *ary, void *out)
{
  if(!ary->count) return false;
  ary->_head = idx_prev(ary->_head, ary->limit);
  if(out) memcpy(out, ary_ptr(ary, ary->_head), ary->element_size);
  ary->count--;
  return true;
}

bool ary_shift(ary_t *ary, void *out)
{
  if(!ary->count) return false;
  if(out) memcpy(out, ary_ptr(ary, ary->_tail), ary->element_size);
  ary->_tail = idx_wrap(ary->_tail + 1, ary->limit);
  ary->count--;
  return true;
}

bool ary_unshift(ary_t *ary, const void *element)
{
  if(ary->count == ary->limit) {
    if(!ary->overwrite) return false;
    ary->_head = idx_prev(ary->_head, ary->limit);
    ary->count--;
  }
  ary->_tail = idx_prev(ary->_tail, ary->limit);
  memcpy(ary_ptr(ary, ary->_tail), element, ary->element_size);
  ary->count++;
  return true;
}

bool ary_peek(const ary_t *ary, void *out)
{
  if(!ary->count) return false;
  memcpy(out, ary_ptr(ary, idx_prev(ary->_head, ary->limit)), ary->element_size);
  return true;
}

bool ary_peek_first(const ary_t *ary, void *out)
{
  if(!ary->count) return false;
  memcpy(out, ary_ptr(ary, ary->_tail), ary->element_size);
  return true;
}

void *ary_get(const ary_t *ary, uint16_t index)
{
  if(index >= ary->count) return NULL;
  return ary_ptr(ary, idx_wrap(ary->_tail + index, ary->limit));
}

bool ary_set(ary_t *ary, uint16_t index, const void *element)
{
  if(index >= ary->count) return false;
  memcpy(ary_ptr(ary, idx_wrap(ary->_tail + index, ary->limit)), element, ary->element_size);
  return true;
}

bool ary_insert(ary_t *ary, uint16_t index, const void *element)
{
  if(index > ary->count) return false;
  if(ary->count == ary->limit && !ary->overwrite) return false;
  if(index == ary->count) return ary_push(ary, element);
  if(index == 0) return ary_unshift(ary, element);
  uint8_t tmp[ary->element_size];
  for(uint16_t i = ary->count; i > index; i--) {
    memcpy(tmp, ary_get(ary, i - 1), ary->element_size);
    if(i == ary->count) ary_push(ary, tmp);
    else ary_set(ary, i, tmp);
  }
  ary_set(ary, index, element);
  return true;
}

bool ary_remove(ary_t *ary, uint16_t index, void *out)
{
  if(index >= ary->count) return false;
  if(out) memcpy(out, ary_get(ary, index), ary->element_size);
  if(index == 0) return ary_shift(ary, NULL), true;
  if(index == ary->count - 1) return ary_pop(ary, NULL), true;
  for(uint16_t i = index; i < ary->count - 1; i++) {
    memcpy(ary_get(ary, i), ary_get(ary, i + 1), ary->element_size);
  }
  ary->_head = idx_prev(ary->_head, ary->limit);
  ary->count--;
  return true;
}

bool ary_swap(ary_t *ary, uint16_t i, uint16_t j)
{
  if(i >= ary->count || j >= ary->count) return false;
  if(i == j) return true;
  uint8_t *a = ary_get(ary, i), *b = ary_get(ary, j);
  for(uint16_t k = 0; k < ary->element_size; k++) {
    uint8_t t = a[k]; a[k] = b[k]; b[k] = t;
  }
  return true;
}

void ary_clear(ary_t *ary) { ary->_head = ary->_tail = ary->count = 0; }
bool ary_empty(const ary_t *ary) { return !ary->count; }
bool ary_full(const ary_t *ary) { return ary->count == ary->limit; }
uint16_t ary_free(const ary_t *ary) { return ary->limit - ary->count; }

uint16_t ary_copy_last(const ary_t *ary, uint16_t count, void *out)
{
  if(!ary->count || !count) return 0;
  if(count > ary->count) count = ary->count;
  uint16_t start = ary->count - count;
  uint8_t *dst = (uint8_t *)out;
  for(uint16_t i = 0; i < count; i++) {
    memcpy(dst, ary_get(ary, start + i), ary->element_size);
    dst += ary->element_size;
  }
  return count;
}

//-------------------------------------------------------------------------------------------------
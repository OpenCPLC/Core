// lib/col/ary.h

#ifndef ARY_H_
#define ARY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Array of fixed-size elements over user memory, optionally a ring.
 * @param[in] value Element storage
 * @param[in] limit Capacity [elements]
 * @param[in] element_size Size of one element [B]
 * @param[in] overwrite When `true` a push on a full array drops the oldest element
 * @param[out] count Elements held
 * Internal:
 * @param _head Write index, next free slot
 * @param _tail Read index, oldest element
 */
typedef struct {
  void *value;
  uint16_t limit;
  uint16_t element_size;
  bool overwrite;
  uint16_t count;
  // internal
  uint16_t _head;
  uint16_t _tail;
} ary_t;

//------------------------------------------------------------------------------------------ Macros

/**
 * @brief Declare an empty array with its storage.
 * @param name Variable name
 * @param type Element type
 * @param capacity Maximum elements
 */
#define ary_new(name, type, capacity) \
  type name##_data[capacity]; \
  ary_t name = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type), \
    .overwrite = false, ._head = 0, ._tail = 0, .count = 0 }

/**
 * @brief Declare a ring buffer, the oldest element is overwritten when full.
 * @param name Variable name
 * @param type Element type
 * @param capacity Maximum elements
 */
#define ary_ring(name, type, capacity) \
  type name##_data[capacity]; \
  ary_t name = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type), \
    .overwrite = true, ._head = 0, ._tail = 0, .count = 0 }

/**
 * @brief Declare an array with initial values.
 *   `_head` holds `(_tail + count) % limit`, so a list filling the array wraps it to `0`.
 * @param name Variable name
 * @param type Element type
 * @param capacity Maximum elements
 * @param ... Initial values in braces, `{1, 2, 3}`
 */
#define ary_init(name, type, capacity, ...) \
  type name##_data[capacity] = __VA_ARGS__; \
  ary_t name = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type), \
    .overwrite = false, \
    ._head = (uint16_t)((sizeof((type[])__VA_ARGS__) / sizeof(type)) % (capacity)), \
    ._tail = 0, .count = (uint16_t)(sizeof((type[])__VA_ARGS__) / sizeof(type)) }

/**
 * @brief Declare a read-only array sized by its initializer.
 * @param name Variable name
 * @param type Element type
 * @param ... Initial values in braces, `{1, 2, 3}`
 */
#define ary_const(name, type, ...) \
  const type name##_data[] = __VA_ARGS__; \
  enum { name##_limit = sizeof(name##_data) / sizeof(type) }; \
  const ary_t name = { .value = (void *)name##_data, .limit = name##_limit, \
    .element_size = sizeof(type), .overwrite = false, \
    ._head = name##_limit, ._tail = 0, .count = name##_limit }

/**
 * @brief Loop over element indices.
 * @param idx Index variable name
 * @param ary Pointer to `ary_t`
 */
#define ary_for(idx, ary) \
  for(uint16_t idx = 0; idx < (ary)->count; idx++)

// Elements held
#define ary_count(ary) ((ary)->count)

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Add element at the end.
 * @param[in,out] ary Array
 * @param[in] element Element to add
 * @return `true` on success, `false` if full and `overwrite` is off
 */
bool ary_push(ary_t *ary, const void *element);

/**
 * @brief Remove the last element.
 * @param[in,out] ary Array
 * @param[out] out Removed element, `NULL` = discard
 * @return `true` on success, `false` if empty
 */
bool ary_pop(ary_t *ary, void *out);

/**
 * @brief Remove the first element.
 * @param[in,out] ary Array
 * @param[out] out Removed element, `NULL` = discard
 * @return `true` on success, `false` if empty
 */
bool ary_shift(ary_t *ary, void *out);

/**
 * @brief Add element at the front.
 * @param[in,out] ary Array
 * @param[in] element Element to add
 * @return `true` on success, `false` if full and `overwrite` is off
 */
bool ary_unshift(ary_t *ary, const void *element);

/**
 * @brief Copy the last element without removing it.
 * @param[in] ary Array
 * @param[out] out Element copy
 * @return `true` on success, `false` if empty
 */
bool ary_peek(const ary_t *ary, void *out);

/**
 * @brief Copy the first element without removing it.
 * @param[in] ary Array
 * @param[out] out Element copy
 * @return `true` on success, `false` if empty
 */
bool ary_peek_first(const ary_t *ary, void *out);

/**
 * @brief Element at `index`.
 * @param[in] ary Array
 * @param[in] index Element index
 * @return Pointer to the element, `NULL` when out of range
 */
void *ary_get(const ary_t *ary, uint16_t index);

/**
 * @brief Overwrite element at `index`.
 * @param[in,out] ary Array
 * @param[in] index Element index
 * @param[in] element New value
 * @return `true` on success, `false` when out of range
 */
bool ary_set(ary_t *ary, uint16_t index, const void *element);

/**
 * @brief Insert element at `index`, the rest moves up.
 * @param[in,out] ary Array
 * @param[in] index Insert position
 * @param[in] element Element to insert
 * @return `true` on success, `false` when out of range or full
 */
bool ary_insert(ary_t *ary, uint16_t index, const void *element);

/**
 * @brief Remove element at `index`, the rest moves down.
 * @param[in,out] ary Array
 * @param[in] index Remove position
 * @param[out] out Removed element, `NULL` = discard
 * @return `true` on success, `false` when out of range
 */
bool ary_remove(ary_t *ary, uint16_t index, void *out);

/**
 * @brief Swap elements `i` and `j`.
 * @param[in,out] ary Array
 * @param[in] i First index
 * @param[in] j Second index
 * @return `true` on success, `false` when out of range
 */
bool ary_swap(ary_t *ary, uint16_t i, uint16_t j);

// Drop every element
void ary_clear(ary_t *ary);

bool ary_empty(const ary_t *ary);
bool ary_full(const ary_t *ary);

// Free slots left
uint16_t ary_free(const ary_t *ary);

/**
 * @brief Copy the newest `count` elements, oldest of them first.
 * @param[in] ary Array
 * @param[in] count Elements requested
 * @param[out] out Output, room for `count` elements
 * @return Elements copied, `count` capped by what the array holds
 */
uint16_t ary_copy_last(const ary_t *ary, uint16_t count, void *out);

//-------------------------------------------------------------------------------------------------
#endif

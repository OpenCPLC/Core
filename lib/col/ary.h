/** @file lib/col/ary.h */

#ifndef ARY_H_
#define ARY_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------

/**
 * @brief Universal array/ring buffer structure.
 * @param[in] value Pointer to element buffer.
 * @param[in] limit Maximum capacity (elements).
 * @param[in] element_size Size of single element (bytes).
 * @param[in] overwrite When `true` and full, push overwrites oldest.
 * @param[in,out] count Current number of elements.
 * Internal:
 * @param _head Write index (next free slot).
 * @param _tail Read index (oldest element).
 */
typedef struct {
  void *value;
  uint16_t limit;
  uint16_t element_size;
  bool overwrite;
  uint16_t count;
  uint16_t _head;
  uint16_t _tail;
} ary_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Declare empty array.
 * @param name Variable name.
 * @param type Element type.
 * @param capacity Maximum elements.
 */
#define ary_new(name, type, capacity) \
  type name##_data[capacity]; \
  ary_t name = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type), \
    .overwrite = false, ._head = 0, ._tail = 0, .count = 0 }

/**
 * @brief Declare ring buffer (overwrites oldest when full).
 * @param name Variable name.
 * @param type Element type.
 * @param capacity Maximum elements.
 */
#define ary_ring(name, type, capacity) \
  type name##_data[capacity]; \
  ary_t name = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type), \
    .overwrite = true, ._head = 0, ._tail = 0, .count = 0 }

/**
 * @brief Declare array with initial values.
 * @param name Variable name.
 * @param type Element type.
 * @param capacity Maximum elements.
 * @param ... Initial values in braces, e.g. `{1, 2, 3}`.
 */
#define ary_init(name, type, capacity, ...) \
  type name##_data[capacity] = __VA_ARGS__; \
  ary_t name = { .value = name##_data, .limit = (capacity), .element_size = sizeof(type), \
    .overwrite = false, ._head = (uint16_t)(sizeof((type[])__VA_ARGS__) / sizeof(type)), \
    ._tail = 0, .count = (uint16_t)(sizeof((type[])__VA_ARGS__) / sizeof(type)) }

/**
 * @brief Declare const array (fixed size, read-only).
 * @param name Variable name.
 * @param type Element type.
 * @param ... Initial values in braces, e.g. `{1, 2, 3}`.
 */
#define ary_const(name, type, ...) \
  const type name##_data[] = __VA_ARGS__; \
  enum { name##_limit = sizeof(name##_data) / sizeof(type) }; \
  const ary_t name = { .value = (void *)name##_data, .limit = name##_limit, \
    .element_size = sizeof(type), .overwrite = false, \
    ._head = name##_limit, ._tail = 0, .count = name##_limit }

/**
 * @brief Iterate over array elements by index.
 * @param idx Index variable name.
 * @param ary Pointer to `ary_t`.
 */
#define ary_for(idx, ary) \
  for(uint16_t idx = 0; idx < (ary)->count; idx++)

/**
 * @brief Get element count.
 * @param ary Pointer to `ary_t`.
 */
#define ary_count(ary) ((ary)->count)

//-------------------------------------------------------------------------------------------------

/**
 * @brief Add element at end (head).
 * @param[in,out] ary Array.
 * @param[in] element Element to add.
 * @return `true` on success, `false` if full and `overwrite` disabled.
 */
bool ary_push(ary_t *ary, const void *element);

/**
 * @brief Remove element from end (head).
 * @param[in,out] ary Array.
 * @param[out] out Removed element or `NULL`.
 * @return `true` on success, `false` if empty.
 */
bool ary_pop(ary_t *ary, void *out);

/**
 * @brief Remove element from front (tail).
 * @param[in,out] ary Array.
 * @param[out] out Removed element or `NULL`.
 * @return `true` on success, `false` if empty.
 */
bool ary_shift(ary_t *ary, void *out);

/**
 * @brief Add element at front (tail).
 * @param[in,out] ary Array.
 * @param[in] element Element to add.
 * @return `true` on success, `false` if full and `overwrite` disabled.
 */
bool ary_unshift(ary_t *ary, const void *element);

/**
 * @brief Read last element without removing.
 * @param[in] ary Array.
 * @param[out] out Element copy.
 * @return `true` on success, `false` if empty.
 */
bool ary_peek(const ary_t *ary, void *out);

/**
 * @brief Read first element without removing.
 * @param[in] ary Array.
 * @param[out] out Element copy.
 * @return `true` on success, `false` if empty.
 */
bool ary_peek_first(const ary_t *ary, void *out);

/**
 * @brief Get pointer to element at `index`.
 * @param[in] ary Array.
 * @param[in] index Element index.
 * @return Pointer to element or `NULL` if out of range.
 */
void *ary_get(const ary_t *ary, uint16_t index);

/**
 * @brief Set element at `index`.
 * @param[in,out] ary Array.
 * @param[in] index Element index.
 * @param[in] element New value.
 * @return `true` on success, `false` if out of range.
 */
bool ary_set(ary_t *ary, uint16_t index, const void *element);

/**
 * @brief Insert element at `index`, shift others right.
 * @param[in,out] ary Array.
 * @param[in] index Insert position.
 * @param[in] element Element to insert.
 * @return `true` on success, `false` if out of range or full.
 */
bool ary_insert(ary_t *ary, uint16_t index, const void *element);

/**
 * @brief Remove element at `index`, shift others left.
 * @param[in,out] ary Array.
 * @param[in] index Remove position.
 * @param[out] out Removed element or `NULL`.
 * @return `true` on success, `false` if out of range.
 */
bool ary_remove(ary_t *ary, uint16_t index, void *out);

/**
 * @brief Swap elements at indices `i` and `j`.
 * @param[in,out] ary Array.
 * @param[in] i First index.
 * @param[in] j Second index.
 * @return `true` on success, `false` if out of range.
 */
bool ary_swap(ary_t *ary, uint16_t i, uint16_t j);

/**
 * @brief Clear array (reset count).
 * @param[in,out] ary Array.
 */
void ary_clear(ary_t *ary);

/**
 * @brief Check if array is empty.
 * @param[in] ary Array.
 * @return `true` if empty.
 */
bool ary_empty(const ary_t *ary);

/**
 * @brief Check if array is full.
 * @param[in] ary Array.
 * @return `true` if full.
 */
bool ary_full(const ary_t *ary);

/**
 * @brief Get remaining free slots.
 * @param[in] ary Array.
 * @return Number of free elements.
 */
uint16_t ary_free(const ary_t *ary);

/**
 * @brief Copy last N elements to output buffer.
 * @param[in] ary Array.
 * @param[in] count Requested number of elements.
 * @param[out] out Output buffer (must fit `count` elements).
 * @return Actual number of elements copied (min of `count` and `ary->count`).
 */
uint16_t ary_copy_last(const ary_t *ary, uint16_t count, void *out);

//-------------------------------------------------------------------------------------------------
#endif
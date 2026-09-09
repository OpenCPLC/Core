// hal/stm32/per/eeprom.h

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdbool.h>
#include <stdint.h>
#include "flash.h"
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------- Types

// What a storage half holds, the recovery decision of `EEPROM_Init` reads it
typedef enum {
  EEPROM_State_None,
  EEPROM_State_Empty,      // every slot erased
  EEPROM_State_Filled,     // data, no marker, no BEGIN: the active half mid-write
  EEPROM_State_InProgress, // BEGIN at slot 0, no marker: a rewrite target mid-copy
  EEPROM_State_Full,       // every data slot written, marker erased, no BEGIN
  EEPROM_State_Complete    // marker present: a rewritten half carrying its generation
} EEPROM_State_t;

typedef enum {
  EEPROM_Storage_A = 0,
  EEPROM_Storage_B = 1
} EEPROM_Storage_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Key-value store over two flash halves, A and B, appended one 8-byte slot at a time.
 *   A full half is rewritten into the other with one slot per key. The last slot of each
 *   half is a marker with a generation counter, the newer half wins on `Complete/Complete`.
 *   The first slot of a rewrite target takes a BEGIN tag before any data, so a power loss
 *   mid-rewrite is recognized on the next boot and no `Full/Full` data-loss path exists.
 *   Reserved keys: `0xFFFFFFFF` erased, `0xFFFFFFFE` marker, `0xFFFFFFFD` begin.
 *   A torn write leaves a garbage slot; one whose key field happens to equal a user key
 *   reads back as that key, a `2^-32` chance per bad write.
 * @param[in] page_start First flash page
 * @param[in] page_count Number of pages, even and at least `2`
 * Internal:
 * @param _storage_pages Pages of one half
 * @param _addr_start First slot of each half
 * @param _addr_end Past the last slot of each half
 * @param _active Half taking writes
 * @param _cursor Next slot to write
 * @param _generation Generation of the active half
 * @param _init Initialization completed flag
 */
typedef struct {
  uint16_t page_start;
  uint16_t page_count;
  // internal
  uint16_t _storage_pages;
  uint32_t _addr_start[2];
  uint32_t _addr_end[2];
  EEPROM_Storage_t _active;
  uint32_t _cursor;
  uint16_t _generation;
  bool _init;
} EEPROM_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Scan both halves and recover the state, a second call is a no-op.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @return `OK` on success, `ERR` on invalid config or a flash error
 */
status_t EEPROM_Init(EEPROM_t *eeprom);

/**
 * @brief Erase both halves. A power loss mid-clear may leave stale data readable.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @return `OK` on success, `ERR` on a flash error
 */
status_t EEPROM_Clear(EEPROM_t *eeprom);

/**
 * @brief Store a value under a key.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] key Entry key, the reserved ones are refused
 * @param[in] value Entry value
 * @return `OK` on success, `ERR` on a flash error or a reserved key
 */
status_t EEPROM_Write(EEPROM_t *eeprom, uint32_t key, uint32_t value);

/**
 * @brief Value under a key.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[in] key Entry key
 * @param[in] default_value Returned when the key is absent
 * @return Stored value or `default_value`
 */
uint32_t EEPROM_Read(EEPROM_t *eeprom, uint32_t key, uint32_t default_value);

/**
 * @brief Store a variable under its own address.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] var Variable
 * @return `OK` on success, `ERR` on a flash error
 */
status_t EEPROM_Save(EEPROM_t *eeprom, uint32_t *var);

/**
 * @brief Restore a variable stored under its own address.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[out] var Variable, written only when found
 * @return `OK` when found, `ERR` when absent
 */
status_t EEPROM_Load(EEPROM_t *eeprom, uint32_t *var);

/**
 * @brief Store variables from a `NULL`-terminated list.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] var First variable, the rest follow as arguments, `NULL` last
 * @return `OK` when every one was stored, `ERR` when any failed
 */
status_t EEPROM_SaveList(EEPROM_t *eeprom, uint32_t *var, ...);

/**
 * @brief Restore variables from a `NULL`-terminated list.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[out] var First variable, the rest follow as arguments, `NULL` last
 * @return `OK` when every one was found, `ERR` when any is absent
 */
status_t EEPROM_LoadList(EEPROM_t *eeprom, uint32_t *var, ...);

/**
 * @brief Store a 64-bit variable as two entries keyed by `var` and `var + 4`.
 *   Not atomic: a power loss between the two writes leaves halves of different ages,
 *   which `EEPROM_Load64` cannot tell apart from a whole value.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] var Variable
 * @return `OK` on success, `ERR` on a flash error
 */
status_t EEPROM_Save64(EEPROM_t *eeprom, uint64_t *var);

/**
 * @brief Restore a 64-bit variable, written only when both halves are found.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[out] var Variable
 * @return `OK` when found, `ERR` when either half is absent
 */
status_t EEPROM_Load64(EEPROM_t *eeprom, uint64_t *var);

// Float stored and read back as its raw bits
status_t EEPROM_WriteF32(EEPROM_t *eeprom, uint32_t key, float value);
float EEPROM_ReadF32(EEPROM_t *eeprom, uint32_t key, float default_value);

//------------------------------------------------------------------------------------------- Cache

// The `EEPROM_...` API on one instance registered with `CACHE_Init`,
// `ERR` or the default before that
status_t CACHE_Init(EEPROM_t *eeprom);
status_t CACHE_Clear(void);
status_t CACHE_Write(uint32_t key, uint32_t value);
uint32_t CACHE_Read(uint32_t key, uint32_t default_value);
status_t CACHE_Save(uint32_t *var);
status_t CACHE_Load(uint32_t *var);
status_t CACHE_SaveList(uint32_t *var, ...);
status_t CACHE_LoadList(uint32_t *var, ...);
status_t CACHE_Save64(uint64_t *var);
status_t CACHE_Load64(uint64_t *var);
status_t CACHE_WriteF32(uint32_t key, float value);
float CACHE_ReadF32(uint32_t key, float default_value);

//-------------------------------------------------------------------------------------------------
#endif

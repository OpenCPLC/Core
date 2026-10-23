// lib/sys/eeprom.h

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
  EEPROM_State_InProgress, // BEGIN at slot 0 or a marker not whole: a rewrite cut
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
 *   Reserved keys: `0xFFFFFFFF` erased, `0xFFFFFFFE` marker, `0xFFFFFFFD` begin,
 *   `0xFFFFFFFC` a slot ECC cannot read.
 *   A write cut halfway leaves a slot ECC cannot read, or one ECC corrects to a wrong value,
 *   about 5% of cuts on G071, and a slot a cut tore can pass ECC on a later read (WB55).
 *   So the newest slot of the store, the only one a cut tears, counts only when it reads
 *   clean: init moves the store without it, and the key in flight keeps its old value.
 *   The cost: a bit flipped by age in that slot drops it as well, which loses a key written
 *   only once. Elsewhere a slot ECC corrects counts as written, one it cannot read is skipped.
 *   Damage ECC misses goes unnoticed: the key its key field matches reads the damaged value.
 * @param[in] page_start First flash page
 * @param[in] page_count Number of pages, even and at least `2`
 * Internal:
 * @param _storage_pages Pages of one half
 * @param _addr_start First slot of each half
 * @param _addr_end Past the last slot of each half
 * @param _active Half taking writes
 * @param _cursor Next slot to write
 * @param _generation Generation of the active half
 * @param _blank Active half read empty at init, erased whole before its first write
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
  bool _blank;
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
status_t CACHE_WriteF32(uint32_t key, float value);
float CACHE_ReadF32(uint32_t key, float default_value);

//-------------------------------------------------------------------------------------------------
#endif

#include "eeprom.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Get current status of storage block (A/B) in EEPROM emulation
 * Function scans flash pages assigned to storage:
 * `EEPROM_State_EMPTY`: block never written (ready for use)
 * `EEPROM_State_FILLED`: block partly used, still has free space
 * `EEPROM_State_FULL`: block completely used, needs erase
 * @param eeprom Pointer to `EEPROM_t` instance
 * @param storage Storage block `EEPROM_Storage_A` or `EEPROM_Storage_B`
 * @return Current storage state
 */
static EEPROM_State_t EEPROM_StorageStatus(EEPROM_t *eeprom, EEPROM_Storage_t storage)
{
  uint32_t i;
  uint32_t addr = eeprom->adrr_end[storage] - 8;
  if(*(uint64_t *)(addr) != (uint64_t)0xFFFFFFFFFFFFFFFF) return EEPROM_State_FULL;
  addr -= 8;
  for(i = addr; i >= eeprom->adrr_start[storage]; i -= 8) {
    if(*(uint64_t *)(i) != (uint64_t)0xFFFFFFFFFFFFFFFF) return EEPROM_State_FILLED;
  }
  return EEPROM_State_EMPTY;
}

/**
 * @brief Find write cursor inside active storage block
 * Scans flash backwards from end of block until last written entry.
 * @param eeprom Pointer to `EEPROM_t` instance
 * @return Address of next free entry inside active storage
 */
static uint32_t EEPROM_FindCursor(EEPROM_t *eeprom)
{
  uint32_t i;
  for(i = eeprom->adrr_end[eeprom->active_storage] - 8; i >= eeprom->adrr_start[eeprom->active_storage]; i -= 8) {
    if(*(uint64_t *) (i) != (uint64_t) 0xFFFFFFFFFFFFFFFF) break;
  }
  return i + 8;
}

/**
 * @brief Erase all flash pages reserved for EEPROM emulation
 * Clears both storage blocks (A and B).
 * After clear active storage is set to A, cursor at block start.
 * @param eeprom Pointer to `EEPROM_t` instance
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Clear(EEPROM_t *eeprom)
{
  uint8_t page_count = 2 * eeprom->storage_pages;
  for(uint8_t i = 0; i < page_count; i++) {
    if(FLASH_Erase(eeprom->page_start + i)) return ERR;
  }
  eeprom->active_storage = EEPROM_Storage_A;
  eeprom->cursor = eeprom->adrr_start[EEPROM_Storage_A];
  return OK;
}

/**
 * @brief Erase all flash pages of one storage block
 * Clears either storage A or B depending on `storage`.
 * @param eeprom Pointer to `EEPROM_t` instance
 * @param storage Selected storage block (`EEPROM_Storage_A` or `EEPROM_Storage_B`)
 * @return `OK` on success, `ERR` on error
 */
static status_t EEPROM_ClearStorage(EEPROM_t *eeprom, EEPROM_Storage_t storage)
{
  uint8_t page_storage_start = eeprom->page_start + (storage == EEPROM_Storage_B ? eeprom->storage_pages : 0);
  for(uint8_t i = 0; i < eeprom->storage_pages; i++) {
    if(FLASH_Erase(page_storage_start + i)) return ERR;
  }
  return OK;
}

/**
 * @brief Rewrite EEPROM when active storage is full
 * Copies valid key/value pairs from full block into the empty block.
 * Old block is erased at the end, new block becomes active.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] full_storage Storage block that is full
 * @return `OK` on success, `ERR` on error
 */
static status_t EEPROM_Rewrite(EEPROM_t *eeprom, EEPROM_Storage_t full_storage)
{
  EEPROM_Storage_t empty_storage = !full_storage;
  eeprom->active_storage = empty_storage;
  eeprom->cursor = eeprom->adrr_start[empty_storage];
  // Max records in one block
  uint32_t max_records = (eeprom->adrr_end[full_storage] - eeprom->adrr_start[full_storage]) / 8;
  // Temporary RAM buffer
  typedef struct { uint32_t key; uint32_t value; } Record_t;
  Record_t records[max_records];
  uint32_t count = 0;
  // Scan full block from end → take last entries
  for(uint32_t i = eeprom->adrr_end[full_storage] - 8; i >= eeprom->adrr_start[full_storage]; i -= 8) {
    uint32_t key   = *(uint32_t *)(i);
    uint32_t value = *(uint32_t *)(i + 4);
    if(key != 0xFFFFFFFF) {
      // Check if key already in buffer
      bool exists = false;
      for(uint32_t k = 0; k < count; k++) {
        if(records[k].key == key) {
          exists = true;
          break;
        }
      }
      // Add new record
      if(!exists && count < max_records) {
        records[count].key   = key;
        records[count].value = value;
        count++;
      }
    }
  }
  // Write collected records to empty block
  for(uint32_t k = 0; k < count; k++) {
    if(FLASH_Write(eeprom->cursor, records[k].key, records[k].value)) return ERR;
    eeprom->cursor += 8;
  }
  if(EEPROM_ClearStorage(eeprom, full_storage)) return ERR;
  return OK;
}

// static status_t EEPROM_Rewrite(EEPROM_t *eeprom, EEPROM_Storage_t full_storage)
// {
//   EEPROM_Storage_t empty_storage = !full_storage;
//   eeprom->active_storage = empty_storage;
//   eeprom->cursor = eeprom->adrr_start[empty_storage];
//   for(uint32_t i = eeprom->adrr_end[full_storage] - 8; i >= eeprom->adrr_start[full_storage]; i -= 8) {
//     uint32_t key = *(uint32_t *)(i);
//     uint32_t value = *(uint32_t *)(i + 4);
//     if(key != 0xFFFFFFFF) {
//       for(uint32_t j = eeprom->adrr_start[empty_storage]; j < eeprom->adrr_end[empty_storage]; j += 8) {
//         uint32_t tmp = *(uint32_t *)(j);
//         if(key == tmp) break;
//         else if(tmp == 0xFFFFFFFF) {
//           if(FLASH_Write(j, key, value)) return ERR;
//           eeprom->cursor += 8;
//           break;
//         }
//       }
//     }
//   }
//   if(EEPROM_ClearStorage(eeprom, full_storage)) return ERR;
//   return OK;
// }

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize EEPROM emulation
 * Sets up flash pages, determines active storage block, finds cursor.
 * Handles cases: both empty, both full, restart in progress.
 * @param eeprom Pointer to `EEPROM_t` instance
 * @return `OK` if initialized, `ERR` if config invalid
 */
status_t EEPROM_Init(EEPROM_t *eeprom)
{
  if(eeprom->adrr_start[EEPROM_Storage_A]) return OK;
  eeprom->storage_pages = (eeprom->page_count + 1) / 2;
  if(eeprom->page_start + (2 * eeprom->storage_pages) > FLASH_PAGES) return ERR;
  FLASH_Init();
  eeprom->adrr_start[EEPROM_Storage_A] = FLASH_GetAddress(eeprom->page_start, 0);
  eeprom->adrr_end[EEPROM_Storage_A] = FLASH_GetAddress(eeprom->page_start + eeprom->storage_pages, 0);
  eeprom->adrr_start[EEPROM_Storage_B] = FLASH_GetAddress(eeprom->page_start + eeprom->storage_pages, 0);
  eeprom->adrr_end[EEPROM_Storage_B] = FLASH_GetAddress(eeprom->page_start + (2 * eeprom->storage_pages), 0);
  EEPROM_State_t status_a = EEPROM_StorageStatus(eeprom, EEPROM_Storage_A);
  EEPROM_State_t status_b = EEPROM_StorageStatus(eeprom, EEPROM_Storage_B);
  if(status_a == EEPROM_State_FULL && status_b == EEPROM_State_FULL) { // Error
    if(EEPROM_Clear(eeprom)) return ERR;
  }
  else if(status_a == EEPROM_State_EMPTY && status_b == EEPROM_State_EMPTY) {
    eeprom->active_storage = EEPROM_Storage_A;
    eeprom->cursor = eeprom->adrr_start[EEPROM_Storage_A];
  }
  else if(status_a == EEPROM_State_FULL || status_b == EEPROM_State_FULL) { // Restart in progress
    if(status_b == EEPROM_State_FULL) {
      if(EEPROM_ClearStorage(eeprom,EEPROM_Storage_A)) return ERR;
      if(EEPROM_Rewrite(eeprom, EEPROM_Storage_B)) return ERR;
    }
    else {
      if(EEPROM_ClearStorage(eeprom,EEPROM_Storage_B)) return ERR;
      if(EEPROM_Rewrite(eeprom, EEPROM_Storage_A)) return ERR;
    }
  }
  else if(status_a == EEPROM_State_EMPTY || status_b == EEPROM_State_EMPTY) { // Standard work
    if(status_a == EEPROM_State_FILLED) eeprom->active_storage = EEPROM_Storage_A;
    else eeprom->active_storage = EEPROM_Storage_B;
    eeprom->cursor = EEPROM_FindCursor(eeprom);
  }
  return OK;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Write key/value to EEPROM
 * Store entry at cursor and move forward, rewrite if block is full
 * @param eeprom EEPROM object
 * @param key Entry key
 * @param value Entry value
 * @return `OK` on success, `ERR` on flash error
 */
status_t EEPROM_Write(EEPROM_t *eeprom, uint32_t key, uint32_t value)
{
  if(FLASH_Write(eeprom->cursor, key, value)) return ERR;
  eeprom->cursor += 8;
  if(eeprom->cursor >= eeprom->adrr_end[eeprom->active_storage]) {
    if(EEPROM_Rewrite(eeprom, eeprom->active_storage)) return ERR;
  }
  return OK;
}

/**
 * @brief Read value by key from EEPROM
 * Scan block backward to find latest entry
 * @param eeprom EEPROM object
 * @param key Entry key
 * @param default_value Returned if key not found
 * @return Stored value or default
 */
uint32_t EEPROM_Read(EEPROM_t *eeprom, uint32_t key, uint32_t default_value)
{
  uint32_t i, flash_key, flash_value;
  for(i = eeprom->adrr_end[eeprom->active_storage] - 8; i >= eeprom->adrr_start[eeprom->active_storage]; i -= 8) {
    flash_key = *(uint32_t *)(i);
    flash_value = *(uint32_t *)(i + 4);
    if(key == flash_key) {
      return flash_value;
    }
  }
  return default_value;
}

/**
 * @brief Save variable into Flash EEPROM
 * Store address and value at cursor, then move cursor forward.
 * If page is full, rewrite it.
 * @param eeprom EEPROM object
 * @param var Pointer to variable to save
 * @return `OK` on success, `ERR` on failure
 */
status_t EEPROM_Save(EEPROM_t *eeprom, uint32_t *var)
{
  if(FLASH_Write(eeprom->cursor, (uint32_t)var, *var)) return ERR;
  eeprom->cursor += 8;
  if(eeprom->cursor >= eeprom->adrr_end[eeprom->active_storage]) {
    if(EEPROM_Rewrite(eeprom, eeprom->active_storage)) return ERR;
  }
  return OK;
}

/**
 * @brief Load variable from Flash EEPROM
 * Search backward for stored address and return the latest value.
 * @param eeprom EEPROM object
 * @param var Pointer to variable (updated if found)
 * @return `OK` if value found, `ERR` otherwise
 */
status_t EEPROM_Load(EEPROM_t *eeprom, uint32_t *var)
{
  uint32_t i, flash_key, flash_value;
  for(i = eeprom->adrr_end[eeprom->active_storage] - 8; i >= eeprom->adrr_start[eeprom->active_storage]; i -= 8) {
    flash_key = *(uint32_t *)(i);
    flash_value = *(uint32_t *)(i + 4);
    if((uint32_t)var == flash_key) {
      *var = flash_value;
      return OK;
    }
  }
  return ERR;
}

/**
 * @brief Save multiple variables into Flash EEPROM
 * Use variable list, end with NULL
 * @param eeprom EEPROM object
 * @param var First variable pointer, then more with `...`
 * @return `OK` if all saved, `ERR` if any failed
 */
status_t EEPROM_SaveList(EEPROM_t *eeprom, uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) { 
    if(EEPROM_Save(eeprom, var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

/**
 * @brief Load multiple variables from Flash EEPROM
 * Use variable list, end with NULL
 * @param eeprom EEPROM object
 * @param var First variable pointer, then more with `...`
 * @return `OK` if all found, `ERR` if any missing
 */
status_t EEPROM_LoadList(EEPROM_t *eeprom, uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) { 
    if(EEPROM_Load(eeprom, var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

/**
 * @brief Save 64-bit variable into Flash EEPROM
 * Split into two 32-bit parts and store with EEPROM_Save
 * @param eeprom EEPROM object
 * @param var Pointer to 64-bit variable to save
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Save64(EEPROM_t *eeprom, uint64_t *var)
{
  uint32_t p32[2];
  (void)memcpy(&p32[0], var, sizeof(uint64_t));
  if(EEPROM_Save(eeprom, &p32[0]) != OK) return ERR;
  if(EEPROM_Save(eeprom, &p32[1]) != OK) return ERR;
  return OK;
}

// inconsistent with MISRA/IEC61508 norm!
// status_t EEPROM_Save64(EEPROM_t *eeprom, uint64_t *var)
// {
//   uint32_t *p32 = (uint32_t *)(void *)var;  // low = p32[0], high = p32[1]
//   if(EEPROM_Save(eeprom, &p32[0]) != OK) return ERR;
//   if(EEPROM_Save(eeprom, &p32[1]) != OK) return ERR;
//   return OK;
// }


/**
 * @brief Load 64-bit variable from Flash EEPROM
 * Read two 32-bit parts and merge into 64-bit value
 * @param eeprom EEPROM object
 * @param var Pointer to 64-bit variable (updated if found)
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Load64(EEPROM_t *eeprom, uint64_t *var)
{
  uint32_t p32[2];
  if(EEPROM_Load(eeprom, &p32[0]) != OK) return ERR;
  if(EEPROM_Load(eeprom, &p32[1]) != OK) return ERR;
  (void)memcpy(var, &p32[0], sizeof(uint64_t));
  return OK;
}

// inconsistent with MISRA/IEC61508 norm!
// status_t EEPROM_Load64(EEPROM_t *eeprom, uint64_t *var)
// {
//   uint32_t *p32 = (uint32_t *)(void *)var;
//   if(EEPROM_Load(eeprom, &p32[0]) != OK) return ERR;
//   if(EEPROM_Load(eeprom, &p32[1]) != OK) return ERR;
//   return OK;
// }

/**
 * @brief Write floating-point value to EEPROM
 * Convert `float` to 32-bit raw binary and store under given key.
 * This version uses `memcpy` to ensure compliance with MISRA and IEC 61508.
 * @param eeprom EEPROM object
 * @param key Unique entry identifier
 * @param value Floating-point value to write
 * @return `OK` on success, `ERR` on flash error
 */
status_t EEPROM_WriteF32(EEPROM_t *eeprom, uint32_t key, float value)
{
  uint32_t raw;
  (void)memcpy(&raw, &value, sizeof(float));
  return EEPROM_Write(eeprom, key, raw);
}

// inconsistent with MISRA/IEC61508 norm!
// status_t EEPROM_WriteFloat(EEPROM_t *eeprom, uint32_t key, float value)
// {
//   return EEPROM_Write(eeprom, key, *(uint32_t *)&value);
// }

/**
 * @brief Read floating-point value from EEPROM
 * Retrieve 32-bit raw binary by key and convert back to `float`.
 * Returns `default_value` if key not found. Uses `memcpy` to avoid aliasing.
 * @param eeprom EEPROM object
 * @param key Unique entry identifier
 * @param default_value Value returned if key is missing
 * @return Stored float value or default
 */
float EEPROM_ReadF32(EEPROM_t *eeprom, uint32_t key, float default_value)
{
  uint32_t raw, def;
  (void)memcpy(&def, &default_value, sizeof(float));
  raw = EEPROM_Read(eeprom, key, def);
  (void)memcpy(&default_value, &raw, sizeof(float));
  return default_value;
}

// inconsistent with MISRA/IEC61508 norm!
// float EEPROM_ReadFloat(EEPROM_t *eeprom, uint32_t key, float default_value)
// {
//   uint32_t raw = EEPROM_Read(eeprom, key, *(uint32_t *)&default_value);
//   return *(float *)&raw;
// }

//------------------------------------------------------------------------------------------------- Cache

static EEPROM_t *eeprom_cache;

/**
 * @brief Clear EEPROM using cache
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Clear(void)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Clear(eeprom_cache);
}

/**
 * @brief Init cache wrapper for EEPROM
 * @param eeprom EEPROM object
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Init(EEPROM_t *eeprom)
{
  eeprom_cache = eeprom;
  return EEPROM_Init(eeprom_cache);
}

/**
 * @brief Write key/value to EEPROM using cache
 * @param key Entry key
 * @param value Entry value
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Write(uint32_t key, uint32_t value)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Write(eeprom_cache, key, value);
}

/**
 * @brief Read value by key from EEPROM using cache
 * @param key Entry key
 * @param default_value Returned if key not found
 * @return Stored value or default
 */
uint32_t CACHE_Read(uint32_t key, uint32_t default_value)
{
  if(!eeprom_cache) return default_value;
  return EEPROM_Read(eeprom_cache, key, default_value);
}

/**
 * @brief Save single variable using cache
 * @param var Pointer to variable
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Save(uint32_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Save(eeprom_cache, var);
}

/**
 * @brief Load single variable using cache
 * @param var Pointer to variable (updated if found)
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Load(uint32_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Load(eeprom_cache, var);
}

/**
 * @brief Save multiple variables using cache
 * Pass list of variables, end with NULL
 * @param var First variable pointer
 * @return `OK` if all saved, `ERR` if any failed
 */
status_t CACHE_SaveList(uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) { 
    if(CACHE_Save(var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

/**
 * @brief Load multiple variables using cache
 * Pass list of variables, end with NULL
 * @param var First variable pointer
 * @return `OK` if all found, `ERR` if any missing
 */
status_t CACHE_LoadList(uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) { 
    if(CACHE_Load(var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

/**
 * @brief Save 64-bit variable using cache
 * @param var Pointer to 64-bit variable
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Save64(uint64_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Save64(eeprom_cache, var);
}

/**
 * @brief Load 64-bit variable using cache
 * @param var Pointer to 64-bit variable (updated if found)
 * @return `OK` on success, `ERR` on error
 */
status_t CACHE_Load64(uint64_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Load64(eeprom_cache, var);
}

/**
 * @brief Write floating-point value to cache
 * Convert `float` to 32-bit raw binary and store it safely.
 * @param key Unique cache entry identifier
 * @param value Floating-point value to store
 * @return `OK` on success, `ERR` on failure
 */
status_t CACHE_WriteF32(uint32_t key, float value)
{
  uint32_t raw;
  (void)memcpy(&raw, &value, sizeof(float));
  return CACHE_Write(key, raw);
}

/**
 * @brief Read floating-point value from cache
 * Retrieve raw 32-bit data and convert it back to `float`.
 * Returns `default_value` if key is not found.
 * @param key Unique cache entry identifier
 * @param default_value Returned if key not found
 * @return Stored float value or default
 */
float CACHE_ReadF32(uint32_t key, float default_value)
{
  uint32_t raw, def;
  (void)memcpy(&def, &default_value, sizeof(float));
  raw = CACHE_Read(key, def);
  (void)memcpy(&default_value, &raw, sizeof(float));
  return default_value;
}

//-------------------------------------------------------------------------------------------------

// hal/stm32/per/eeprom.c

#include "eeprom.h"

//-------------------------------------------------------------------------------------------------

static EEPROM_State_t EEPROM_StorageStatus(EEPROM_t *eeprom, EEPROM_Storage_t storage)
{
  uint32_t i;
  uint32_t addr = eeprom->_addr_end[storage] - 8;
  if(*(uint64_t *)(addr) != (uint64_t)0xFFFFFFFFFFFFFFFF) return EEPROM_State_Full;
  addr -= 8;
  for(i = addr; i >= eeprom->_addr_start[storage]; i -= 8) {
    if(*(uint64_t *)(i) != (uint64_t)0xFFFFFFFFFFFFFFFF) return EEPROM_State_Filled;
  }
  return EEPROM_State_Empty;
}

static uint32_t EEPROM_FindCursor(EEPROM_t *eeprom)
{
  uint32_t i;
  for(i = eeprom->_addr_end[eeprom->_active] - 8; i >= eeprom->_addr_start[eeprom->_active]; i -= 8) {
    if(*(uint64_t *) (i) != (uint64_t) 0xFFFFFFFFFFFFFFFF) break;
  }
  return i + 8;
}

status_t EEPROM_Clear(EEPROM_t *eeprom)
{
  uint8_t page_count = 2 * eeprom->_storage_pages;
  for(uint8_t i = 0; i < page_count; i++) {
    if(FLASH_Erase(eeprom->page_start + i)) return ERR;
  }
  eeprom->_active = EEPROM_Storage_A;
  eeprom->_cursor = eeprom->_addr_start[EEPROM_Storage_A];
  return OK;
}

static status_t EEPROM_ClearStorage(EEPROM_t *eeprom, EEPROM_Storage_t storage)
{
  uint8_t page_base = eeprom->page_start
    + (storage == EEPROM_Storage_B ? eeprom->_storage_pages : 0);
  // Erase last page first: StorageStatus checks last slot,
  // so partial erase looks Full (not Empty) on next boot
  for(int8_t i = eeprom->_storage_pages - 1; i >= 0; i--) {
    if(FLASH_Erase(page_base + i)) return ERR;
  }
  return OK;
}

static status_t EEPROM_Rewrite(EEPROM_t *eeprom, EEPROM_Storage_t full_storage)
{
  EEPROM_Storage_t empty_storage = !full_storage;
  eeprom->_active = empty_storage;
  eeprom->_cursor = eeprom->_addr_start[empty_storage];
  uint32_t max_records = (eeprom->_addr_end[full_storage] - eeprom->_addr_start[full_storage]) / 8;
  typedef struct { uint32_t key; uint32_t value; } Record_t;
  Record_t records[max_records];
  uint32_t count = 0;
  for(uint32_t i = eeprom->_addr_end[full_storage] - 8; i >= eeprom->_addr_start[full_storage]; i -= 8) {
    uint32_t key   = *(uint32_t *)(i);
    uint32_t value = *(uint32_t *)(i + 4);
    if(key != 0xFFFFFFFF) {
      bool exists = false;
      for(uint32_t k = 0; k < count; k++) {
        if(records[k].key == key) {
          exists = true;
          break;
        }
      }
      if(!exists && count < max_records) {
        records[count].key   = key;
        records[count].value = value;
        count++;
      }
    }
  }
  for(uint32_t k = 0; k < count; k++) {
    if(FLASH_Write(eeprom->_cursor, records[k].key, records[k].value)) return ERR;
    eeprom->_cursor += 8;
  }
  if(EEPROM_ClearStorage(eeprom, full_storage)) return ERR;
  return OK;
}

//-------------------------------------------------------------------------------------------------

status_t EEPROM_Init(EEPROM_t *eeprom)
{
  if(eeprom->_addr_start[EEPROM_Storage_A]) return OK;
  eeprom->_storage_pages = (eeprom->page_count + 1) / 2;
  if(eeprom->page_start + (2 * eeprom->_storage_pages) > FLASH_PAGES) return ERR;
  eeprom->_addr_start[EEPROM_Storage_A] = FLASH_GetAddress(eeprom->page_start, 0);
  eeprom->_addr_end[EEPROM_Storage_A] = FLASH_GetAddress(eeprom->page_start + eeprom->_storage_pages, 0);
  eeprom->_addr_start[EEPROM_Storage_B] = FLASH_GetAddress(eeprom->page_start + eeprom->_storage_pages, 0);
  eeprom->_addr_end[EEPROM_Storage_B] = FLASH_GetAddress(eeprom->page_start + (2 * eeprom->_storage_pages), 0);
  EEPROM_State_t status_a = EEPROM_StorageStatus(eeprom, EEPROM_Storage_A);
  EEPROM_State_t status_b = EEPROM_StorageStatus(eeprom, EEPROM_Storage_B);
  if(status_a == EEPROM_State_Full && status_b == EEPROM_State_Full) {
    if(EEPROM_Clear(eeprom)) return ERR;
  }
  else if(status_a == EEPROM_State_Empty && status_b == EEPROM_State_Empty) {
    eeprom->_active = EEPROM_Storage_A;
    eeprom->_cursor = eeprom->_addr_start[EEPROM_Storage_A];
  }
  else if(status_a == EEPROM_State_Full || status_b == EEPROM_State_Full) {
    if(status_b == EEPROM_State_Full) {
      if(EEPROM_ClearStorage(eeprom, EEPROM_Storage_A)) return ERR;
      if(EEPROM_Rewrite(eeprom, EEPROM_Storage_B)) return ERR;
    }
    else {
      if(EEPROM_ClearStorage(eeprom, EEPROM_Storage_B)) return ERR;
      if(EEPROM_Rewrite(eeprom, EEPROM_Storage_A)) return ERR;
    }
  }
  else if(status_a == EEPROM_State_Filled && status_b == EEPROM_State_Filled) {
    // Power loss during Rewrite: recover from A
    if(EEPROM_ClearStorage(eeprom, EEPROM_Storage_B)) return ERR;
    if(EEPROM_Rewrite(eeprom, EEPROM_Storage_A)) return ERR;
  }
  else if(status_a == EEPROM_State_Empty || status_b == EEPROM_State_Empty) {
    if(status_a == EEPROM_State_Filled) eeprom->_active = EEPROM_Storage_A;
    else eeprom->_active = EEPROM_Storage_B;
    eeprom->_cursor = EEPROM_FindCursor(eeprom);
  }
  return OK;
}

//-------------------------------------------------------------------------------------------------

status_t EEPROM_Write(EEPROM_t *eeprom, uint32_t key, uint32_t value)
{
  if(FLASH_Write(eeprom->_cursor, key, value)) {
    eeprom->_cursor += 8; // skip corrupted slot
    if(eeprom->_cursor >= eeprom->_addr_end[eeprom->_active]) {
      if(EEPROM_Rewrite(eeprom, eeprom->_active)) return ERR;
    }
    if(FLASH_Write(eeprom->_cursor, key, value)) return ERR;
  }
  eeprom->_cursor += 8;
  if(eeprom->_cursor >= eeprom->_addr_end[eeprom->_active]) {
    if(EEPROM_Rewrite(eeprom, eeprom->_active)) return ERR;
  }
  return OK;
}

uint32_t EEPROM_Read(EEPROM_t *eeprom, uint32_t key, uint32_t default_value)
{
  uint32_t i, flash_key, flash_value;
  for(i = eeprom->_addr_end[eeprom->_active] - 8; i >= eeprom->_addr_start[eeprom->_active]; i -= 8) {
    flash_key = *(uint32_t *)(i);
    flash_value = *(uint32_t *)(i + 4);
    if(key == flash_key) {
      return flash_value;
    }
  }
  return default_value;
}

status_t EEPROM_Save(EEPROM_t *eeprom, uint32_t *var)
{
  if(FLASH_Write(eeprom->_cursor, (uint32_t)var, *var)) return ERR;
  eeprom->_cursor += 8;
  if(eeprom->_cursor >= eeprom->_addr_end[eeprom->_active]) {
    if(EEPROM_Rewrite(eeprom, eeprom->_active)) return ERR;
  }
  return OK;
}

status_t EEPROM_Load(EEPROM_t *eeprom, uint32_t *var)
{
  uint32_t i, flash_key, flash_value;
  for(i = eeprom->_addr_end[eeprom->_active] - 8; i >= eeprom->_addr_start[eeprom->_active]; i -= 8) {
    flash_key = *(uint32_t *)(i);
    flash_value = *(uint32_t *)(i + 4);
    if((uint32_t)var == flash_key) {
      *var = flash_value;
      return OK;
    }
  }
  return ERR;
}

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

status_t EEPROM_Save64(EEPROM_t *eeprom, uint64_t *var)
{
  uint32_t p32[2];
  (void)memcpy(&p32[0], var, sizeof(uint64_t));
  if(EEPROM_Save(eeprom, &p32[0]) != OK) return ERR;
  if(EEPROM_Save(eeprom, &p32[1]) != OK) return ERR;
  return OK;
}

status_t EEPROM_Load64(EEPROM_t *eeprom, uint64_t *var)
{
  uint32_t p32[2];
  if(EEPROM_Load(eeprom, &p32[0]) != OK) return ERR;
  if(EEPROM_Load(eeprom, &p32[1]) != OK) return ERR;
  (void)memcpy(var, &p32[0], sizeof(uint64_t));
  return OK;
}

status_t EEPROM_WriteF32(EEPROM_t *eeprom, uint32_t key, float value)
{
  uint32_t raw;
  (void)memcpy(&raw, &value, sizeof(float));
  return EEPROM_Write(eeprom, key, raw);
}

float EEPROM_ReadF32(EEPROM_t *eeprom, uint32_t key, float default_value)
{
  uint32_t raw, def;
  (void)memcpy(&def, &default_value, sizeof(float));
  raw = EEPROM_Read(eeprom, key, def);
  (void)memcpy(&default_value, &raw, sizeof(float));
  return default_value;
}

//------------------------------------------------------------------------------------------------- Cache

static EEPROM_t *eeprom_cache;

status_t CACHE_Clear(void)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Clear(eeprom_cache);
}

status_t CACHE_Init(EEPROM_t *eeprom)
{
  eeprom_cache = eeprom;
  return EEPROM_Init(eeprom_cache);
}

status_t CACHE_Write(uint32_t key, uint32_t value)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Write(eeprom_cache, key, value);
}

uint32_t CACHE_Read(uint32_t key, uint32_t default_value)
{
  if(!eeprom_cache) return default_value;
  return EEPROM_Read(eeprom_cache, key, default_value);
}

status_t CACHE_Save(uint32_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Save(eeprom_cache, var);
}

status_t CACHE_Load(uint32_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Load(eeprom_cache, var);
}

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

status_t CACHE_Save64(uint64_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Save64(eeprom_cache, var);
}

status_t CACHE_Load64(uint64_t *var)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Load64(eeprom_cache, var);
}

status_t CACHE_WriteF32(uint32_t key, float value)
{
  uint32_t raw;
  (void)memcpy(&raw, &value, sizeof(float));
  return CACHE_Write(key, raw);
}

float CACHE_ReadF32(uint32_t key, float default_value)
{
  uint32_t raw, def;
  (void)memcpy(&def, &default_value, sizeof(float));
  raw = CACHE_Read(key, def);
  (void)memcpy(&default_value, &raw, sizeof(float));
  return default_value;
}

//-------------------------------------------------------------------------------------------------
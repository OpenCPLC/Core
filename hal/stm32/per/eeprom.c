// hal/stm32/per/eeprom.c

#include "eeprom.h"

#include <stdarg.h>
#include <string.h>

//--------------------------------------------------------------------------------------- Constants

#define EEPROM_ERASED_KEY 0xFFFFFFFFu
#define EEPROM_ERASED_WORD 0xFFFFFFFFFFFFFFFFull
#define EEPROM_MARKER_KEY 0xFFFFFFFEu
#define EEPROM_BEGIN_KEY 0xFFFFFFFDu  // first slot of a rewrite target, the recovery tag
#define EEPROM_MARKER_MAGIC 0x5A5Au   // low half of the marker value, a torn marker lacks it

//---------------------------------------------------------------------------------------- Internal

// The last slot of a half is the marker
static inline uint32_t _marker_addr(EEPROM_t *e, EEPROM_Storage_t s)
{
  return e->_addr_end[s] - 8;
}

// State of a half, generation of a `Complete` one in `*gen_out` (`NULL` = skip).
// Priority: Complete, InProgress, Full, Filled, Empty. The BEGIN tag names the rewrite
// target no matter how far the copy got, which closes the `Full/Full` data-loss path
static EEPROM_State_t _storage_status(EEPROM_t *e, EEPROM_Storage_t s, uint16_t *gen_out)
{
  uint32_t start = e->_addr_start[s];
  uint32_t marker = _marker_addr(e, s);
  uint32_t marker_key = *(volatile uint32_t *)marker;
  uint32_t marker_val = *(volatile uint32_t *)(marker + 4);
  if(marker_key == EEPROM_MARKER_KEY && (marker_val & 0xFFFFu) == EEPROM_MARKER_MAGIC) {
    if(gen_out) *gen_out = (uint16_t)(marker_val >> 16);
    return EEPROM_State_Complete;
  }
  uint32_t slot0_key = *(volatile uint32_t *)start;
  if(slot0_key == EEPROM_BEGIN_KEY) return EEPROM_State_InProgress;
  uint64_t marker_word = *(volatile uint64_t *)marker;
  if(marker_word != EEPROM_ERASED_WORD) return EEPROM_State_Full;
  uint32_t last_data = marker - 8;
  if(*(volatile uint64_t *)last_data != EEPROM_ERASED_WORD) return EEPROM_State_Full;
  for(uint32_t addr = start; addr < last_data; addr += 8) {
    if(*(volatile uint64_t *)addr != EEPROM_ERASED_WORD) return EEPROM_State_Filled;
  }
  return EEPROM_State_Empty;
}

// Slot after the last non-erased one: holes from failed writes stay holes,
// a write never lands in the middle of the log
static uint32_t _find_cursor(EEPROM_t *e)
{
  uint32_t start = e->_addr_start[e->_active];
  uint32_t marker = _marker_addr(e, e->_active);
  uint32_t cursor = start;
  for(uint32_t addr = start; addr < marker; addr += 8) {
    if(*(volatile uint64_t *)addr != EEPROM_ERASED_WORD) cursor = addr + 8;
  }
  return cursor;
}

// Newest entry of a key: physical order is write order, so the scan runs from the end
static status_t _read_key(EEPROM_t *e, uint32_t key, uint32_t *out)
{
  uint32_t start = e->_addr_start[e->_active];
  uint32_t marker = _marker_addr(e, e->_active);
  for(uint32_t addr = marker; addr > start;) {
    addr -= 8;
    uint32_t k = *(volatile uint32_t *)addr;
    if(k == EEPROM_ERASED_KEY) continue;
    if(k == EEPROM_MARKER_KEY) continue;
    if(k == EEPROM_BEGIN_KEY) continue;
    if(k == key) {
      *out = *(volatile uint32_t *)(addr + 4);
      return OK;
    }
  }
  return ERR;
}

static status_t _clear_storage(EEPROM_t *e, EEPROM_Storage_t s)
{
  uint16_t base = e->page_start + (s == EEPROM_Storage_B ? e->_storage_pages : 0);
  // Last page first: `_storage_status` reads the last slot first, so a torn clear
  // looks Full on the next boot, never Empty
  for(int32_t i = (int32_t)e->_storage_pages - 1; i >= 0; i--) {
    if(FLASH_Erase(base + (uint16_t)i)) return ERR;
  }
  return OK;
}

static status_t _write_marker(EEPROM_t *e, EEPROM_Storage_t s, uint16_t gen)
{
  uint32_t value = ((uint32_t)gen << 16) | EEPROM_MARKER_MAGIC;
  return FLASH_Write(_marker_addr(e, s), EEPROM_MARKER_KEY, value);
}

// Copy the newest value of every key from `src_s` into the other half, newest first,
// then mark it with `_generation + 1` and clear the source. The BEGIN tag lands before
// any data, so a power loss before the marker leaves a recognizable target; one between
// the marker and the clear leaves `Complete/Complete`, resolved by generation.
// Keys past the capacity are dropped by physical position, the store is meant for
// key sets smaller than a half
static status_t _rewrite(EEPROM_t *e, EEPROM_Storage_t src_s)
{
  EEPROM_Storage_t dst_s = !src_s;
  uint32_t src_start = e->_addr_start[src_s];
  uint32_t src_end = e->_addr_end[src_s];
  uint32_t dst_start = e->_addr_start[dst_s];
  uint32_t dst_marker = _marker_addr(e, dst_s);
  uint16_t new_gen = (uint16_t)(e->_generation + 1);
  if(FLASH_Write(dst_start, EEPROM_BEGIN_KEY, (uint32_t)new_gen)) return ERR;
  uint32_t dst_cursor = dst_start + 8;
  uint32_t src = src_end;
  while(src > src_start) {
    src -= 8;
    uint32_t key = *(volatile uint32_t *)src;
    if(key == EEPROM_ERASED_KEY) continue;
    if(key == EEPROM_MARKER_KEY) continue;
    if(key == EEPROM_BEGIN_KEY) continue;
    bool dup = false;
    for(uint32_t d = dst_start + 8; d < dst_cursor; d += 8) {
      if(*(volatile uint32_t *)d == key) { dup = true; break; }
    }
    if(dup) continue;
    // One data slot stays free for the write that triggered the rewrite
    if(dst_cursor + 8 >= dst_marker) break;
    uint32_t value = *(volatile uint32_t *)(src + 4);
    if(FLASH_Write(dst_cursor, key, value)) return ERR;
    dst_cursor += 8;
  }
  if(_write_marker(e, dst_s, new_gen)) return ERR;
  e->_active = dst_s;
  e->_cursor = dst_cursor;
  e->_generation = new_gen;
  if(_clear_storage(e, src_s)) return ERR;
  return OK;
}

//--------------------------------------------------------------------------------------------- API

status_t EEPROM_Clear(EEPROM_t *e)
{
  uint16_t page_count = 2 * e->_storage_pages;
  for(uint16_t i = 0; i < page_count; i++) {
    if(FLASH_Erase(e->page_start + i)) return ERR;
  }
  e->_active = EEPROM_Storage_A;
  e->_cursor = e->_addr_start[EEPROM_Storage_A];
  e->_generation = 0;
  return OK;
}

// Recovery decision tree, strongest signal first: Complete, InProgress, Full, Filled, Empty
status_t EEPROM_Init(EEPROM_t *e)
{
  if(e->_init) return OK;
  if(e->page_count < 2 || (e->page_count & 1)) return ERR;
  e->_storage_pages = e->page_count / 2;
  if((uint32_t)e->page_start + 2u * (uint32_t)e->_storage_pages > FLASH_PAGES) return ERR;
  e->_addr_start[EEPROM_Storage_A] = FLASH_GetAddress(e->page_start, 0);
  e->_addr_end[EEPROM_Storage_A] = FLASH_GetAddress(e->page_start + e->_storage_pages, 0);
  e->_addr_start[EEPROM_Storage_B] = FLASH_GetAddress(e->page_start + e->_storage_pages, 0);
  e->_addr_end[EEPROM_Storage_B] = FLASH_GetAddress(e->page_start + 2 * e->_storage_pages, 0);
  e->_generation = 0;
  uint16_t gen_a = 0, gen_b = 0;
  EEPROM_State_t sa = _storage_status(e, EEPROM_Storage_A, &gen_a);
  EEPROM_State_t sb = _storage_status(e, EEPROM_Storage_B, &gen_b);
  // Both Complete: power loss between the marker write and the source clear.
  // The higher generation wins, the compare wraps with the 16-bit counter
  if(sa == EEPROM_State_Complete && sb == EEPROM_State_Complete) {
    int16_t diff = (int16_t)((uint16_t)(gen_a - gen_b));
    if(diff == 0) {
      if(EEPROM_Clear(e)) return ERR; // equal generations: an anomaly
    }
    else if(diff > 0) {
      e->_active = EEPROM_Storage_A;
      e->_generation = gen_a;
      e->_cursor = _find_cursor(e);
      if(_clear_storage(e, EEPROM_Storage_B)) return ERR;
    }
    else {
      e->_active = EEPROM_Storage_B;
      e->_generation = gen_b;
      e->_cursor = _find_cursor(e);
      if(_clear_storage(e, EEPROM_Storage_A)) return ERR;
    }
  }
  // One Complete: use it, clean up the other
  else if(sa == EEPROM_State_Complete) {
    e->_active = EEPROM_Storage_A;
    e->_generation = gen_a;
    e->_cursor = _find_cursor(e);
    if(sb != EEPROM_State_Empty) {
      if(_clear_storage(e, EEPROM_Storage_B)) return ERR;
    }
  }
  else if(sb == EEPROM_State_Complete) {
    e->_active = EEPROM_Storage_B;
    e->_generation = gen_b;
    e->_cursor = _find_cursor(e);
    if(sa != EEPROM_State_Empty) {
      if(_clear_storage(e, EEPROM_Storage_A)) return ERR;
    }
  }
  // InProgress on both cannot happen, a rewrite is never chained
  else if(sa == EEPROM_State_InProgress && sb == EEPROM_State_InProgress) {
    if(EEPROM_Clear(e)) return ERR;
  }
  // InProgress on one: that half is the rewrite target, clear it and replay the
  // rewrite when the source is Full
  else if(sa == EEPROM_State_InProgress) {
    if(_clear_storage(e, EEPROM_Storage_A)) return ERR;
    if(sb == EEPROM_State_Full) {
      e->_active = EEPROM_Storage_B;
      if(_rewrite(e, EEPROM_Storage_B)) return ERR;
    }
    else if(sb == EEPROM_State_Filled) {
      e->_active = EEPROM_Storage_B;
      e->_cursor = _find_cursor(e);
    }
    else { // Empty: the source vanished, an anomaly
      e->_active = EEPROM_Storage_A;
      e->_cursor = e->_addr_start[EEPROM_Storage_A];
    }
  }
  else if(sb == EEPROM_State_InProgress) {
    if(_clear_storage(e, EEPROM_Storage_B)) return ERR;
    if(sa == EEPROM_State_Full) {
      e->_active = EEPROM_Storage_A;
      if(_rewrite(e, EEPROM_Storage_A)) return ERR;
    }
    else if(sa == EEPROM_State_Filled) {
      e->_active = EEPROM_Storage_A;
      e->_cursor = _find_cursor(e);
    }
    else {
      e->_active = EEPROM_Storage_A;
      e->_cursor = e->_addr_start[EEPROM_Storage_A];
    }
  }
  // Full/Full without a BEGIN tag is flash corruption
  else if(sa == EEPROM_State_Full && sb == EEPROM_State_Full) {
    if(EEPROM_Clear(e)) return ERR;
  }
  // One Full: rewrite it, the other side is scratch
  else if(sa == EEPROM_State_Full) {
    if(sb == EEPROM_State_Filled) {
      if(_clear_storage(e, EEPROM_Storage_B)) return ERR;
    }
    e->_active = EEPROM_Storage_A;
    if(_rewrite(e, EEPROM_Storage_A)) return ERR;
  }
  else if(sb == EEPROM_State_Full) {
    if(sa == EEPROM_State_Filled) {
      if(_clear_storage(e, EEPROM_Storage_A)) return ERR;
    }
    e->_active = EEPROM_Storage_B;
    if(_rewrite(e, EEPROM_Storage_B)) return ERR;
  }
  // Filled/Filled: both mid-write, no marker, no BEGIN, an anomaly
  else if(sa == EEPROM_State_Filled && sb == EEPROM_State_Filled) {
    if(EEPROM_Clear(e)) return ERR;
  }
  // Filled/Empty: the active half mid-write
  else if(sa == EEPROM_State_Filled) {
    e->_active = EEPROM_Storage_A;
    e->_cursor = _find_cursor(e);
  }
  else if(sb == EEPROM_State_Filled) {
    e->_active = EEPROM_Storage_B;
    e->_cursor = _find_cursor(e);
  }
  // Empty/Empty: fresh flash
  else {
    e->_active = EEPROM_Storage_A;
    e->_cursor = e->_addr_start[EEPROM_Storage_A];
  }
  e->_init = true;
  return OK;
}

// A failed `FLASH_Write` skips the slot and retries once on the next; the cursor moves
// past a broken slot either way, so the next call never lands on it again
static status_t _store_kv(EEPROM_t *e, uint32_t key, uint32_t value)
{
  if(e->_cursor >= _marker_addr(e, e->_active)) {
    if(_rewrite(e, e->_active)) return ERR;
  }
  if(FLASH_Write(e->_cursor, key, value)) {
    e->_cursor += 8;
    if(e->_cursor >= _marker_addr(e, e->_active)) {
      if(_rewrite(e, e->_active)) return ERR;
    }
    if(FLASH_Write(e->_cursor, key, value)) {
      e->_cursor += 8;
      return ERR;
    }
  }
  e->_cursor += 8;
  if(e->_cursor >= _marker_addr(e, e->_active)) {
    if(_rewrite(e, e->_active)) return ERR;
  }
  return OK;
}

status_t EEPROM_Write(EEPROM_t *e, uint32_t key, uint32_t value)
{
  if(key == EEPROM_ERASED_KEY) return ERR;
  if(key == EEPROM_MARKER_KEY) return ERR;
  if(key == EEPROM_BEGIN_KEY) return ERR;
  return _store_kv(e, key, value);
}

uint32_t EEPROM_Read(EEPROM_t *e, uint32_t key, uint32_t default_value)
{
  if(key == EEPROM_ERASED_KEY) return default_value;
  if(key == EEPROM_MARKER_KEY) return default_value;
  if(key == EEPROM_BEGIN_KEY) return default_value;
  uint32_t value;
  if(_read_key(e, key, &value)) return default_value;
  return value;
}

// A RAM address is never a reserved key, so the guard of `EEPROM_Write` is not needed
status_t EEPROM_Save(EEPROM_t *e, uint32_t *var)
{
  return _store_kv(e, (uint32_t)var, *var);
}

status_t EEPROM_Load(EEPROM_t *e, uint32_t *var)
{
  return _read_key(e, (uint32_t)var, var);
}

status_t EEPROM_SaveList(EEPROM_t *e, uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) {
    if(EEPROM_Save(e, var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

status_t EEPROM_LoadList(EEPROM_t *e, uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) {
    if(EEPROM_Load(e, var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

status_t EEPROM_Save64(EEPROM_t *e, uint64_t *var)
{
  uint32_t *p = (uint32_t *)var;
  if(EEPROM_Save(e, &p[0])) return ERR;
  if(EEPROM_Save(e, &p[1])) return ERR;
  return OK;
}

status_t EEPROM_Load64(EEPROM_t *e, uint64_t *var)
{
  uint32_t *p = (uint32_t *)var;
  uint32_t lo, hi;
  if(_read_key(e, (uint32_t)&p[0], &lo)) return ERR;
  if(_read_key(e, (uint32_t)&p[1], &hi)) return ERR;
  p[0] = lo;
  p[1] = hi;
  return OK;
}

status_t EEPROM_WriteF32(EEPROM_t *e, uint32_t key, float value)
{
  uint32_t raw;
  memcpy(&raw, &value, sizeof(raw));
  return EEPROM_Write(e, key, raw);
}

float EEPROM_ReadF32(EEPROM_t *e, uint32_t key, float default_value)
{
  uint32_t raw, def;
  memcpy(&def, &default_value, sizeof(def));
  raw = EEPROM_Read(e, key, def);
  memcpy(&default_value, &raw, sizeof(raw));
  return default_value;
}

//------------------------------------------------------------------------------------------- Cache

static EEPROM_t *eeprom_cache;

status_t CACHE_Init(EEPROM_t *eeprom)
{
  if(EEPROM_Init(eeprom)) return ERR;
  eeprom_cache = eeprom;
  return OK;
}

status_t CACHE_Clear(void)
{
  if(!eeprom_cache) return ERR;
  return EEPROM_Clear(eeprom_cache);
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
  memcpy(&raw, &value, sizeof(raw));
  return CACHE_Write(key, raw);
}

float CACHE_ReadF32(uint32_t key, float default_value)
{
  uint32_t raw, def;
  memcpy(&def, &default_value, sizeof(def));
  raw = CACHE_Read(key, def);
  memcpy(&default_value, &raw, sizeof(raw));
  return default_value;
}

//-------------------------------------------------------------------------------------------------

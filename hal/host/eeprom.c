// hal/host/eeprom.c

// Port of `hal/stm32/per/eeprom.c`. The algorithm is unchanged: only the flash
// reads differ, because host flash is emulated instead of memory-mapped.
//   *(volatile uint32_t *)addr -> FLASH_Read(addr)
//   *(volatile uint64_t *)addr -> _read64(addr)
// A variable's address is also 64-bit here, so `_key` narrows it explicitly
// instead of relying on the implicit 32-bit cast the target can afford.

#include "eeprom.h"

#define EEPROM_ERASED_KEY 0xFFFFFFFFu
#define EEPROM_ERASED_WORD 0xFFFFFFFFFFFFFFFFULL
#define EEPROM_MARKER_KEY 0xFFFFFFFEu
#define EEPROM_BEGIN_KEY 0xFFFFFFFDu // First slot of rewrite-target. Power-loss recovery tag.
#define EEPROM_MARKER_MAGIC 0x5A5Au // Low 16b of marker value. Detects partial marker write.

//-------------------------------------------------------------------------------------------------

// Slot read. A single memory-mapped access on target, two emulated reads here.
// Flash is little-endian on both, and an out-of-range read yields the erased
// pattern, so the caller sees the same value either way.
static inline uint64_t _read64(uint32_t addr)
{
  return ((uint64_t)FLASH_Read(addr + 4) << 32) | FLASH_Read(addr);
}

// Variable address as entry key. See `EEPROM_Save` note in the header.
static inline uint32_t _key(const void *var)
{
  return (uint32_t)(uintptr_t)var;
}

static inline uint32_t _marker_addr(EEPROM_t *e, EEPROM_Storage_t s)
{
  return e->_addr_end[s] - 8; // Last 8B slot is reserved for the marker.
}

// Returns state. If `Complete`, writes generation (16b) to `*gen_out` (may be NULL).
// Priority: Complete, InProgress, Full, Filled, Empty.
// `InProgress` (BEGIN tag at slot[0], no valid marker) identifies a rewrite-target.
// This eliminates the `Full/Full` data-loss path on recovery.
static EEPROM_State_t _storage_status(EEPROM_t *e, EEPROM_Storage_t s, uint16_t *gen_out)
{
  uint32_t start = e->_addr_start[s];
  uint32_t marker = _marker_addr(e, s);
  uint32_t marker_key = FLASH_Read(marker);
  uint32_t marker_val = FLASH_Read(marker + 4);
  if(marker_key == EEPROM_MARKER_KEY && (marker_val & 0xFFFFu) == EEPROM_MARKER_MAGIC) {
    if(gen_out) *gen_out = (uint16_t)(marker_val >> 16);
    return EEPROM_State_Complete;
  }
  // BEGIN tag overrides Full/Filled/Empty. Storage is a rewrite-target regardless
  // of how much got copied or whether marker write was torn.
  uint32_t slot0_key = FLASH_Read(start);
  if(slot0_key == EEPROM_BEGIN_KEY) return EEPROM_State_InProgress;
  uint64_t marker_word = _read64(marker);
  if(marker_word != EEPROM_ERASED_WORD) return EEPROM_State_Full;
  uint32_t last_data = marker - 8;
  if(_read64(last_data) != EEPROM_ERASED_WORD) return EEPROM_State_Full;
  for(uint32_t addr = start; addr < last_data; addr += 8) {
    if(_read64(addr) != EEPROM_ERASED_WORD) return EEPROM_State_Filled;
  }
  return EEPROM_State_Empty;
}

// Cursor points to the slot after the last non-erased entry. Holes from failed
// writes are tolerated, so we never write into the middle of the log.
static uint32_t _find_cursor(EEPROM_t *e)
{
  uint32_t start = e->_addr_start[e->_active];
  uint32_t marker = _marker_addr(e, e->_active);
  uint32_t cursor = start;
  for(uint32_t addr = start; addr < marker; addr += 8) {
    if(_read64(addr) != EEPROM_ERASED_WORD) cursor = addr + 8;
  }
  return cursor;
}

// Reverse scan with early exit. Physical order equals write order,
// so the first match from the end is the newest.
// Erased, marker, and begin slots are skipped.
static status_t _read_key(EEPROM_t *e, uint32_t key, uint32_t *out)
{
  uint32_t start = e->_addr_start[e->_active];
  uint32_t marker = _marker_addr(e, e->_active);
  for(uint32_t addr = marker; addr > start;) {
    addr -= 8;
    uint32_t k = FLASH_Read(addr);
    if(k == EEPROM_ERASED_KEY) continue;
    if(k == EEPROM_MARKER_KEY) continue;
    if(k == EEPROM_BEGIN_KEY) continue;
    if(k == key) {
      *out = FLASH_Read(addr + 4);
      return OK;
    }
  }
  return ERR;
}

static status_t _clear_storage(EEPROM_t *e, EEPROM_Storage_t s)
{
  uint16_t base = e->page_start + (s == EEPROM_Storage_B ? e->_storage_pages : 0);
  // Erase last page first. `_storage_status` checks the last slot first,
  // so partial erase looks Full (not Empty) on next boot.
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

// In-place dedup. Iterates src newest-first, skips if key already present in dst.
// BEGIN tag is written to dst slot[0] before any data.
// This identifies dst as a rewrite-target on next boot
// if power loss occurs before marker write.
// Reserved last slot (marker) guarantees dst never becomes Full, so no data loss.
// Newest value of each key always survives.
// If unique keys exceed dst capacity the excess is dropped (by physical position,
// which copy order reverses each pass -- not strictly the oldest;
// intended for key sets smaller than capacity).
// Marker is written with `_generation + 1`.
// After power loss between marker write and src clear,
// `Complete/Complete` is resolved by generation comparison.
static status_t _rewrite(EEPROM_t *e, EEPROM_Storage_t src_s)
{
  EEPROM_Storage_t dst_s = !src_s;
  uint32_t src_start = e->_addr_start[src_s];
  uint32_t src_end = e->_addr_end[src_s];
  uint32_t dst_start = e->_addr_start[dst_s];
  uint32_t dst_marker = _marker_addr(e, dst_s);
  uint16_t new_gen = (uint16_t)(e->_generation + 1);
  // BEGIN tag on slot[0] before any data. Recovery anchor.
  if(FLASH_Write(dst_start, EEPROM_BEGIN_KEY, (uint32_t)new_gen)) return ERR;
  uint32_t dst_cursor = dst_start + 8;
  uint32_t src = src_end;
  while(src > src_start) {
    src -= 8;
    uint32_t key = FLASH_Read(src);
    if(key == EEPROM_ERASED_KEY) continue;
    if(key == EEPROM_MARKER_KEY) continue;
    if(key == EEPROM_BEGIN_KEY) continue;
    bool dup = false;
    for(uint32_t d = dst_start + 8; d < dst_cursor; d += 8) {
      if(FLASH_Read(d) == key) { dup = true; break; }
    }
    if(dup) continue;
    // Keep one data slot free: the cursor must stay < marker,
    // so the write that triggered this rewrite lands on flash,
    // not the (non-erased) marker slot.
    if(dst_cursor + 8 >= dst_marker) break; // dst full, drop oldest remaining
    uint32_t value = FLASH_Read(src + 4);
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

//-------------------------------------------------------------------------------------------------

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

// Recovery decision tree, ordered by signal strength.
// Complete (marker+gen), then InProgress (BEGIN tag), then Full, Filled, Empty.
status_t EEPROM_Init(EEPROM_t *e)
{
  if(e->_initialized) return OK;
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
  // Both Complete means power loss between marker write and old-storage clear.
  // Higher generation wins. The compare wraps safely for 16-bit counters.
  if(sa == EEPROM_State_Complete && sb == EEPROM_State_Complete) {
    int16_t diff = (int16_t)((uint16_t)(gen_a - gen_b));
    if(diff == 0) {
      if(EEPROM_Clear(e)) return ERR; // True anomaly.
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
  // One Complete. Use it, clean up the other.
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
  // InProgress on both is impossible in normal flow. Rewrite is not chained.
  else if(sa == EEPROM_State_InProgress && sb == EEPROM_State_InProgress) {
    if(EEPROM_Clear(e)) return ERR;
  }
  // InProgress on one means that storage is the rewrite-target.
  // Recover: clear it, then replay the rewrite if src is Full.
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
    else { // Empty. Anomaly, src vanished.
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
  // Full/Full without BEGIN on either is a genuine anomaly (flash corruption).
  else if(sa == EEPROM_State_Full && sb == EEPROM_State_Full) {
    if(EEPROM_Clear(e)) return ERR;
  }
  // One Full. Rewrite it, clean up scratch on the other side.
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
  // Filled/Filled. Both mid-write without marker, no BEGIN. Anomaly.
  else if(sa == EEPROM_State_Filled && sb == EEPROM_State_Filled) {
    if(EEPROM_Clear(e)) return ERR;
  }
  // Filled/Empty. Normal active storage, mid-write.
  else if(sa == EEPROM_State_Filled) {
    e->_active = EEPROM_Storage_A;
    e->_cursor = _find_cursor(e);
  }
  else if(sb == EEPROM_State_Filled) {
    e->_active = EEPROM_Storage_B;
    e->_cursor = _find_cursor(e);
  }
  // Empty/Empty. Fresh.
  else {
    e->_active = EEPROM_Storage_A;
    e->_cursor = e->_addr_start[EEPROM_Storage_A];
  }
  e->_initialized = true;
  return OK;
}

//-------------------------------------------------------------------------------------------------

// On `FLASH_Write` fail, skip the slot and retry. Holes (erased slots in middle of the log)
// are tolerated by `_read_key` and `_find_cursor`.
// Partial-garbage slots are treated as opaque records.
// See header for limitations.
// On retry fail, the cursor is advanced past the broken slot before returning ERR,
// so the next call does not loop on the same broken slot.
static status_t _store_kv(EEPROM_t *e, uint32_t key, uint32_t value)
{
  if(e->_cursor >= _marker_addr(e, e->_active)) {
    if(_rewrite(e, e->_active)) return ERR;
  }
  if(FLASH_Write(e->_cursor, key, value)) {
    e->_cursor += 8; // skip corrupted slot
    if(e->_cursor >= _marker_addr(e, e->_active)) {
      if(_rewrite(e, e->_active)) return ERR;
    }
    if(FLASH_Write(e->_cursor, key, value)) {
      e->_cursor += 8; // advance past second broken slot too
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

// Key is the variable's address: never a reserved marker (RAM, not 0xFFFFFFFx),
// so the reserved-key guard `EEPROM_Write` needs is unnecessary here.
status_t EEPROM_Save(EEPROM_t *e, uint32_t *var)
{
  return _store_kv(e, _key(var), *var);
}

status_t EEPROM_Load(EEPROM_t *e, uint32_t *var)
{
  return _read_key(e, _key(var), var);
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
  if(_read_key(e, _key(&p[0]), &lo)) return ERR;
  if(_read_key(e, _key(&p[1]), &hi)) return ERR;
  p[0] = lo;
  p[1] = hi;
  return OK;
}

status_t EEPROM_WriteF32(EEPROM_t *e, uint32_t key, float value)
{
  uint32_t raw;
  (void)memcpy(&raw, &value, sizeof(float));
  return EEPROM_Write(e, key, raw);
}

float EEPROM_ReadF32(EEPROM_t *e, uint32_t key, float default_value)
{
  uint32_t raw, def;
  (void)memcpy(&def, &default_value, sizeof(float));
  raw = EEPROM_Read(e, key, def);
  (void)memcpy(&default_value, &raw, sizeof(float));
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

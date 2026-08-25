// hal/stm32/per/pdb.c

#include "pdb.h"

//---------------------------------------------------------------------------------------- Internal

static void _page_bounds(PDB_t *pdb, uint16_t page, uint32_t *start, uint32_t *end)
{
  *start = FLASH_GetAddress(page, 0);
  uint16_t slots = FLASH_PAGE_SIZE / pdb->_record_size;
  *end = *start + (uint32_t)slots * pdb->_record_size;
}

static void _calc_bounds(PDB_t *pdb)
{
  _page_bounds(pdb, pdb->_page_active, &pdb->_pointer_start, &pdb->_pointer_end);
}

static bool _slot_erased(uint32_t addr, uint16_t size)
{
  for(uint16_t i = 0; i < size; i += 4) {
    if(*(volatile uint32_t *)(addr + i) != 0xFFFFFFFF) return false;
  }
  return true;
}

static bool _record_valid(PDB_t *pdb, uint32_t addr)
{
  if(!pdb->crc) return true;
  uint16_t len = pdb->payload_size + pdb->crc->width / 8;
  return CRC_Error(pdb->crc, (uint8_t *)addr, len) == OK;
}

// Scan page, locate slot after the last non-erased record.
// Bad-CRC slots are silently skipped for torn-write recovery.
// They remain as garbage and get filtered on read.
// Holes from failed writes are tolerated,
// so the next insert never lands in the middle of the log.
static PDB_Status_t _scan_page(PDB_t *pdb, uint16_t page, uint32_t *cursor)
{
  uint32_t start, end;
  _page_bounds(pdb, page, &start, &end);
  uint32_t last_used = start; // Slot after last non-erased record.
  uint16_t valid_count = 0;
  bool any_used = false;
  for(uint32_t addr = start; addr < end; addr += pdb->_record_size) {
    if(_slot_erased(addr, pdb->_record_size)) continue;
    any_used = true;
    last_used = addr + pdb->_record_size;
    if(_record_valid(pdb, addr)) valid_count++;
  }
  if(!any_used) {
    *cursor = start;
    return PDB_Status_Empty;
  }
  if(last_used >= end) {
    *cursor = end;
    return PDB_Status_Full;
  }
  *cursor = last_used;
  return valid_count ? PDB_Status_Filled : PDB_Status_Empty;
}

// Erase next page first, update state on success.
// Power loss between erase and state update is recoverable.
// Reboot scan sees `(old active = Full, next = Empty)`
// and Pass 2 picks `next` via Empty-after-Full detection.
static status_t _advance_page(PDB_t *pdb)
{
  uint16_t next = pdb->_page_active + 1;
  if(next >= pdb->_page_stop) next = pdb->page_start;
  PDB_LOG("Erase page:%d", next);
  if(FLASH_Erase(next)) return ERR;
  pdb->_page_active = next;
  _calc_bounds(pdb);
  pdb->_pointer = pdb->_pointer_start;
  return OK;
}

// Page holding the maximum sort key (the newest). Its successor is the oldest,
// hence the page to recycle when recovering an all-Full ring. Assumes monotonic keys.
static bool _find_newest_page(PDB_t *pdb, uint16_t *out_page)
{
  bool have = false;
  uint32_t best = 0;
  uint16_t best_page = pdb->page_start;
  for(uint16_t p = pdb->page_start; p < pdb->_page_stop; p++) {
    uint32_t start, end;
    _page_bounds(pdb, p, &start, &end);
    for(uint32_t addr = start; addr < end; addr += pdb->_record_size) {
      if(_slot_erased(addr, pdb->_record_size)) continue;
      if(!_record_valid(pdb, addr)) continue;
      uint32_t key = *(volatile uint32_t *)addr;
      if(!have || key > best) { best = key; best_page = p; have = true; }
    }
  }
  if(have) *out_page = best_page;
  return have;
}

//-------------------------------------------------------------------------------------------- Init

status_t PDB_Init(PDB_t *pdb)
{
  if(pdb->payload_size < 4) return ERR;
  if(pdb->page_count < 2) return ERR;
  uint8_t crc_bytes = pdb->crc ? pdb->crc->width / 8 : 0;
  uint16_t rec = ((pdb->payload_size + crc_bytes + 7) / 8) * 8;
  if(rec > PDB_RECORD_LIMIT) return ERR;
  pdb->_record_size = rec;
  if((uint32_t)pdb->page_start + (uint32_t)pdb->page_count > FLASH_PAGES) return ERR;
  pdb->_page_stop = pdb->page_start + pdb->page_count;
  // Pass 1: locate Filled page (the unique active page in normal state).
  uint16_t active = UINT16_MAX;
  for(uint16_t p = pdb->page_start; p < pdb->_page_stop; p++) {
    uint32_t cursor;
    if(_scan_page(pdb, p, &cursor) == PDB_Status_Filled) {
      active = p;
      pdb->_page_active = p;
      _calc_bounds(pdb);
      pdb->_pointer = cursor;
      break;
    }
  }
  // Pass 2: no Filled page. Pick Empty page that follows a Full one (post-wrap).
  if(active == UINT16_MAX) {
    for(uint16_t p = pdb->page_start; p < pdb->_page_stop; p++) {
      uint16_t prev = (p == pdb->page_start) ? pdb->_page_stop - 1 : p - 1;
      uint32_t c1, c2;
      PDB_Status_t st = _scan_page(pdb, p, &c1);
      PDB_Status_t st_prev = _scan_page(pdb, prev, &c2);
      if(st == PDB_Status_Empty && st_prev == PDB_Status_Full) {
        active = p;
        pdb->_page_active = p;
        _calc_bounds(pdb);
        pdb->_pointer = c1;
        break;
      }
    }
  }
  // Pass 3: every page Full -> advance interrupted
  // (active filled, next not yet erased).
  // Resume from the newest page and run the deferred advance.
  if(active == UINT16_MAX) {
    bool all_full = true;
    for(uint16_t p = pdb->page_start; p < pdb->_page_stop; p++) {
      uint32_t c;
      if(_scan_page(pdb, p, &c) != PDB_Status_Full) { all_full = false; break; }
    }
    uint16_t newest;
    if(all_full && _find_newest_page(pdb, &newest)) {
      pdb->_page_active = newest;
      if(!_advance_page(pdb)) active = pdb->_page_active;
    }
  }
  // Fallback: fresh flash or anomaly. Anchor on `page_start`, but honour any used
  // slots so the next write never lands in an occupied (non-erased) slot.
  if(active == UINT16_MAX) {
    pdb->_page_active = pdb->page_start;
    _calc_bounds(pdb);
    uint32_t cursor;
    _scan_page(pdb, pdb->page_start, &cursor);
    pdb->_pointer = cursor;
  }
  PDB_LOG("Init page:%d ptr:0x%08X rec:%dB",
    pdb->_page_active, pdb->_pointer, pdb->_record_size);
  return OK;
}

//----------------------------------------------------------------------------------- Insert/Delete

// On `FLASH_Write` fail mid-record, advance `_pointer` past the corrupted slot
// to the next slot boundary, then retry the whole record on the clean slot.
// The bad slot is left as garbage. Iterator skips it via CRC check, or treats
// it as opaque without CRC. Returns ERR only after the retry also fails.
status_t PDB_Insert(PDB_t *pdb, const void *record)
{
  // Resolve a deferred advance (a prior insert filled the page but its erase failed)
  // so the first write never targets a full slot at `_pointer_end`.
  if(pdb->_pointer >= pdb->_pointer_end) {
    if(_advance_page(pdb)) return ERR;
  }
  uint64_t buf[PDB_RECORD_LIMIT / 8]; // 8B-aligned, satisfies doubleword write.
  uint8_t *bytes = (uint8_t *)buf;
  memcpy(bytes, record, pdb->payload_size);
  uint16_t used = pdb->payload_size;
  if(pdb->crc) used = CRC_Append(pdb->crc, bytes, used);
  memset(bytes + used, 0xFF, pdb->_record_size - used);
  uint32_t *words = (uint32_t *)buf;
  for(uint8_t attempt = 0; attempt < 2; attempt++) {
    uint32_t slot_start = pdb->_pointer;
    bool fail = false;
    for(uint16_t i = 0; i < pdb->_record_size; i += 8) {
      if(FLASH_Write(pdb->_pointer, words[i / 4], words[i / 4 + 1])) {
        pdb->_pointer = slot_start + pdb->_record_size; // align to next slot
        fail = true;
        break;
      }
      pdb->_pointer += 8;
    }
    if(!fail) {
      PDB_LOG("Insert page:%d ptr:0x%08X", pdb->_page_active, pdb->_pointer);
      // Durable: never report ERR now. If the page filled, advance eagerly; a failed
      // erase is retried on the next insert (leaving `_pointer` at `_pointer_end`).
      if(pdb->_pointer >= pdb->_pointer_end) _advance_page(pdb);
      return OK;
    }
    // Skipped a bad slot. If that ran us off the page, advance before retrying.
    if(pdb->_pointer >= pdb->_pointer_end) {
      if(_advance_page(pdb)) return ERR;
    }
    PDB_LOG("Insert page:%d ptr:0x%08X retry%d",
      pdb->_page_active, pdb->_pointer, (int)(attempt + 1));
  }
  return ERR;
}

status_t PDB_Delete(PDB_t *pdb)
{
  for(uint16_t p = pdb->page_start; p < pdb->_page_stop; p++) {
    if(FLASH_Erase(p)) return ERR;
  }
  pdb->_page_active = pdb->page_start;
  _calc_bounds(pdb);
  pdb->_pointer = pdb->_pointer_start;
  PDB_LOG("Delete pages:%d-%d", pdb->page_start, pdb->_page_stop - 1);
  return OK;
}

//---------------------------------------------------------------------------------------- Iterator

static void _iter_set_page(PDB_Iter_t *iter, uint16_t page)
{
  iter->_page = page;
  _page_bounds(iter->_pdb, page, &iter->_pointer_start, &iter->_pointer_end);
}

static bool _iter_dec(PDB_Iter_t *iter)
{
  PDB_t *pdb = iter->_pdb;
  if(iter->_pointer == iter->_pointer_start) {
    uint16_t page = (iter->_page == pdb->page_start)
      ? pdb->_page_stop - 1 : iter->_page - 1;
    _iter_set_page(iter, page);
    iter->_pointer = iter->_pointer_end - pdb->_record_size;
  }
  else {
    iter->_pointer -= pdb->_record_size;
  }
  return iter->_pointer == iter->_origin;
}

static bool _iter_inc(PDB_Iter_t *iter)
{
  PDB_t *pdb = iter->_pdb;
  iter->_pointer += pdb->_record_size;
  if(iter->_pointer >= iter->_pointer_end) {
    uint16_t page = iter->_page + 1;
    if(page >= pdb->_page_stop) page = pdb->page_start;
    _iter_set_page(iter, page);
    iter->_pointer = iter->_pointer_start;
  }
  return iter->_pointer == iter->_origin;
}

// For Desc: start at cursor, first dec lands on newest record.
// For Asc: start at active-page last slot,
// first inc wraps to slot 0 of `(active+1) % N`.
// That is the oldest page: post-wrap has data,
// pre-wrap is erased and skipped until the scan wraps back to `page_start`.
// Origin is cursor in both cases.
status_t PDB_IterInit(PDB_t *pdb, PDB_Iter_t *iter, const PDB_Query_t *query)
{
  iter->_pdb = pdb;
  iter->_query = *query;
  iter->count = 0;
  iter->_skipped = 0;
  iter->_done = false;
  _iter_set_page(iter, pdb->_page_active);
  iter->_origin = pdb->_pointer;
  // The cursor sits past the last slot while a page-advance is still pending, and the
  // stepper only ever lands on slots. Bounding the walk ends it even then.
  iter->_steps_left = (uint32_t)pdb->page_count *
    ((iter->_pointer_end - iter->_pointer_start) / pdb->_record_size);
  if(query->dir == PDB_Desc) {
    iter->_pointer = pdb->_pointer;
  }
  else {
    iter->_pointer = iter->_pointer_end - pdb->_record_size;
  }
  return OK;
}

status_t PDB_IterNext(PDB_Iter_t *iter, void *out)
{
  if(iter->_done) return ERR;
  PDB_t *pdb = iter->_pdb;
  bool (*step)(PDB_Iter_t *) = iter->_query.dir == PDB_Desc ? _iter_dec : _iter_inc;
  while(1) {
    if(!iter->_steps_left) { iter->_done = true; return ERR; }
    iter->_steps_left--;
    if(step(iter)) { iter->_done = true; return ERR; }
    if(_slot_erased(iter->_pointer, pdb->_record_size)) continue;
    if(!_record_valid(pdb, iter->_pointer)) continue;
    uint32_t key = *(volatile uint32_t *)iter->_pointer;
    if(iter->_query.key_min && key < iter->_query.key_min) continue;
    if(iter->_query.key_max && key > iter->_query.key_max) continue;
    if(iter->_query.filter
      && !iter->_query.filter((const void *)iter->_pointer, iter->_query.filter_ctx)) continue;
    if(iter->_skipped < iter->_query.skip) {
      iter->_skipped++;
      continue;
    }
    if(out) memcpy(out, (const void *)iter->_pointer, pdb->payload_size);
    iter->count++;
    if(iter->_query.limit && iter->count >= iter->_query.limit) iter->_done = true;
    return OK;
  }
}

const void *PDB_IterRef(PDB_Iter_t *iter)
{
  return (const void *)iter->_pointer;
}

//------------------------------------------------------------------------------------ Select/Count

uint32_t PDB_Select(PDB_t *pdb, const PDB_Query_t *query, void *out, uint32_t max)
{
  if(!max) return 0;
  PDB_Iter_t iter;
  PDB_Query_t q = *query;
  if(q.limit == 0 || q.limit > max) q.limit = max;
  PDB_IterInit(pdb, &iter, &q);
  uint8_t *ptr = (uint8_t *)out;
  while(PDB_IterNext(&iter, ptr) == OK) {
    ptr += pdb->payload_size;
  }
  return iter.count;
}

uint32_t PDB_Count(PDB_t *pdb, const PDB_Query_t *query)
{
  PDB_Iter_t iter;
  PDB_IterInit(pdb, &iter, query);
  while(PDB_IterNext(&iter, NULL) == OK);
  return iter.count;
}

//-------------------------------------------------------------------------------------------------

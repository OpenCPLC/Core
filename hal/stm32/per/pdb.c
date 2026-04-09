// hal/stm32/pdb.c

#include "pdb.h"

//------------------------------------------------------------------------------------ Internal

static void _page_bounds(PDB_t *pdb, uint8_t page, uint32_t *start, uint32_t *end)
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

// Scan page, locate first erased slot. Bad-CRC slots are silently skipped
// (torn-write recovery): they remain as garbage and get filtered on read.
static PDB_Status_t _scan_page(PDB_t *pdb, uint8_t page, uint32_t *cursor)
{
  uint32_t start, end;
  _page_bounds(pdb, page, &start, &end);
  uint32_t first_erased = 0;
  uint16_t valid_count = 0;
  for(uint32_t addr = start; addr < end; addr += pdb->_record_size) {
    if(_slot_erased(addr, pdb->_record_size)) {
      if(!first_erased) first_erased = addr;
      continue;
    }
    if(_record_valid(pdb, addr)) valid_count++;
  }
  if(first_erased) {
    *cursor = first_erased;
    return valid_count ? PDB_Status_Filled : PDB_Status_Empty;
  }
  *cursor = end;
  return PDB_Status_Full;
}

static status_t _advance_page(PDB_t *pdb)
{
  pdb->_page_active++;
  if(pdb->_page_active >= pdb->_page_stop) pdb->_page_active = pdb->page_start;
  _calc_bounds(pdb);
  pdb->_pointer = pdb->_pointer_start;
  PDB_LOG("Erase page:%d", pdb->_page_active);
  return FLASH_Erase(pdb->_page_active);
}

//---------------------------------------------------------------------------------------- Init

status_t PDB_Init(PDB_t *pdb)
{
  if(pdb->payload_size < 4) return ERR;
  if(pdb->page_count < 2) return ERR;
  uint8_t crc_bytes = pdb->crc ? pdb->crc->width / 8 : 0;
  uint16_t rec = ((pdb->payload_size + crc_bytes + 7) / 8) * 8;
  if(rec > PDB_RECORD_LIMIT) return ERR;
  pdb->_record_size = rec;
  pdb->_page_stop = pdb->page_start + pdb->page_count;
  if(pdb->_page_stop > FLASH_PAGES) return ERR;
  // Pass 1: locate Filled page (the unique active page in normal state)
  uint8_t active = 0xFF;
  for(uint8_t p = pdb->page_start; p < pdb->_page_stop; p++) {
    uint32_t cursor;
    if(_scan_page(pdb, p, &cursor) == PDB_Status_Filled) {
      active = p;
      pdb->_page_active = p;
      _calc_bounds(pdb);
      pdb->_pointer = cursor;
      break;
    }
  }
  // Pass 2: no Filled — pick Empty page that follows a Full one (post-wrap)
  if(active == 0xFF) {
    for(uint8_t p = pdb->page_start; p < pdb->_page_stop; p++) {
      uint8_t prev = (p == pdb->page_start) ? pdb->_page_stop - 1 : p - 1;
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
  // Fallback: fresh flash or anomaly — start at page_start
  if(active == 0xFF) {
    pdb->_page_active = pdb->page_start;
    _calc_bounds(pdb);
    pdb->_pointer = pdb->_pointer_start;
  }
  PDB_LOG("Init page:%d ptr:0x%08X rec:%dB",
    pdb->_page_active, pdb->_pointer, pdb->_record_size);
  return OK;
}

//------------------------------------------------------------------------------- Insert/Delete

status_t PDB_Insert(PDB_t *pdb, const void *record)
{
  uint64_t buf[PDB_RECORD_LIMIT / 8]; // 8B-aligned, satisfies doubleword write
  uint8_t *bytes = (uint8_t *)buf;
  memcpy(bytes, record, pdb->payload_size);
  uint16_t used = pdb->payload_size;
  if(pdb->crc) used = CRC_Append(pdb->crc, bytes, used);
  memset(bytes + used, 0xFF, pdb->_record_size - used);
  uint32_t *words = (uint32_t *)buf;
  uint32_t addr = pdb->_pointer;
  for(uint16_t i = 0; i < pdb->_record_size; i += 8) {
    if(FLASH_Write(addr, words[i / 4], words[i / 4 + 1])) return ERR;
    addr += 8;
  }
  pdb->_pointer = addr;
  PDB_LOG("Insert page:%d ptr:0x%08X", pdb->_page_active, pdb->_pointer);
  if(pdb->_pointer >= pdb->_pointer_end) {
    if(_advance_page(pdb)) return ERR;
  }
  return OK;
}

status_t PDB_Delete(PDB_t *pdb)
{
  for(uint8_t p = pdb->page_start; p < pdb->_page_stop; p++) {
    if(FLASH_Erase(p)) return ERR;
  }
  pdb->_page_active = pdb->page_start;
  _calc_bounds(pdb);
  pdb->_pointer = pdb->_pointer_start;
  PDB_LOG("Delete pages:%d-%d", pdb->page_start, pdb->_page_stop - 1);
  return OK;
}

//------------------------------------------------------------------------------------ Iterator

static void _iter_set_page(PDB_Iter_t *iter, uint8_t page)
{
  iter->_page = page;
  _page_bounds(iter->_pdb, page, &iter->_pointer_start, &iter->_pointer_end);
}

static bool _iter_dec(PDB_Iter_t *iter)
{
  PDB_t *pdb = iter->_pdb;
  if(iter->_pointer == iter->_pointer_start) {
    uint8_t page = (iter->_page == pdb->page_start)
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
    uint8_t page = iter->_page + 1;
    if(page >= pdb->_page_stop) page = pdb->page_start;
    _iter_set_page(iter, page);
    iter->_pointer = iter->_pointer_start;
  }
  return iter->_pointer == iter->_origin;
}

status_t PDB_IterInit(PDB_t *pdb, PDB_Iter_t *iter, const PDB_Query_t *query)
{
  iter->_pdb = pdb;
  iter->_query = *query;
  iter->count = 0;
  iter->_skipped = 0;
  iter->_done = false;
  _iter_set_page(iter, pdb->_page_active);
  iter->_pointer = pdb->_pointer;
  iter->_origin = pdb->_pointer;
  return OK;
}

status_t PDB_IterNext(PDB_Iter_t *iter, void *out)
{
  if(iter->_done) return ERR;
  PDB_t *pdb = iter->_pdb;
  bool (*step)(PDB_Iter_t *) = iter->_query.dir == PDB_Desc ? _iter_dec : _iter_inc;
  while(1) {
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

//-------------------------------------------------------------------------------- Select/Count

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

//---------------------------------------------------------------------------------------------
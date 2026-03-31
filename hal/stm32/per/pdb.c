// hal/stm32/pdb.c

#include "pdb.h"

//------------------------------------------------------------------------------------ Internal

static void _page_bounds(PDB_t *pdb, uint8_t page,
  uint32_t *start, uint32_t *end)
{
  *start = FLASH_GetAddress(page, 0);
  uint16_t slots = FLASH_PAGE_SIZE / pdb->_record_size;
  *end = *start + (uint32_t)slots * pdb->_record_size;
}

static void _calc_bounds(PDB_t *pdb)
{
  _page_bounds(pdb, pdb->_page_active,
    &pdb->_pointer_start, &pdb->_pointer_end);
}

static bool _slot_erased(uint32_t addr, uint8_t size)
{
  for(uint8_t i = 0; i < size; i += 4) {
    if(*(volatile uint32_t *)(addr + i) != 0xFFFFFFFF)
      return false;
  }
  return true;
}

static bool _record_valid(PDB_t *pdb, uint32_t addr)
{
  if(!pdb->crc) return true;
  uint8_t len = pdb->payload_size + pdb->crc->width / 8;
  return CRC_Error(pdb->crc, (uint8_t *)addr, len) == OK;
}

static PDB_Status_t _scan_page(PDB_t *pdb, uint8_t page,
  uint32_t *cursor)
{
  uint32_t start, end;
  _page_bounds(pdb, page, &start, &end);
  uint16_t count = 0;
  for(uint32_t addr = start; addr < end;
    addr += pdb->_record_size) {
    if(_slot_erased(addr, pdb->_record_size)) {
      *cursor = addr;
      return count ? PDB_Status_Filled : PDB_Status_Empty;
    }
    if(!_record_valid(pdb, addr)) {
      *cursor = addr;
      return count ? PDB_Status_Filled : PDB_Status_Empty;
    }
    count++;
  }
  *cursor = end;
  return PDB_Status_Full;
}

static status_t _advance_page(PDB_t *pdb)
{
  pdb->_page_active++;
  if(pdb->_page_active >= pdb->_page_stop)
    pdb->_page_active = pdb->page_start;
  _calc_bounds(pdb);
  pdb->_pointer = pdb->_pointer_start;
  PDB_LOG("erase page:%d", pdb->_page_active);
  return FLASH_Erase(pdb->_page_active);
}

//---------------------------------------------------------------------------------------- Init

status_t PDB_Init(PDB_t *pdb)
{
  uint8_t crc_bytes = pdb->crc ? pdb->crc->width / 8 : 0;
  pdb->_record_size =
    ((pdb->payload_size + crc_bytes + 7) / 8) * 8;
  pdb->_page_stop = pdb->page_start + pdb->page_count;
  if(pdb->_page_stop > FLASH_PAGES) return ERR;
  bool found = false;
  for(uint8_t p = pdb->page_start; p < pdb->_page_stop; p++) {
    uint32_t cursor;
    PDB_Status_t st = _scan_page(pdb, p, &cursor);
    if(!found && (st == PDB_Status_Filled
      || st == PDB_Status_Empty)) {
      pdb->_page_active = p;
      _calc_bounds(pdb);
      pdb->_pointer = cursor;
      found = true;
    }
  }
  if(!found) {
    pdb->_page_active = pdb->page_start;
    _calc_bounds(pdb);
    pdb->_pointer = pdb->_pointer_start;
  }
  PDB_LOG("init page:%d ptr:0x%08X rec:%dB", pdb->_page_active, pdb->_pointer, pdb->_record_size);
  return OK;
}

//------------------------------------------------------------------------------- Insert/Delete

status_t PDB_Insert(PDB_t *pdb, const void *record)
{
  uint8_t buf[pdb->_record_size];
  memcpy(buf, record, pdb->payload_size);
  uint8_t used = pdb->payload_size;
  if(pdb->crc) used = CRC_Append(pdb->crc, buf, used);
  memset(buf + used, 0xFF, pdb->_record_size - used);
  uint32_t *words = (uint32_t *)buf;
  uint32_t addr = pdb->_pointer;
  for(uint8_t i = 0; i < pdb->_record_size; i += 8) {
    if(FLASH_Write(addr, words[i / 4], words[i / 4 + 1]))
      return ERR;
    addr += 8;
  }
  pdb->_pointer = addr;
  PDB_LOG("insert page:%d ptr:0x%08X", pdb->_page_active, pdb->_pointer);
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
  PDB_LOG("delete pages:%d-%d", pdb->page_start, pdb->_page_stop - 1);
  return OK;
}

//------------------------------------------------------------------------------------ Iterator

static void _iter_set_page(PDB_Iter_t *iter, uint8_t page)
{
  iter->_page = page;
  _page_bounds(iter->_pdb, page,
    &iter->_pointer_start, &iter->_pointer_end);
}

static bool _iter_dec(PDB_Iter_t *iter)
{
  PDB_t *pdb = iter->_pdb;
  iter->_pointer -= pdb->_record_size;
  if(iter->_pointer < iter->_pointer_start) {
    uint8_t page = (iter->_page <= pdb->page_start)
      ? pdb->_page_stop - 1 : iter->_page - 1;
    _iter_set_page(iter, page);
    iter->_pointer = iter->_pointer_end - pdb->_record_size;
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
  bool (*step)(PDB_Iter_t *) =
    iter->_query.dir == PDB_Desc ? _iter_dec : _iter_inc;
  while(1) {
    if(step(iter)) { iter->_done = true; return ERR; }
    if(_slot_erased(iter->_pointer, pdb->_record_size)) continue;
    if(!_record_valid(pdb, iter->_pointer)) continue;
    uint32_t key = *(volatile uint32_t *)iter->_pointer;
    if(iter->_query.key_min && key < iter->_query.key_min)
      continue;
    if(iter->_query.key_max && key > iter->_query.key_max)
      continue;
    if(iter->_query.filter
      && !iter->_query.filter((const void *)iter->_pointer))
      continue;
    if(iter->_skipped < iter->_query.skip) {
      iter->_skipped++;
      continue;
    }
    if(out) memcpy(out, (const void *)iter->_pointer,
      pdb->payload_size);
    iter->count++;
    if(iter->_query.limit && iter->count >= iter->_query.limit)
      iter->_done = true;
    return OK;
  }
}

const void *PDB_IterRef(PDB_Iter_t *iter)
{
  return (const void *)iter->_pointer;
}

//-------------------------------------------------------------------------------- Select/Count

uint32_t PDB_Select(PDB_t *pdb, const PDB_Query_t *query,
  void *out, uint32_t max)
{
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
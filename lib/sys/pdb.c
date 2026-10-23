// lib/sys/pdb.c

#include "pdb.h"

#include <stddef.h>
#include <string.h>

#include "crc.h"
#include "flash.h"

//--------------------------------------------------------------------------------------- Constants

#define PDB_MARK 0x01424450u  // "PDB" and format `1`, the first word of a header
#define PDB_NAME 0x00FFFFFFu  // bytes "PDB" of that word, the same in every format
#define PDB_HEAD 16u          // header [B], two doublewords
#define PDB_NONE UINT16_MAX   // no page, or a cursor before slot `0`

//---------------------------------------------------------------------------------------- Internal

// Header opening a page, written right after its erase.
// Every format keeps "PDB" in the first word and the CRC-32 in the last,
// so a page of another format reads as foreign, not as no header.
typedef struct {
  uint32_t mark;   // "PDB" in the low 3 bytes, the format in the top one
  uint16_t kind;
  uint16_t record_size;
  uint32_t first;  // number of the first record of the page
  uint32_t crc;    // CRC-32 of the words before it
} PDB_Head_t;

typedef enum {
  PDB_Page_None,    // no valid header: erased, torn or never formatted
  PDB_Page_Own,     // header of this base
  PDB_Page_Foreign  // valid header of another format, kind or record size
} PDB_Page_t;

typedef enum {
  PDB_Slot_Free,  // erased
  PDB_Slot_Used,  // no valid record: cut mid write, zeroed, or ones only ECC corrected
  PDB_Slot_Valid  // whole record numbered within its page
} PDB_Slot_t;

static uint16_t slots(const PDB_t *pdb) { return (FLASH_PAGE_SIZE - PDB_HEAD) / pdb->record_size; }

static uint32_t slot_addr(const PDB_t *pdb, uint16_t page, uint16_t slot)
{
  return FLASH_GetAddress(page, (int16_t)(PDB_HEAD + slot * pdb->record_size));
}

// CRC-32 of a record, every word but its last
static uint32_t record_crc(const PDB_t *pdb, const uint32_t *record)
{
  return CRC_Run(&crc32_iso, record, pdb->record_size - 4u);
}

// Header of `page`, the number of its first record in `*first` for one of this base
static PDB_Page_t page_head(const PDB_t *pdb, uint16_t page, uint32_t *first)
{
  PDB_Head_t head;
  if(FLASH_Copy(FLASH_GetAddress(page, 0), &head, sizeof(head), NULL)) return PDB_Page_None;
  if((head.mark & PDB_NAME) != (PDB_MARK & PDB_NAME)) return PDB_Page_None;
  if(CRC_Run(&crc32_iso, &head, offsetof(PDB_Head_t, crc)) != head.crc) return PDB_Page_None;
  if(head.mark != PDB_MARK || head.kind != pdb->kind || head.record_size != pdb->record_size) {
    return PDB_Page_Foreign;
  }
  if(!head.first) return PDB_Page_None;
  *first = head.first;
  return PDB_Page_Own;
}

// Slot through ECC into `record`, on a page numbered from `first`
static PDB_Slot_t slot_load(const PDB_t *pdb, uint16_t page, uint32_t first, uint16_t slot,
  uint32_t *record)
{
  bool corrected;
  if(FLASH_Copy(slot_addr(pdb, page, slot), record, pdb->record_size, &corrected)) {
    return PDB_Slot_Used;
  }
  uint16_t words = pdb->record_size / 4;
  bool erased = true;
  for(uint16_t i = 0; i < words; i++) {
    if(record[i] != 0xFFFFFFFFu) erased = false;
  }
  if(erased) return corrected ? PDB_Slot_Used : PDB_Slot_Free;
  if(record_crc(pdb, record) != record[words - 1]) return PDB_Slot_Used;
  return record[0] >= first && record[0] - first < slots(pdb) ? PDB_Slot_Valid : PDB_Slot_Used;
}

// Page the base moves on to past `_page`: the next one up, round to the first in a ring,
// past pages of another kind. `PDB_NONE` past the last page of a `no_wrap` base,
// or when a ring comes round to `_page` itself.
static uint16_t page_after(const PDB_t *pdb)
{
  uint16_t end = pdb->page_start + pdb->page_count;
  uint16_t page = pdb->_page == PDB_NONE ? pdb->page_start : pdb->_page + 1;
  for(uint16_t i = 0; i < pdb->page_count; i++, page++) {
    if(page >= end) {
      if(pdb->no_wrap) return PDB_NONE;
      page = pdb->page_start;
    }
    if(page == pdb->_page) return PDB_NONE;
    uint32_t first;
    if(page_head(pdb, page, &first) != PDB_Page_Foreign) return page;
  }
  return PDB_NONE;
}

// `page` given its header, the newest page of the base, numbering on from `_next`
static status_t page_mark(PDB_t *pdb, uint16_t page)
{
  PDB_Head_t head = { .mark = PDB_MARK, .kind = pdb->kind, .record_size = pdb->record_size,
    .first = pdb->_next };
  head.crc = CRC_Run(&crc32_iso, &head, offsetof(PDB_Head_t, crc));
  uint32_t word[4];
  memcpy(word, &head, sizeof(word));
  uint32_t addr = FLASH_GetAddress(page, 0);
  if(FLASH_Write(addr, word[0], word[1]) || FLASH_Write(addr + 8, word[2], word[3])) return ERR;
  pdb->_page = page;
  pdb->_slot = 0;
  pdb->_first = pdb->_next;
  pdb->_fresh = false;
  return OK;
}

// Base on to its next page, erased whole and given its header:
// an erase cut late leaves a page reading blank that holds no program until erased whole.
// The page left behind may hold no valid record: the new one still numbers past its first,
// so no two headers share a first number.
static status_t page_open(PDB_t *pdb)
{
  uint16_t page = page_after(pdb);
  if(page == PDB_NONE) return ERR;
  PDB_LOG("Erase page:%u", page);
  if(FLASH_EraseFull(page)) return ERR;
  if(pdb->_page != PDB_NONE && pdb->_next == pdb->_first) pdb->_next++;
  return page_mark(pdb, page);
}

// Slot under the cursor zeroed: zeros go over whatever a cut left there and never read valid
static status_t slot_zero(const PDB_t *pdb)
{
  uint32_t addr = slot_addr(pdb, pdb->_page, pdb->_slot);
  for(uint16_t at = 0; at < pdb->record_size; at += 8) {
    if(FLASH_Write(addr + at, 0, 0)) return ERR;
  }
  return OK;
}

// Slot under the cursor ready for a record.
// Past the last slot the base moves on to its next page.
// The first record after `PDB_Init` zeroes the first free slot and takes the next,
// since a write cut at its very start can leave its slot reading free.
static status_t slot_take(PDB_t *pdb)
{
  if(pdb->_page == PDB_NONE || pdb->_slot >= slots(pdb)) {
    if(page_open(pdb)) return ERR;
  }
  if(!pdb->_fresh) return OK;
  pdb->_fresh = false;
  status_t err = slot_zero(pdb);
  pdb->_slot++;
  if(err) return ERR;
  return pdb->_slot < slots(pdb) ? OK : page_open(pdb);
}

// Record into the slot under the cursor, the doubleword with its CRC-32 last, then read back:
// `ERR` unless it reads whole, ECC correcting none of it.
static status_t slot_write(const PDB_t *pdb, const uint32_t *record)
{
  uint32_t addr = slot_addr(pdb, pdb->_page, pdb->_slot);
  for(uint16_t at = 0; at < pdb->record_size; at += 8) {
    if(FLASH_Write(addr + at, record[at / 4], record[at / 4 + 1])) return ERR;
  }
  for(uint16_t at = 0; at < pdb->record_size; at += 8) {
    uint32_t word[2];
    bool corrected;
    if(FLASH_Copy(addr + at, word, sizeof(word), &corrected) || corrected) return ERR;
    if(word[0] != record[at / 4] || word[1] != record[at / 4 + 1]) return ERR;
  }
  return OK;
}

//-------------------------------------------------------------------------------------------- Init

// Newest own page into `_page`, the number of its first record into `_first`
static void page_newest(PDB_t *pdb)
{
  for(uint16_t page = pdb->page_start; page < pdb->page_start + pdb->page_count; page++) {
    uint32_t first;
    if(page_head(pdb, page, &first) != PDB_Page_Own) continue;
    if(pdb->_page != PDB_NONE && first <= pdb->_first) continue;
    pdb->_page = page;
    pdb->_first = first;
  }
}

// Cursor past the last slot of `_page` not free, the next number past the highest valid one
static void cursor_find(PDB_t *pdb)
{
  uint32_t record[PDB_RECORD_LIMIT / 4];
  pdb->_next = pdb->_first;
  for(uint16_t slot = 0; slot < slots(pdb); slot++) {
    PDB_Slot_t state = slot_load(pdb, pdb->_page, pdb->_first, slot, record);
    if(state == PDB_Slot_Free) continue;
    pdb->_slot = slot + 1;
    if(state == PDB_Slot_Valid && record[0] >= pdb->_next) pdb->_next = record[0] + 1;
  }
}

status_t PDB_Init(PDB_t *pdb)
{
  if(pdb->page_count < (pdb->no_wrap ? 1 : 2)) return ERR;
  if(pdb->page_start + pdb->page_count > FLASH_PAGES) return ERR;
  if(pdb->record_size < 16 || pdb->record_size > PDB_RECORD_LIMIT || pdb->record_size % 8) {
    return ERR;
  }
  pdb->_page = PDB_NONE;
  pdb->_slot = 0;
  pdb->_first = 1;
  pdb->_next = 1;
  pdb->_fresh = false;
  page_newest(pdb);
  if(pdb->_page == PDB_NONE) {
    PDB_LOG("Init empty");
    return OK;
  }
  cursor_find(pdb);
  pdb->_fresh = true;
  PDB_LOG("Init page:%u slot:%u next:%u", pdb->_page, pdb->_slot, pdb->_next);
  return OK;
}

//----------------------------------------------------------------------------------- Insert/Delete

status_t PDB_Insert(PDB_t *pdb, const void *record)
{
  uint32_t sealed[PDB_RECORD_LIMIT / 4];
  uint16_t words = pdb->record_size / 4;
  memcpy(sealed, record, pdb->record_size);
  for(uint8_t attempt = 0; attempt < 2; attempt++) {
    if(slot_take(pdb)) return ERR;
    sealed[0] = pdb->_next;
    sealed[words - 1] = record_crc(pdb, sealed);
    status_t err = slot_write(pdb, sealed);
    PDB_LOG("Insert page:%u slot:%u number:%u%s",
      pdb->_page, pdb->_slot, pdb->_next, err ? " read back wrong" : "");
    pdb->_slot++;
    if(!err) {
      pdb->_next++;
      return OK;
    }
  }
  return ERR;
}

status_t PDB_Delete(PDB_t *pdb)
{
  pdb->_page = PDB_NONE;
  pdb->_next = 1;
  for(uint16_t page = pdb->page_start; page < pdb->page_start + pdb->page_count; page++) {
    if(FLASH_EraseFull(page)) return ERR;
  }
  PDB_LOG("Delete pages:%u-%u", pdb->page_start, pdb->page_start + pdb->page_count - 1);
  return page_mark(pdb, pdb->page_start);
}

//---------------------------------------------------------------------------------------- Iterator

// Own page next to the one numbered from `*first` in the order of `dir`:
// `false` when none is past it.
static bool page_near(const PDB_t *pdb, PDB_Dir_t dir, uint16_t *page, uint32_t *first)
{
  bool found = false;
  uint32_t from = *first;
  for(uint16_t p = pdb->page_start; p < pdb->page_start + pdb->page_count; p++) {
    uint32_t n;
    if(page_head(pdb, p, &n) != PDB_Page_Own) continue;
    bool past = dir == PDB_Asc ? n > from : n < from;
    bool nearer = !found || (dir == PDB_Asc ? n < *first : n > *first);
    if(!past || !nearer) continue;
    found = true;
    *page = p;
    *first = n;
  }
  return found;
}

// Cursor onto the next slot of the walk, across pages: `false` past the last one.
// A walk up ends at the write cursor of the newest page.
static bool iter_step(PDB_Iter_t *iter)
{
  PDB_t *pdb = iter->_pdb;
  if(iter->_query.dir == PDB_Desc) {
    while(!iter->_slot) {
      if(!page_near(pdb, PDB_Desc, &iter->_page, &iter->_first)) return false;
      iter->_slot = slots(pdb);
    }
    iter->_slot--;
    return true;
  }
  iter->_slot++;
  while(iter->_slot >= (iter->_page == pdb->_page ? pdb->_slot : slots(pdb))) {
    if(iter->_page == pdb->_page) return false;
    if(!page_near(pdb, PDB_Asc, &iter->_page, &iter->_first)) return false;
    iter->_slot = 0;
  }
  return true;
}

// A walk down starts at the write cursor, a walk up before slot `0` of the oldest page
status_t PDB_IterInit(PDB_t *pdb, PDB_Iter_t *iter, const PDB_Query_t *query)
{
  iter->_pdb = pdb;
  iter->_query = *query;
  iter->count = 0;
  iter->_number = 0;
  iter->_skipped = 0;
  iter->_done = pdb->_page == PDB_NONE;
  if(query->dir == PDB_Desc) {
    iter->_page = pdb->_page;
    iter->_first = pdb->_first;
    iter->_slot = pdb->_slot;
  }
  else {
    iter->_first = 0;
    iter->_slot = PDB_NONE;
    if(!iter->_done) iter->_done = !page_near(pdb, PDB_Asc, &iter->_page, &iter->_first);
  }
  return OK;
}

status_t PDB_IterNext(PDB_Iter_t *iter, void *out)
{
  PDB_t *pdb = iter->_pdb;
  uint32_t *record = (uint32_t *)iter->_record;
  while(!iter->_done && iter_step(iter)) {
    if(slot_load(pdb, iter->_page, iter->_first, iter->_slot, record) != PDB_Slot_Valid) continue;
    if(record[0] == iter->_number) continue; // a record a write took twice, its copy beside it
    iter->_number = record[0];
    #if(PDB_QUERY)
    const PDB_Query_t *query = &iter->_query;
    if(query->number_min && record[0] < query->number_min) continue;
    if(query->number_max && record[0] > query->number_max) continue;
    if(query->filter && !query->filter(record, query->filter_ctx)) continue;
    if(iter->_skipped < query->skip) {
      iter->_skipped++;
      continue;
    }
    #endif
    if(out) memcpy(out, record, pdb->record_size);
    iter->count++;
    #if(PDB_QUERY)
    if(query->limit && iter->count >= query->limit) iter->_done = true;
    #endif
    return OK;
  }
  iter->_done = true;
  return ERR;
}

const void *PDB_IterRef(PDB_Iter_t *iter)
{
  return iter->_record;
}

//------------------------------------------------------------------------------------ Select/Count
#if(PDB_QUERY)

uint32_t PDB_Select(PDB_t *pdb, const PDB_Query_t *query, void *out, uint32_t max)
{
  if(!max) return 0;
  PDB_Iter_t iter;
  PDB_Query_t q = *query;
  if(q.limit == 0 || q.limit > max) q.limit = max;
  PDB_IterInit(pdb, &iter, &q);
  uint8_t *ptr = (uint8_t *)out;
  while(!PDB_IterNext(&iter, ptr)) ptr += pdb->record_size;
  return iter.count;
}

uint32_t PDB_Count(PDB_t *pdb, const PDB_Query_t *query)
{
  PDB_Iter_t iter;
  PDB_IterInit(pdb, &iter, query);
  while(!PDB_IterNext(&iter, NULL));
  return iter.count;
}

#endif
//-------------------------------------------------------------------------------------------------

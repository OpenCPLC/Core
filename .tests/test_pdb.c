// .tests/test_pdb.c
// Flash log `PDB_t` with a CRC, on the host flash: records come back in the order written,
// newest or oldest first, through the queries a caller pages with,
// and a ring that wrapped keeps the full pages behind the one being written.
// After a power cut at any flash cycle of an insert, before it or halfway through it,
// the log holds a run of the newest records, the one in flight at most, none of them torn.

// A G081: 128kB of flash in pages of 2kB
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64

#include <string.h>

#include "test.h"
#include "flash.c"
#include "pdb.c"

// Record of the tests: the sort key `PDB_t` takes first, then words only a whole record holds.
// With its CRC it spans two doublewords, so a cut between them leaves a record cut short.
typedef struct {
  uint32_t key;
  uint32_t check[2];
} entry_t;

#define PAGES 3
#define PER_PAGE (FLASH_PAGE_SIZE / 16u) // a slot: the record, its CRC, padding to 16

// The ring full, then two of its pages recycled
#define SWEEP_INSERTS (PAGES * PER_PAGE + 140)

static PDB_t pdb;
static entry_t got[PAGES * PER_PAGE];
static volatile uint32_t sweep_done; // inserts of a sweep acknowledged before the cut

static entry_t entry(uint32_t key) { return (entry_t){ key, { key * 2654435761u, ~key } }; }

// Power-up over whatever the flash holds: a new descriptor on the last pages, as after a reset
static void power_up(void)
{
  pdb = (PDB_t){
    .page_start = FLASH_PAGES - PAGES,
    .page_count = PAGES,
    .payload_size = sizeof(entry_t),
    .crc = &crc16_modbus
  };
  TEST_Check(PDB_Init(&pdb) == OK, "init");
}

// Records of keys `from` up to `to`, exclusive
static void insert_keys(uint32_t from, uint32_t to)
{
  for(uint32_t key = from; key < to; key++) {
    entry_t record = entry(key);
    TEST_Check(PDB_Insert(&pdb, &record) == OK, "key:%u", key);
  }
}

// Every record into `got` in the order of `dir`, returns their count
static uint32_t walk(PDB_Dir_t dir)
{
  PDB_Query_t query = { .dir = dir };
  return PDB_Select(&pdb, &query, got, array_len(got));
}

// The log holds keys `oldest` through `newest`, whole, newest first and oldest first alike
static void holds_keys(uint32_t oldest, uint32_t newest)
{
  uint32_t count = walk(PDB_Desc);
  TEST_Check(count == newest - oldest + 1, "holds %u, keys %u to %u", count, oldest, newest);
  for(uint32_t i = 0; i < count; i++) {
    entry_t expected = entry(newest - i);
    TEST_Check(!memcmp(&got[i], &expected, sizeof(expected)), "newest first, record %u", i);
  }
  TEST_Check(walk(PDB_Asc) == count, "oldest first");
  for(uint32_t i = 0; i < count; i++) {
    entry_t expected = entry(oldest + i);
    TEST_Check(!memcmp(&got[i], &expected, sizeof(expected)), "oldest first, record %u", i);
  }
}

//--------------------------------------------------------------------------------------------- Log

static void records_come_back_in_order(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Equal(walk(PDB_Desc), 0, "blank log");
  insert_keys(0, 100);
  holds_keys(0, 99);
  power_up();
  holds_keys(0, 99);
  insert_keys(100, 110);
  holds_keys(0, 109);
  TEST_Check(PDB_Delete(&pdb) == OK);
  TEST_Equal(walk(PDB_Desc), 0, "deleted");
  power_up();
  TEST_Equal(walk(PDB_Desc), 0, "deleted, after a power cycle");
}

// A ring that wrapped keeps the full pages behind the one being written, and that one's records
static void ring_keeps_the_newest_pages(void)
{
  FLASH_HostBlank();
  power_up();
  insert_keys(0, 1000);
  uint32_t kept = (PAGES - 1) * PER_PAGE + 1000 % PER_PAGE;
  holds_keys(1000 - kept, 999);
  power_up();
  holds_keys(1000 - kept, 999);
}

// Keys divisible by the number the context points at
static bool divisible(const void *record, void *ctx)
{
  return ((const entry_t *)record)->key % *(const uint32_t *)ctx == 0;
}

// A key range, paged by skip and limit in both directions, a filter, a buffer too small
static void queries_page_through_the_log(void)
{
  FLASH_HostBlank();
  power_up();
  insert_keys(1, 51);
  PDB_Query_t query = { .key_min = 31, .key_max = 40, .dir = PDB_Desc };
  TEST_Equal(PDB_Count(&pdb, &query), 10, "keys 31 to 40");
  query.skip = 3;
  query.limit = 4;
  TEST_Equal(PDB_Select(&pdb, &query, got, array_len(got)), 4, "second page, newest first");
  for(uint32_t i = 0; i < 4; i++) TEST_Check(got[i].key == 37 - i, "newest first, record %u", i);
  query.dir = PDB_Asc;
  TEST_Equal(PDB_Select(&pdb, &query, got, array_len(got)), 4, "second page, oldest first");
  for(uint32_t i = 0; i < 4; i++) TEST_Check(got[i].key == 34 + i, "oldest first, record %u", i);
  uint32_t ten = 10;
  query = (PDB_Query_t){ .dir = PDB_Desc, .filter = divisible, .filter_ctx = &ten };
  TEST_Equal(PDB_Count(&pdb, &query), 5, "keys divisible by ten");
  TEST_Equal(PDB_Select(&pdb, &query, got, 2), 2, "buffer for two");
  TEST_Check(got[0].key == 50 && got[1].key == 40, "the two newest");
  PDB_Iter_t iter;
  PDB_IterInit(&pdb, &iter, &query);
  entry_t record;
  TEST_Check(PDB_IterNext(&iter, &record) == OK && record.key == 50);
  TEST_Bytes(PDB_IterRef(&iter), &record, sizeof(record), "reference to the record copied");
}

// A slot that refuses its write is skipped: the record lands one slot on, whole
static void refused_slot_skipped(void)
{
  FLASH_HostBlank();
  power_up();
  insert_keys(0, 3);
  TEST_Check(FLASH_Write(pdb._pointer, 0, 0) == OK, "slot taken behind the log");
  insert_keys(3, 4);
  power_up();
  holds_keys(0, 3);
}

// An advance whose erase failed leaves the cursor past the last slot of the ring,
// where no step lands: a walk still ends, after every slot of the ring at most
static void iterator_ends_past_the_last_slot(void)
{
  FLASH_HostBlank();
  power_up();
  insert_keys(0, (PAGES - 1) * PER_PAGE);
  pdb._pointer = pdb._pointer_end; // the last page, its advance pending
  PDB_Query_t query = { .dir = PDB_Desc };
  PDB_Iter_t iter;
  PDB_IterInit(&pdb, &iter, &query);
  uint32_t limit = PAGES * PER_PAGE, steps = 0;
  entry_t record;
  while(steps <= limit && PDB_IterNext(&iter, &record) == OK) steps++;
  TEST_Check(steps <= limit, "%u steps over %u slots", steps, limit);
}

//-------------------------------------------------------------------------------------- Power cuts

// Inserts of a sweep, until the power goes or an insert fails
static void sweep_inserts(void)
{
  for(sweep_done = 0; sweep_done < SWEEP_INSERTS; sweep_done++) {
    entry_t record = entry(sweep_done);
    if(PDB_Insert(&pdb, &record)) return;
  }
}

// Flash cycles of a whole sweep, the cut points.
// A cut run repeats this one up to its cut, so its inserts are checked here, once.
static uint32_t sweep_cycles(void)
{
  FLASH_HostBlank();
  power_up();
  uint32_t before = FLASH_HostCycles();
  sweep_inserts();
  TEST_Check(sweep_done == SWEEP_INSERTS, "insert:%u refused", sweep_done);
  return FLASH_HostCycles() - before;
}

// Sweep cut at flash cycle `cut`, where `where` says in it, then a power-up.
// Returns the inserts acknowledged before the cut, the next one was in flight.
static uint32_t sweep_cut(uint32_t cut, FLASH_Cut_t where)
{
  FLASH_HostBlank();
  power_up();
  FLASH_HostCut((int32_t)cut, where, sweep_inserts);
  power_up();
  return sweep_done;
}

// Records at the start of `got` that run whole down from key `newest`, one key apart
static uint32_t run_length(uint32_t count, uint32_t newest)
{
  uint32_t n = 0;
  while(n < count) {
    entry_t expected = entry(newest - n);
    if(memcmp(&got[n], &expected, sizeof(expected))) break;
    n++;
  }
  return n;
}

// A run of the newest records, whole, at least the full pages behind the one being written,
// less the slot a torn record takes there.
// The newest is the last one acknowledged, or the one in flight.
static void newest_run_kept(uint32_t cut, uint32_t done)
{
  uint32_t count = walk(PDB_Desc);
  uint32_t least = minv(done, (PAGES - 1) * PER_PAGE - 1);
  TEST_Check(count >= least, "cut:%u insert:%u holds %u, at least %u", cut, done, count, least);
  if(!count) return;
  uint32_t newest = got[0].key;
  TEST_Check(newest + 1 == done || newest == done, "cut:%u insert:%u newest %u",
    cut, done, newest);
  uint32_t whole = run_length(count, newest);
  TEST_Check(whole == count, "cut:%u insert:%u record %u of %u breaks the run",
    cut, done, whole, count);
}

// The log goes on after a recovery: a new record is the newest, through the next power cycle
static void log_goes_on(uint32_t cut)
{
  entry_t record = entry(SWEEP_INSERTS);
  TEST_Check(PDB_Insert(&pdb, &record) == OK, "cut:%u", cut);
  power_up();
  TEST_Check(walk(PDB_Desc) && got[0].key == SWEEP_INSERTS, "cut:%u", cut);
}

// Every cut point of a sweep, the cut landing where `where` says
static void sweep_survives(FLASH_Cut_t where)
{
  uint32_t total = sweep_cycles();
  for(uint32_t cut = 0; cut <= total; cut++) {
    uint32_t done = sweep_cut(cut, where);
    newest_run_kept(cut, done);
    log_goes_on(cut);
  }
}

static void insert_survives_a_cut(void) { sweep_survives(FLASH_Cut_Before); }
static void insert_survives_a_torn_cut(void) { sweep_survives(FLASH_Cut_Unreadable); }
static void insert_survives_a_miscorrected_cut(void) { sweep_survives(FLASH_Cut_Corrected); }

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(records_come_back_in_order);
  TEST(ring_keeps_the_newest_pages);
  TEST(queries_page_through_the_log);
  TEST(refused_slot_skipped);
  TEST(iterator_ends_past_the_last_slot);
  TEST(insert_survives_a_cut);
  TEST(insert_survives_a_torn_cut);
  TEST(insert_survives_a_miscorrected_cut);
}

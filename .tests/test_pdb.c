// .tests/test_pdb.c
// Database `PDB_t` of format `1` on the host flash, a ring and `no_wrap`.
// A page opens with its header, its records numbered from `1` and sealed by a CRC-32,
// byte for byte as the format lays them out, and they come back in the order of their numbers,
// through the queries a caller pages with.
// Init only reads, and the first record after it zeroes a slot first.
// A page under a header of another kind, record size or format stays as it is,
// and a page without a header holds nothing and is erased whole before a record goes in.
// After a power cut at any flash cycle, wherever in it,
// the base holds a run of the newest records, the one in flight at most, none of them torn,
// and goes on.

// A G081: 128kB of flash in pages of 2kB
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64

#include <stddef.h>
#include <string.h>

#include "test.h"
#include "crc.h"
#include "flash.c"
#include "pdb.c"

// Record of the tests: its number, words only a whole record holds, its CRC-32.
// It spans two doublewords, so a cut between them leaves a record cut short.
typedef struct {
  uint32_t number;
  uint32_t check[2];
  uint32_t crc;
} entry_t;

#define KIND 7
#define MARK 0x01424450u                          // "PDB" and format `1`, as a header opens
#define PAGES 3
#define PAGE (FLASH_PAGES - PAGES)                // first page of the base
#define PER_PAGE ((FLASH_PAGE_SIZE - 16u) / 16u)  // slots of a page: its header, then 16B each

static PDB_t layout; // base a power-up builds, set by each case
static PDB_t pdb;
static entry_t got[PAGES * PER_PAGE];

// Record numbered `n` as PDB stores it: words only a whole record holds, then its CRC-32
static entry_t sealed(uint32_t n)
{
  entry_t record = { n, { n * 2654435761u, ~n }, 0 };
  record.crc = CRC_Run(&crc32_iso, &record, offsetof(entry_t, crc));
  return record;
}

// Record of number `n` as a caller gives it, its number and CRC-32 left to PDB
static entry_t given(uint32_t n) { return (entry_t){ 0, { n * 2654435761u, ~n }, 0 }; }

static void ring(void)
{
  layout = (PDB_t){ .page_start = PAGE, .page_count = PAGES,
    .record_size = sizeof(entry_t), .kind = KIND };
}

static void no_wrap(uint16_t pages)
{
  layout = (PDB_t){ .page_start = PAGE, .page_count = pages,
    .record_size = sizeof(entry_t), .kind = KIND, .no_wrap = true };
}

// Power-up over whatever the flash holds: a new descriptor, as after a reset
static void power_up(void)
{
  pdb = layout;
  TEST_Check(PDB_Init(&pdb) == OK, "init");
}

// Records numbered `from` up to `to`, exclusive
static void insert_numbers(uint32_t from, uint32_t to)
{
  for(uint32_t n = from; n < to; n++) {
    entry_t record = given(n);
    TEST_Check(PDB_Insert(&pdb, &record) == OK, "number:%u", n);
  }
}

// Every record into `got` in the order of `dir`, returns their count
static uint32_t walk(PDB_Dir_t dir)
{
  PDB_Query_t query = { .dir = dir };
  return PDB_Select(&pdb, &query, got, array_len(got));
}

// The base holds numbers `oldest` through `newest`, whole, newest first and oldest first alike
static void holds(uint32_t oldest, uint32_t newest)
{
  uint32_t count = walk(PDB_Desc);
  TEST_Check(count == newest - oldest + 1, "holds %u, numbers %u to %u", count, oldest, newest);
  for(uint32_t i = 0; i < count; i++) {
    entry_t expected = sealed(newest - i);
    TEST_Check(!memcmp(&got[i], &expected, sizeof(expected)), "newest first, record %u", i);
  }
  TEST_Check(walk(PDB_Asc) == count, "oldest first");
  for(uint32_t i = 0; i < count; i++) {
    entry_t expected = sealed(oldest + i);
    TEST_Check(!memcmp(&got[i], &expected, sizeof(expected)), "oldest first, record %u", i);
  }
}

static uint32_t slot_at(uint16_t page, uint16_t slot) {
  return FLASH_GetAddress(page, 16 + 16 * slot);
}

// Slot holds `record`, ECC correcting none of it
static bool slot_holds(uint32_t addr, const entry_t *record)
{
  entry_t read;
  bool corrected;
  if(FLASH_Copy(addr, &read, sizeof(read), &corrected) || corrected) return false;
  return !memcmp(&read, record, sizeof(read));
}

static bool slot_zeroed(uint32_t addr)
{
  entry_t zeros = { 0 };
  return slot_holds(addr, &zeros);
}

// Header on `page` as the format lays it out, the page erased under it as by a programmer
static void header_put(uint16_t page, uint32_t mark, uint16_t kind, uint16_t size, uint32_t first)
{
  uint32_t head[4] = { mark, kind | (uint32_t)size << 16, first, 0 };
  head[3] = CRC_Run(&crc32_iso, head, 12);
  FLASH_HostProgram(FLASH_GetAddress(page, 0), (const uint8_t *)head, sizeof(head));
}

static void record_put(uint16_t page, uint16_t slot, entry_t record)
{
  const uint32_t *word = (const uint32_t *)&record;
  uint32_t addr = slot_at(page, slot);
  TEST_Check(FLASH_Write(addr, word[0], word[1]) == OK, "page:%u slot:%u", page, slot);
  TEST_Check(FLASH_Write(addr + 8, word[2], word[3]) == OK, "page:%u slot:%u", page, slot);
}

//--------------------------------------------------------------------------------------------- Log

// Records numbered from `1` come back in order across power-ups, a delete starts again from `1`
static void records_come_back_in_order(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  TEST_Equal(walk(PDB_Desc), 0, "blank base");
  insert_numbers(1, 101);
  holds(1, 100);
  power_up();
  holds(1, 100);
  insert_numbers(101, 111);
  holds(1, 110);
  TEST_Check(PDB_Delete(&pdb) == OK);
  TEST_Equal(walk(PDB_Desc), 0, "deleted");
  power_up();
  TEST_Equal(walk(PDB_Desc), 0, "deleted, after a power-up");
  insert_numbers(1, 3);
  holds(1, 2);
}

// A ring erases its oldest page when the newest fills: the full pages behind the newest stay
static void ring_erases_the_oldest_page(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 1001);
  uint32_t kept = (PAGES - 1) * PER_PAGE + 1000 % PER_PAGE;
  holds(1001 - kept, 1000);
  power_up();
  holds(1001 - kept, 1000);
}

// Numbers divisible by the number the context points at
static bool divisible(const void *record, void *ctx)
{
  return ((const entry_t *)record)->number % *(const uint32_t *)ctx == 0;
}

// A number range, paged by skip and limit in both directions, a filter, a buffer too small
static void queries_page_through_the_base(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 51);
  PDB_Query_t query = { .number_min = 31, .number_max = 40, .dir = PDB_Desc };
  TEST_Equal(PDB_Count(&pdb, &query), 10, "numbers 31 to 40");
  query.skip = 3;
  query.limit = 4;
  TEST_Equal(PDB_Select(&pdb, &query, got, array_len(got)), 4, "second page, newest first");
  for(uint32_t i = 0; i < 4; i++) {
    TEST_Check(got[i].number == 37 - i, "newest first, record %u", i);
  }
  query.dir = PDB_Asc;
  TEST_Equal(PDB_Select(&pdb, &query, got, array_len(got)), 4, "second page, oldest first");
  for(uint32_t i = 0; i < 4; i++) {
    TEST_Check(got[i].number == 34 + i, "oldest first, record %u", i);
  }
  uint32_t ten = 10;
  query = (PDB_Query_t){ .dir = PDB_Desc, .filter = divisible, .filter_ctx = &ten };
  TEST_Equal(PDB_Count(&pdb, &query), 5, "numbers divisible by ten");
  TEST_Equal(PDB_Select(&pdb, &query, got, 2), 2, "buffer for two");
  TEST_Check(got[0].number == 50 && got[1].number == 40, "the two newest");
  PDB_Iter_t iter;
  PDB_IterInit(&pdb, &iter, &query);
  entry_t record;
  TEST_Check(PDB_IterNext(&iter, &record) == OK && record.number == 50);
  TEST_Bytes(PDB_IterRef(&iter), &record, sizeof(record), "reference to the record copied");
}

// A config PDB cannot lay out is refused
static void init_refuses_a_bad_config(void)
{
  FLASH_HostBlank();
  ring();
  PDB_t bad[] = { layout, layout, layout, layout, layout, layout };
  bad[0].record_size = 20;
  bad[1].record_size = 8;
  bad[2].record_size = PDB_RECORD_LIMIT + 8;
  bad[3].page_count = 1;
  bad[4].page_count = 0;
  bad[5].page_start = FLASH_PAGES - PAGES + 1;
  for(uint8_t i = 0; i < array_len(bad); i++) TEST_Check(PDB_Init(&bad[i]) == ERR, "config %u", i);
  no_wrap(1);
  pdb = layout;
  TEST_Check(PDB_Init(&pdb) == OK, "one page without the ring");
}

//-------------------------------------------------------------------------------------- No wrap

// Without the ring a full base refuses a record and erases nothing; its records stay
static void no_wrap_refuses_when_full(void)
{
  FLASH_HostBlank();
  no_wrap(1);
  power_up();
  insert_numbers(1, PER_PAGE + 1);
  entry_t record = given(PER_PAGE + 1);
  uint32_t cycles = FLASH_HostCycles();
  TEST_Check(PDB_Insert(&pdb, &record) == ERR, "full");
  TEST_Equal(FLASH_HostCycles(), cycles, "nothing erased or written");
  holds(1, PER_PAGE);
  power_up();
  TEST_Check(PDB_Insert(&pdb, &record) == ERR, "full, after a power-up");
  holds(1, PER_PAGE);
  no_wrap(2);
  power_up();
  insert_numbers(PER_PAGE + 1, 2 * PER_PAGE + 1);
  record = given(2 * PER_PAGE + 1);
  TEST_Check(PDB_Insert(&pdb, &record) == ERR, "both pages full");
  holds(1, 2 * PER_PAGE);
}

// Ten power-ups, two records after each:
// every power-up but the first costs the slot the first record zeroes,
// and the newest valid record is the one a walk gives first.
static void no_wrap_pays_a_slot_a_power_up(void)
{
  FLASH_HostBlank();
  no_wrap(1);
  uint32_t n = 0;
  for(uint8_t boot = 0; boot < 10; boot++) {
    power_up();
    insert_numbers(n + 1, n + 3);
    n += 2;
  }
  holds(1, n);
  entry_t record = given(n + 1);
  while(PDB_Insert(&pdb, &record) == OK) record = given(++n + 1);
  TEST_Equal(n, PER_PAGE - 9, "records a page takes after nine zeroed slots");
  holds(1, n);
}

//------------------------------------------------------------------------------------------ Writes

static uint32_t next; // number the next record of a cut run takes

static void insert_next(void)
{
  entry_t record = given(next);
  if(!PDB_Insert(&pdb, &record)) next++;
}

// Init reads only: whatever the pages hold, it takes no erase or program cycle
static void init_only_reads(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 2 * PER_PAGE);
  next = 2 * PER_PAGE;
  FLASH_HostCut(1, FLASH_Cut_Unreadable, insert_next);
  header_put(PAGE + 2, MARK, KIND + 1, 16, 1);
  uint32_t cycles = FLASH_HostCycles();
  power_up();
  power_up();
  TEST_Equal(FLASH_HostCycles(), cycles, "no cycle");
  holds(1, 2 * PER_PAGE - 1);
}

// The first record after a power-up zeroes the first free slot and goes in the next one,
// the records after it slot by slot.
static void first_record_after_start_zeroes_a_slot(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 4);
  power_up();
  insert_numbers(4, 6);
  TEST_Check(slot_zeroed(slot_at(PAGE, 3)), "first free slot zeroed");
  entry_t four = sealed(4), five = sealed(5);
  TEST_Check(slot_holds(slot_at(PAGE, 4), &four), "record 4 past it");
  TEST_Check(slot_holds(slot_at(PAGE, 5), &five), "record 5 next to it");
  holds(1, 5);
}

// A write cut at its very start can read free, erased one read and unreadable the next:
// the first record after the power-up zeroes it and goes past it, whole.
static void write_cut_at_its_start_is_zeroed(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 4);
  next = 4;
  FLASH_HostCut(0, FLASH_Cut_Unstable, insert_next);
  power_up();
  insert_numbers(4, 5);
  TEST_Check(slot_zeroed(slot_at(PAGE, 3)), "torn slot zeroed");
  holds(1, 4);
  power_up();
  holds(1, 4);
}

// A record that does not read back clean goes once more into the next slot, same number:
// ECC failing it or correcting it, and a copy that reads valid counts once.
static void record_that_does_not_read_back_goes_again(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 3);
  FLASH_HostAge(slot_at(PAGE, 2), 2);
  insert_numbers(3, 4);
  entry_t three = sealed(3), four = sealed(4);
  TEST_Check(slot_holds(slot_at(PAGE, 3), &three), "record 3 in the next slot");
  holds(1, 3);
  FLASH_HostAge(slot_at(PAGE, 4) + 8, 1);
  insert_numbers(4, 5);
  TEST_Check(slot_holds(slot_at(PAGE, 5), &four), "record 4 again in the next slot");
  holds(1, 4);
  power_up();
  holds(1, 4);
  insert_numbers(5, 6);
  holds(1, 5);
}

// A slot that reads ones only through ECC is not free: no record goes over it
static void corrected_ones_are_not_free(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 4);
  FLASH_HostAge(slot_at(PAGE, 3), 1);
  power_up();
  insert_numbers(4, 5);
  TEST_Check(slot_zeroed(slot_at(PAGE, 4)), "first free slot zeroed");
  holds(1, 4);
  uint8_t ones[16];
  bool corrected;
  TEST_Check(FLASH_Copy(slot_at(PAGE, 3), ones, sizeof(ones), &corrected) == OK && corrected,
    "slot left as it was");
}

//------------------------------------------------------------------------------------------- Pages

// A page under a header of another kind, record size or format is never read, written or erased,
// the ring going round past it, and a delete erases it.
static void foreign_pages_left_alone(void)
{
  static const struct { uint32_t mark; uint16_t kind, size; } FOREIGN[] = {
    { MARK,        KIND + 1, 16 }, // another kind
    { MARK,        KIND,     32 }, // another record size
    { 0x02424450u, KIND,     16 }, // format `2`
  };
  static uint8_t page[FLASH_PAGE_SIZE];
  for(uint8_t i = 0; i < array_len(FOREIGN); i++) {
    FLASH_HostBlank();
    header_put(PAGE + 1, FOREIGN[i].mark, FOREIGN[i].kind, FOREIGN[i].size, 1);
    record_put(PAGE + 1, 0, sealed(1));
    memcpy(page, FLASH_Ref(FLASH_GetAddress(PAGE + 1, 0)), sizeof(page));
    ring();
    power_up();
    TEST_Equal(walk(PDB_Desc), 0, "its records stay out");
    uint32_t n = 3 * PER_PAGE + 10;
    insert_numbers(1, n + 1);
    holds(n - PER_PAGE - 10 + 1, n);
    uint8_t *foreign = FLASH_Ref(FLASH_GetAddress(PAGE + 1, 0));
    TEST_Check(!memcmp(foreign, page, sizeof(page)), "foreign %u left as it was", i);
    TEST_Check(PDB_Delete(&pdb) == OK);
    memset(page, 0xFF, sizeof(page));
    TEST_Check(!memcmp(foreign, page, sizeof(page)), "foreign %u erased by a delete", i);
  }
}

// A page left without a valid record: its one record aged unreadable,
// every write after a power-up torn.
// The next page still numbers past its first,
// so the records written there come back after a power-up and stay.
static void page_left_without_a_valid_record_keeps_the_order(void)
{
  FLASH_HostBlank();
  layout = (PDB_t){ .page_start = PAGE, .page_count = 2,
    .record_size = sizeof(entry_t), .kind = KIND };
  power_up();
  insert_numbers(1, 2);
  FLASH_HostAge(slot_at(PAGE, 0), 2);
  next = 2;
  for(uint8_t boot = 0; boot < 70; boot++) {
    power_up();
    FLASH_HostCut(2, FLASH_Cut_Unreadable, insert_next);
  }
  power_up();
  TEST_Equal(walk(PDB_Desc), 0, "no valid record yet");
  insert_numbers(2, 7);
  holds(2, 6);
  power_up();
  holds(2, 6);
  insert_numbers(7, 8);
  power_up();
  holds(2, 7);
}

// A ring with no page to move on to but its newest refuses a record rather than erase it
static void ring_without_a_page_to_move_on_refuses(void)
{
  FLASH_HostBlank();
  header_put(PAGE + 1, MARK, KIND + 1, 16, 1);
  layout = (PDB_t){ .page_start = PAGE, .page_count = 2,
    .record_size = sizeof(entry_t), .kind = KIND };
  power_up();
  insert_numbers(1, PER_PAGE + 1);
  entry_t record = given(PER_PAGE + 1);
  TEST_Check(PDB_Insert(&pdb, &record) == ERR, "no page to move on to");
  holds(1, PER_PAGE);
}

// Records on a page without a valid header hold nothing,
// and the page is erased whole before a record goes in.
static void page_without_a_header_holds_nothing(void)
{
  FLASH_HostBlank();
  record_put(PAGE, 0, sealed(1));
  record_put(PAGE, 1, sealed(2));
  ring();
  power_up();
  TEST_Equal(walk(PDB_Desc), 0, "no header, no records");
  insert_numbers(1, 2);
  holds(1, 1);
}

// A page a programmer filled with ones reads blank, but takes records only once erased whole
static void ones_from_a_programmer_take_a_full_erase(void)
{
  static uint8_t ones[FLASH_PAGE_SIZE];
  memset(ones, 0xFF, sizeof(ones));
  FLASH_HostBlank();
  FLASH_HostProgram(FLASH_GetAddress(PAGE, 0), ones, sizeof(ones));
  ring();
  power_up();
  insert_numbers(1, 4);
  holds(1, 3);
}

// A record sealed right but numbered `0` or outside its page is not valid
static void numbers_outside_the_page_are_not_valid(void)
{
  FLASH_HostBlank();
  header_put(PAGE, MARK, KIND, 16, 100);
  record_put(PAGE, 0, sealed(99));
  record_put(PAGE, 1, sealed(0));
  record_put(PAGE, 2, sealed(100 + PER_PAGE));
  record_put(PAGE, 3, sealed(100));
  ring();
  power_up();
  holds(100, 100);
  insert_numbers(101, 102);
  holds(100, 101);
}

//------------------------------------------------------------------------------------------ Golden

// Page of format `1`: kind `7`, records of 16B numbered from `1` with the words of `sealed`,
// each CRC-32 (ISO-HDLC) worked out apart from this code.
static const uint8_t GOLDEN[] = {
  0x50, 0x44, 0x42, 0x01, 0x07, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x59, 0xA4, 0x1D, // header: "PDB", 1, kind, size, first, CRC
  0x01, 0x00, 0x00, 0x00, 0xB1, 0x79, 0x37, 0x9E, 0xFE, 0xFF, 0xFF, 0xFF, 0x23, 0x20, 0x92, 0x92, // record 1
  0x02, 0x00, 0x00, 0x00, 0x62, 0xF3, 0x6E, 0x3C, 0xFD, 0xFF, 0xFF, 0xFF, 0xB7, 0x0C, 0x2B, 0x72, // record 2
  0x03, 0x00, 0x00, 0x00, 0x13, 0x6D, 0xA6, 0xDA, 0xFC, 0xFF, 0xFF, 0xFF, 0x74, 0x15, 0x10, 0xD4, // record 3
};

// A page laid out by the format reads as its records, and the base numbers on from them
static void golden_page_reads(void)
{
  FLASH_HostBlank();
  FLASH_HostProgram(FLASH_GetAddress(PAGE, 0), GOLDEN, sizeof(GOLDEN));
  ring();
  power_up();
  holds(1, 3);
  insert_numbers(4, 5);
  holds(1, 4);
}

// Records into a blank base lay out the page byte for byte as the format does, ones after them
static void golden_page_written(void)
{
  static uint8_t page[FLASH_PAGE_SIZE];
  memset(page, 0xFF, sizeof(page));
  memcpy(page, GOLDEN, sizeof(GOLDEN));
  FLASH_HostBlank();
  ring();
  power_up();
  insert_numbers(1, 4);
  TEST_Bytes(FLASH_Ref(FLASH_GetAddress(PAGE, 0)), page, sizeof(page), "page of format 1");
}

//-------------------------------------------------------------------------------------- Power cuts

#define SWEEP_INSERTS (PAGES * PER_PAGE + 140) // the ring full, then two of its pages recycled

static uint32_t done; // records of a sweep acknowledged before the cut

static void sweep_inserts(void)
{
  for(done = 0; done < SWEEP_INSERTS; done++) {
    entry_t record = given(done + 1);
    if(PDB_Insert(&pdb, &record)) return;
  }
}

// Records at the start of `got` that run whole down from number `newest`, one number apart
static uint32_t run_length(uint32_t count, uint32_t newest)
{
  uint32_t n = 0;
  while(n < count) {
    entry_t expected = sealed(newest - n);
    if(memcmp(&got[n], &expected, sizeof(expected))) break;
    n++;
  }
  return n;
}

// Newest first: a run of whole records, the newest the last acknowledged or the one in flight.
// Returns their count, the number of the newest in `*newest`.
static uint32_t newest_run(const char *what, uint32_t cut, uint32_t acked, uint32_t *newest)
{
  uint32_t count = walk(PDB_Desc);
  *newest = count ? got[0].number : 0;
  TEST_Check(*newest == acked || *newest == acked + 1, "%s cut:%u acked %u, newest %u",
    what, cut, acked, *newest);
  TEST_Check(run_length(count, *newest) == count, "%s cut:%u run broken", what, cut);
  return count;
}

// The base goes on after a recovery: a new record is the newest, through the next power-up
static void base_goes_on(const char *what, uint32_t cut, uint32_t newest)
{
  entry_t record = given(newest + 1);
  TEST_Check(PDB_Insert(&pdb, &record) == OK, "%s cut:%u goes on", what, cut);
  power_up();
  TEST_Check(walk(PDB_Desc) && got[0].number == newest + 1, "%s cut:%u goes on", what, cut);
}

// Every cut point of a sweep over the ring, the cut landing where `where` says:
// at least the full pages behind the newest stay, less the slot a torn record takes there.
static void ring_sweep(FLASH_Cut_t where)
{
  FLASH_HostBlank();
  ring();
  power_up();
  uint32_t before = FLASH_HostCycles();
  sweep_inserts();
  TEST_Check(done == SWEEP_INSERTS, "insert:%u refused", done);
  uint32_t total = FLASH_HostCycles() - before;
  for(uint32_t cut = 0; cut <= total; cut++) {
    FLASH_HostBlank();
    power_up();
    FLASH_HostCut((int32_t)cut, where, sweep_inserts);
    power_up();
    uint32_t newest;
    uint32_t count = newest_run("ring", cut, done, &newest);
    uint32_t least = minv(done, (PAGES - 1) * PER_PAGE - 1);
    TEST_Check(count >= least, "ring cut:%u holds %u, at least %u", cut, count, least);
    base_goes_on("ring", cut, newest);
  }
}

static uint32_t fill_done; // records acknowledged since the board

static void fill_inserts(void)
{
  for(fill_done = 0; ; fill_done++) {
    entry_t record = given(5 + fill_done + 1);
    if(PDB_Insert(&pdb, &record)) return;
  }
}

// One page without the ring, records 1 to 5 on it, then records until it is full:
// a cut anywhere loses no record acknowledged before it.
static void no_wrap_sweep(FLASH_Cut_t where)
{
  no_wrap(1);
  FLASH_HostBlank();
  power_up();
  insert_numbers(1, 6);
  uint32_t before = FLASH_HostCycles();
  fill_inserts();
  uint32_t total = FLASH_HostCycles() - before;
  for(uint32_t cut = 0; cut <= total; cut++) {
    FLASH_HostBlank();
    power_up();
    insert_numbers(1, 6);
    FLASH_HostCut((int32_t)cut, where, fill_inserts);
    power_up();
    uint32_t newest;
    uint32_t count = newest_run("no_wrap", cut, 5 + fill_done, &newest);
    TEST_Check(count == newest, "no_wrap cut:%u holds %u of %u", cut, count, newest);
  }
}

static void three_inserts(void)
{
  for(done = 0; done < 3; done++) {
    entry_t record = given(10 + done + 1);
    if(PDB_Insert(&pdb, &record)) return;
  }
}

// Records 1 to 10, then a power-up
static void board_after_start(void)
{
  FLASH_HostBlank();
  power_up();
  insert_numbers(1, 11);
  power_up();
}

// Three records after the power-up:
// a cut anywhere in them, the zeroing included, loses none of the ten.
static void after_start_sweep(FLASH_Cut_t where)
{
  ring();
  board_after_start();
  uint32_t before = FLASH_HostCycles();
  three_inserts();
  uint32_t total = FLASH_HostCycles() - before;
  for(uint32_t cut = 0; cut <= total; cut++) {
    board_after_start();
    FLASH_HostCut((int32_t)cut, where, three_inserts);
    power_up();
    uint32_t newest;
    uint32_t count = newest_run("after start", cut, 10 + done, &newest);
    TEST_Check(count == newest, "after start cut:%u holds %u of %u", cut, count, newest);
    base_goes_on("after start", cut, newest);
  }
}

static void insert_survives_a_cut(void)
{
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Late; where++) ring_sweep(where);
}

static void no_wrap_survives_a_cut(void)
{
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Late; where++) no_wrap_sweep(where);
}

static void first_records_after_start_survive_a_cut(void)
{
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Late; where++) {
    after_start_sweep(where);
  }
}

static bool refused; // an insert of a cut run refused without a cut

static void endless_inserts(void)
{
  while(1) {
    entry_t record = given(next);
    if(PDB_Insert(&pdb, &record)) {
      refused = true;
      return;
    }
    next++;
  }
}

static uint32_t xorshift(uint32_t x)
{
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

// Thousands of cuts at random cycles, of random kinds, each followed by a power-up:
// the base comes up with a run of the newest records, whole,
// the newest the last acknowledged or the one in flight,
// the oldest moving on by the records of one page at most.
static void random_cuts_keep_the_run(void)
{
  FLASH_HostBlank();
  ring();
  power_up();
  uint32_t seed = 2463534242u, oldest = 0, newest = 0;
  for(uint32_t round = 0; round < 4000; round++) {
    seed = xorshift(seed);
    next = newest + 1;
    refused = false;
    FLASH_HostCut((int32_t)(seed % 48), (FLASH_Cut_t)((seed >> 8) % 5), endless_inserts);
    TEST_Check(!refused, "round:%u insert:%u refused", round, next);
    power_up();
    uint32_t was = oldest;
    uint32_t count = newest_run("random", round, next - 1, &newest);
    oldest = count ? newest - count + 1 : 0;
    if(was) TEST_Check(oldest >= was && oldest <= was + PER_PAGE, "round:%u oldest %u, was %u",
      round, oldest, was);
  }
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(records_come_back_in_order);
  TEST(ring_erases_the_oldest_page);
  TEST(queries_page_through_the_base);
  TEST(init_refuses_a_bad_config);
  TEST(no_wrap_refuses_when_full);
  TEST(no_wrap_pays_a_slot_a_power_up);
  TEST(init_only_reads);
  TEST(first_record_after_start_zeroes_a_slot);
  TEST(write_cut_at_its_start_is_zeroed);
  TEST(record_that_does_not_read_back_goes_again);
  TEST(corrected_ones_are_not_free);
  TEST(foreign_pages_left_alone);
  TEST(page_left_without_a_valid_record_keeps_the_order);
  TEST(ring_without_a_page_to_move_on_refuses);
  TEST(page_without_a_header_holds_nothing);
  TEST(ones_from_a_programmer_take_a_full_erase);
  TEST(numbers_outside_the_page_are_not_valid);
  TEST(golden_page_reads);
  TEST(golden_page_written);
  TEST(insert_survives_a_cut);
  TEST(no_wrap_survives_a_cut);
  TEST(first_records_after_start_survive_a_cut);
  TEST(random_cuts_keep_the_run);
}

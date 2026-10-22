// .tests/test_eeprom.c
// EEPROM emulation on the host flash: the newest value of a key wins,
// through rewrites of the halves and power cycles alike.
// After a power cut at any flash cycle of a write, wherever in it,
// every other key reads as it was and the key in flight its old value or the new one.
// A rewrite erases its target whole, also a page an erase cut late left reading blank.

// A G081: 128kB of flash in pages of 2kB
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64

#include "test.h"
#include "flash.c"
#include "eeprom.c"

#define ABSENT 0xFFFF0000u  // default of a read, never a value written here
#define KEYS 8              // keys of a sweep, written in turn

// A half holds 511 writes, so a sweep runs two rewrites: of a fresh half and of a rewritten one
#define SWEEP_WRITES 1100

static EEPROM_t eeprom;
static uint16_t pages = 4; // of the store, half of them each half
static volatile uint32_t sweep_done; // writes of a sweep acknowledged before the cut

// Power-up over whatever the flash holds: a new descriptor, as after a reset.
// The last four pages, two for each half.
static void power_up(void)
{
  eeprom = (EEPROM_t){ .page_start = FLASH_PAGES - pages, .page_count = pages };
  TEST_Check(EEPROM_Init(&eeprom) == OK, "init");
}

//------------------------------------------------------------------------------------------- Store

static void newest_value_wins(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), ABSENT, "blank store");
  TEST_Check(EEPROM_Write(&eeprom, 7, 100) == OK);
  TEST_Check(EEPROM_Write(&eeprom, 9, 200) == OK);
  TEST_Check(EEPROM_Write(&eeprom, 7, 101) == OK);
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 101, "newest value of a key");
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 101, "newest value after a power cycle");
  TEST_Equal(EEPROM_Read(&eeprom, 9, ABSENT), 200, "other key");
  TEST_Equal(EEPROM_Read(&eeprom, 8, ABSENT), ABSENT, "key never written");
}

// Writes in a scrambled order run the halves through several rewrites:
// a key reads its newest value right after the write and after every power cycle.
static void rewrites_keep_every_key(void)
{
  FLASH_HostBlank();
  power_up();
  uint32_t kept[KEYS + 1];
  for(uint32_t key = 1; key <= KEYS; key++) kept[key] = ABSENT;
  uint32_t seed = 1;
  for(uint32_t i = 1; i <= 4000; i++) {
    seed = seed * 1103515245u + 12345u;
    uint32_t key = (seed >> 16) % KEYS + 1;
    TEST_Check(EEPROM_Write(&eeprom, key, i) == OK, "write:%u", i);
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == i, "write:%u key:%u", i, key);
    kept[key] = i;
    if(i % 500) continue;
    power_up();
    for(uint32_t k = 1; k <= KEYS; k++) {
      uint32_t value = EEPROM_Read(&eeprom, k, ABSENT);
      TEST_Check(value == kept[k], "write:%u key:%u reads %u, kept %u", i, k, value, kept[k]);
    }
  }
}

// The keys the layout reserves for itself are never stored
static void reserved_keys_refused(void)
{
  FLASH_HostBlank();
  power_up();
  static const uint32_t reserved[] = { 0xFFFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFDu, 0xFFFFFFFCu };
  for(uint16_t i = 0; i < array_len(reserved); i++) {
    TEST_Check(EEPROM_Write(&eeprom, reserved[i], 1) == ERR, "key:0x%08X", reserved[i]);
    TEST_Check(EEPROM_Read(&eeprom, reserved[i], ABSENT) == ABSENT, "key:0x%08X", reserved[i]);
  }
}

// A float comes back bit for bit, one never written as its default
static void floats_come_back_bit_for_bit(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Check(EEPROM_WriteF32(&eeprom, 5, -0.15625f) == OK);
  power_up();
  TEST_Equal(EEPROM_ReadF32(&eeprom, 5, 0.0f), -0.15625f, "float");
  TEST_Equal(EEPROM_ReadF32(&eeprom, 6, 2.5f), 2.5f, "float never written, the default");
}

// A clear forgets every key, also the ones a rewrite moved to the other half
static void clear_forgets_everything(void)
{
  FLASH_HostBlank();
  power_up();
  for(uint32_t i = 0; i < 600; i++) TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK);
  TEST_Check(EEPROM_Clear(&eeprom) == OK);
  TEST_Equal(EEPROM_Read(&eeprom, 1, ABSENT), ABSENT, "cleared");
  TEST_Check(EEPROM_Write(&eeprom, 2, 42) == OK);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 1, ABSENT), ABSENT, "cleared after a power cycle");
  TEST_Equal(EEPROM_Read(&eeprom, 2, ABSENT), 42, "write after the clear");
}

static void erase_page_a(void) { FLASH_EraseFull(FLASH_PAGES - pages); }

// A clear erases its pages whole, also one an erase cut late left reading blank
static void clear_erases_every_page_whole(void)
{
  FLASH_HostBlank();
  power_up();
  FLASH_HostCut(0, FLASH_Cut_Late, erase_page_a);
  power_up();
  TEST_Check(EEPROM_Clear(&eeprom) == OK);
  for(uint32_t i = 0; i < 600; i++) {
    TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK, "write:%u", i);
  }
  power_up();
  for(uint32_t key = 1; key <= 4; key++) {
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == 595 + key, "key:%u", key);
  }
}

static void clear_store(void) { EEPROM_Clear(&eeprom); }

// A clear cut late leaves A reading blank beside an empty B, as a fresh store:
// its keys are gone, and the first write erases A whole before it lands.
static void store_cleared_late_takes_writes(void)
{
  FLASH_HostBlank();
  power_up();
  for(uint32_t i = 0; i < 10; i++) TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK);
  FLASH_HostCut(0, FLASH_Cut_Late, clear_store);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 1, ABSENT), ABSENT, "cleared");
  for(uint32_t i = 0; i < 600; i++) {
    TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK, "write:%u", i);
  }
  power_up();
  for(uint32_t key = 1; key <= 4; key++) {
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == 595 + key, "key:%u", key);
  }
}

// A slot that refuses its write is skipped: the value lands one slot on
static void refused_slot_skipped(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Check(EEPROM_Write(&eeprom, 1, 10) == OK);
  TEST_Check(FLASH_Write(eeprom._cursor, 0, 0) == OK, "slot taken behind the store");
  TEST_Check(EEPROM_Write(&eeprom, 2, 20) == OK, "written one slot on");
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 1, ABSENT), 10, "key before the taken slot");
  TEST_Equal(EEPROM_Read(&eeprom, 2, ABSENT), 20, "key after it");
}

// A slot ECC corrects counts as written, as after a bit flipped by age, all but the newest
static void aged_slot_kept(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Check(EEPROM_Write(&eeprom, 7, 0x1234) == OK);
  uint32_t slot = eeprom._cursor - 8;
  TEST_Check(EEPROM_Write(&eeprom, 8, 0x5678) == OK);
  FLASH_HostAge(slot, 1);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 0x1234, "key written once, its slot aged");
  TEST_Equal(EEPROM_Read(&eeprom, 8, ABSENT), 0x5678, "newest key");
}

// A write cut halfway can read back corrected too, so the newest slot counts only when it reads
// clean: its key goes back to the older value, one written once is lost, the cost `eeprom.h` names
static void corrected_newest_slot_dropped(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Check(EEPROM_Write(&eeprom, 7, 1) == OK);
  TEST_Check(EEPROM_Write(&eeprom, 7, 2) == OK);
  FLASH_HostAge(eeprom._cursor - 8, 1);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 1, "older value");
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 1, "older value after the next power cycle");
  TEST_Check(EEPROM_Write(&eeprom, 9, 3) == OK);
  FLASH_HostAge(eeprom._cursor - 8, 1);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 9, ABSENT), ABSENT, "key written once");
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 1, "other key");
}

// The newest slot ECC cannot read leaves with the store too: a slot a cut tore can pass ECC
// on a later read, with a wrong value
static void unreadable_newest_slot_dropped(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Check(EEPROM_Write(&eeprom, 7, 1) == OK);
  TEST_Check(EEPROM_Write(&eeprom, 7, 2) == OK);
  uint32_t slot = eeprom._cursor - 8;
  FLASH_HostAge(slot, 2);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 7, ABSENT), 1, "older value");
  TEST_Check(FLASH_Check(slot, 8, NULL) == OK, "store moved, its old half cleared");
}

// A target left written, by a rewrite whose BEGIN tag failed, is cleared by the next rewrite
static void rewrite_clears_its_target(void)
{
  FLASH_HostBlank();
  power_up();
  TEST_Check(FLASH_Write(eeprom._addr_start[!eeprom._active], 1, 2) == OK, "target taken");
  for(uint32_t i = 0; i < 600; i++) {
    TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK, "write:%u", i);
  }
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 4, ABSENT), 599, "newest write, past the rewrite");
}

static void erase_target_page(void)
{
  FLASH_EraseFull(FLASH_PAGES - pages + (eeprom._active == EEPROM_Storage_A ? pages / 2 : 0));
}

// A target an erase cut late left reading blank, short of a full erase:
// the rewrite erases it whole before it writes there, and every key gets through.
static void rewrite_erases_its_target_whole(void)
{
  FLASH_HostBlank();
  power_up();
  FLASH_HostCut(0, FLASH_Cut_Late, erase_target_page);
  power_up();
  for(uint32_t i = 0; i < 600; i++) {
    TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK, "write:%u", i);
  }
  power_up();
  for(uint32_t key = 1; key <= 4; key++) {
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == 595 + key, "key:%u", key);
  }
}

// A store whose marker aged unreadable goes on: no source stands behind it to take its place
static void aged_marker_taken_over(void)
{
  FLASH_HostBlank();
  power_up();
  for(uint32_t i = 0; i < 600; i++) TEST_Check(EEPROM_Write(&eeprom, i % 4 + 1, i) == OK);
  FLASH_HostAge(eeprom._addr_end[eeprom._active] - 8, 2);
  power_up();
  for(uint32_t key = 1; key <= 4; key++) {
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == 595 + key, "key:%u", key);
  }
  TEST_Check(EEPROM_Write(&eeprom, 5, 55) == OK);
  power_up();
  TEST_Equal(EEPROM_Read(&eeprom, 5, ABSENT), 55, "write after the take-over");
}

//-------------------------------------------------------------------------------------- Power cuts

// Key and value of write `i` of a sweep: the keys in turn, every value unique, never all ones
static uint32_t sweep_key(uint32_t i) { return i % KEYS + 1; }
static uint32_t sweep_value(uint32_t i) { return sweep_key(i) << 16 | i; }

// Writes of a sweep, until the power goes or a write fails
static void sweep_writes(void)
{
  for(sweep_done = 0; sweep_done < SWEEP_WRITES; sweep_done++) {
    uint32_t i = sweep_done;
    if(EEPROM_Write(&eeprom, sweep_key(i), sweep_value(i))) return;
  }
}

// Value of `key` after the first `done` writes of a sweep
static uint32_t sweep_kept(uint32_t key, uint32_t done)
{
  while(done--) {
    if(sweep_key(done) == key) return sweep_value(done);
  }
  return ABSENT;
}

// Flash cycles of a whole sweep, the cut points.
// A cut run repeats this one up to its cut, so its writes are checked here, once.
static uint32_t sweep_cycles(void)
{
  FLASH_HostBlank();
  power_up();
  uint32_t before = FLASH_HostCycles();
  sweep_writes();
  TEST_Check(sweep_done == SWEEP_WRITES, "write:%u refused", sweep_done);
  return FLASH_HostCycles() - before;
}

// Sweep cut at flash cycle `cut`, where `where` says in it, then a power-up.
// Returns the writes acknowledged before the cut, the next one was in flight.
static uint32_t sweep_cut(uint32_t cut, FLASH_Cut_t where)
{
  FLASH_HostBlank();
  power_up();
  FLASH_HostCut((int32_t)cut, where, sweep_writes);
  power_up();
  return sweep_done;
}

static bool in_flight(uint32_t key, uint32_t done)
{
  return done < SWEEP_WRITES && key == sweep_key(done);
}

// Every key but the one in flight reads its last acknowledged value
static void others_kept(uint32_t cut, uint32_t done)
{
  for(uint32_t key = 1; key <= KEYS; key++) {
    if(in_flight(key, done)) continue;
    uint32_t value = EEPROM_Read(&eeprom, key, ABSENT);
    uint32_t kept = sweep_kept(key, done);
    TEST_Check(value == kept, "cut:%u write:%u key:%u reads 0x%X, kept 0x%X",
      cut, done, key, value, kept);
  }
}

// The key in flight reads its old value or the new one
static void old_or_new(uint32_t cut, uint32_t done)
{
  if(done == SWEEP_WRITES) return;
  uint32_t key = sweep_key(done);
  uint32_t value = EEPROM_Read(&eeprom, key, ABSENT);
  uint32_t kept = sweep_kept(key, done);
  TEST_Check(value == kept || value == sweep_value(done),
    "cut:%u write:%u key:%u reads 0x%X, old 0x%X, new 0x%X",
    cut, done, key, value, kept, sweep_value(done));
}

// The store goes on after a recovery: a new write stays through the next power cycle
static void store_goes_on(uint32_t cut)
{
  TEST_Check(EEPROM_Write(&eeprom, 1, 0x5AFE) == OK, "cut:%u", cut);
  power_up();
  TEST_Check(EEPROM_Read(&eeprom, 1, ABSENT) == 0x5AFE, "cut:%u", cut);
}

// Every cut point of a sweep, the cut landing where `where` says
static void sweep_survives(FLASH_Cut_t where)
{
  uint32_t total = sweep_cycles();
  for(uint32_t cut = 0; cut <= total; cut++) {
    uint32_t done = sweep_cut(cut, where);
    others_kept(cut, done);
    old_or_new(cut, done);
    store_goes_on(cut);
  }
}

static void write_survives_a_cut(void) { sweep_survives(FLASH_Cut_Before); }
static void write_survives_a_torn_cut(void) { sweep_survives(FLASH_Cut_Unreadable); }
static void write_survives_a_miscorrected_cut(void) { sweep_survives(FLASH_Cut_Corrected); }
static void write_survives_an_unstable_cut(void) { sweep_survives(FLASH_Cut_Unstable); }
static void write_survives_a_late_cut(void) { sweep_survives(FLASH_Cut_Late); }

// Power-up over the flash `FLASH_HostSave` kept, cut at every cycle of its recovery,
// anywhere in the cycle: the power-up after each keeps the keys
static void recovery_cut_everywhere(uint32_t cut, uint32_t done)
{
  uint32_t before = FLASH_HostCycles();
  power_up();
  uint32_t recovery = FLASH_HostCycles() - before;
  for(uint32_t again = 0; again < recovery; again++) {
    for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Corrected; where++) {
      FLASH_HostRestore();
      FLASH_HostCut((int32_t)again, where, power_up);
      power_up();
      others_kept(cut, done);
      old_or_new(cut, done);
    }
  }
}

// A cut write, and the power-up recovering it cut too
static void recovery_survives_a_cut(void)
{
  uint32_t total = sweep_cycles();
  for(uint32_t cut = 0; cut <= total; cut++) {
    for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Corrected; where++) {
      FLASH_HostBlank();
      power_up();
      FLASH_HostCut((int32_t)cut, where, sweep_writes);
      FLASH_HostSave();
      recovery_cut_everywhere(cut, sweep_done);
    }
  }
}

// Half `log` holding keys 1 to `KEYS`, written by hand, the other what a rewrite of it, cut,
// and a cut clear of its target leave: copies of every other key, the BEGIN tag erased
static void log_beside_a_cleared_target(EEPROM_Storage_t log)
{
  FLASH_HostBlank();
  uint32_t start = FLASH_GetAddress(FLASH_PAGES - pages, 0);
  uint32_t half = pages / 2u * FLASH_PAGE_SIZE;
  uint32_t log_at = log == EEPROM_Storage_A ? start : start + half;
  uint32_t copy_at = log == EEPROM_Storage_A ? start + half : start;
  for(uint32_t key = 1; key <= KEYS; key++) {
    TEST_Check(FLASH_Write(log_at + 8 * (key - 1), key, 100 + key) == OK);
    if(key % 2) TEST_Check(FLASH_Write(copy_at + 8 * key, key, 100 + key) == OK);
  }
}

// A cut clear of a rewrite target can take its BEGIN tag and leave copies, so both halves hold
// data and no tag: the store is the half that starts with a key, on either side
static void cleared_target_spares_the_log(void)
{
  log_beside_a_cleared_target(EEPROM_Storage_A);
  power_up();
  for(uint32_t key = 1; key <= KEYS; key++) {
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == 100 + key, "log in A, key:%u", key);
  }
  log_beside_a_cleared_target(EEPROM_Storage_B);
  power_up();
  for(uint32_t key = 1; key <= KEYS; key++) {
    TEST_Check(EEPROM_Read(&eeprom, key, ABSENT) == 100 + key, "log in B, key:%u", key);
  }
}

// Halves of one page each, as ectra and Ediphor run the store:
// the BEGIN tag and the marker of a rewrite target share a page,
// and a cut clear of it can take one without the other.
static void one_page_halves_survive_cuts(void)
{
  pages = 2;
  aged_marker_taken_over();
  rewrite_erases_its_target_whole();
  write_survives_a_cut();
  write_survives_a_torn_cut();
  write_survives_a_miscorrected_cut();
  write_survives_an_unstable_cut();
  write_survives_a_late_cut();
  recovery_survives_a_cut();
  cleared_target_spares_the_log();
  pages = 4;
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(newest_value_wins);
  TEST(rewrites_keep_every_key);
  TEST(reserved_keys_refused);
  TEST(floats_come_back_bit_for_bit);
  TEST(clear_forgets_everything);
  TEST(clear_erases_every_page_whole);
  TEST(store_cleared_late_takes_writes);
  TEST(refused_slot_skipped);
  TEST(aged_slot_kept);
  TEST(corrected_newest_slot_dropped);
  TEST(unreadable_newest_slot_dropped);
  TEST(rewrite_clears_its_target);
  TEST(rewrite_erases_its_target_whole);
  TEST(aged_marker_taken_over);
  TEST(write_survives_a_cut);
  TEST(write_survives_a_torn_cut);
  TEST(write_survives_a_miscorrected_cut);
  TEST(write_survives_an_unstable_cut);
  TEST(write_survives_a_late_cut);
  TEST(recovery_survives_a_cut);
  TEST(cleared_target_spares_the_log);
  TEST(one_page_halves_survive_cuts); // last: a miss there leaves `pages` at two
}

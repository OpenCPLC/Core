// .tests/test_flash.c

// Records of `FLASH_Save` over the host flash model, the code the chip runs:
// a record loads back whole, over one page or several, and a shorter one replaces a longer.
// After a power cut at any flash cycle of a save, before it or halfway through it,
// the page holds the old record, no record or the new one, never a mix of them.
// `FLASH_Copy` takes what ECC passes, and a doubleword ECC cannot read fails it.
// An erase cut late leaves a page reading blank that holds writes only after `FLASH_EraseFull`,
// and zeros go over a doubleword a cut tore, which then reads them clean.

// A G081: 128kB of flash in pages of 2kB
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64

#include <string.h>

#include "test.h"
#include "flash.c"

#define PAGE 10 // first page of the records

static uint8_t old_record[5000];
static uint8_t new_record[5000];
static uint8_t loaded[5000];

// Bytes no two records share, so a mix of them shows
static void pattern(uint8_t *data, uint16_t size, uint8_t seed)
{
  for(uint16_t i = 0; i < size; i++) data[i] = (uint8_t)(i * 31u + seed);
}

//----------------------------------------------------------------------------------------- Records

// One byte, a full doubleword and a byte past it, exactly one page and a byte past it, three pages
static void record_loads_back_whole(void)
{
  static const uint16_t sizes[] = { 1, 8, 9, FLASH_PAGE_SIZE - 8, FLASH_PAGE_SIZE - 7, 5000 };
  for(uint16_t n = 0; n < array_len(sizes); n++) {
    uint16_t size = sizes[n];
    FLASH_HostBlank();
    pattern(new_record, size, (uint8_t)n);
    TEST_Check(FLASH_Save(PAGE, new_record, size) == OK, "size:%u", size);
    memset(loaded, 0, sizeof(loaded));
    TEST_Check(FLASH_Load(PAGE, loaded) == size, "size:%u", size);
    TEST_Check(!memcmp(loaded, new_record, size), "size:%u", size);
    TEST_Check(FLASH_Compare(PAGE, new_record, size), "size:%u", size);
    TEST_Check(!FLASH_Compare(PAGE, new_record, size - 1), "size:%u, one byte short", size);
    new_record[size - 1] ^= 0x01;
    TEST_Check(!FLASH_Compare(PAGE, new_record, size), "size:%u, last byte changed", size);
  }
}

static void shorter_record_replaces_longer(void)
{
  FLASH_HostBlank();
  pattern(old_record, 5000, 1);
  pattern(new_record, 10, 2);
  TEST_Check(FLASH_Save(PAGE, old_record, 5000) == OK);
  TEST_Check(FLASH_Save(PAGE, new_record, 10) == OK);
  TEST_Equal(FLASH_Load(PAGE, loaded), 10, "size of the newer record");
  TEST_Bytes(loaded, new_record, 10, "newer record");
}

// An empty record or one past the end of the flash is refused, one that just fits is taken,
// and the stored one stays
static void refuses_what_does_not_fit(void)
{
  FLASH_HostBlank();
  pattern(old_record, 100, 3);
  TEST_Check(FLASH_Save(PAGE, old_record, 100) == OK);
  TEST_Check(FLASH_Save(PAGE, new_record, 0) == ERR, "empty");
  TEST_Check(FLASH_Save(FLASH_PAGES - 1, new_record, FLASH_PAGE_SIZE) == ERR, "past the end");
  TEST_Check(FLASH_Save(FLASH_PAGES - 1, new_record, FLASH_PAGE_SIZE - 8) == OK, "just fits");
  TEST_Check(FLASH_Save(FLASH_PAGES, new_record, 1) == ERR, "page past the end");
  TEST_Equal(FLASH_Load(PAGE, loaded), 100, "stored record");
  TEST_Bytes(loaded, old_record, 100, "stored record");
}

//--------------------------------------------------------------------------------------- ECC reads

// A copy takes what ECC passes, byte for byte, also from inside a doubleword:
// one aged bit copies corrected, as written, two fail the copy and spare the doubleword beside
static void copy_reads_through_ecc(void)
{
  FLASH_HostBlank();
  uint32_t addr = FLASH_GetAddress(PAGE, 0);
  pattern(new_record, 16, 4);
  uint32_t word[4];
  memcpy(word, new_record, sizeof(word));
  TEST_Check(FLASH_Write(addr, word[0], word[1]) == OK);
  TEST_Check(FLASH_Write(addr + 8, word[2], word[3]) == OK);
  bool corrected;
  TEST_Check(FLASH_Copy(addr + 3, loaded, 10, &corrected) == OK && !corrected, "clean");
  TEST_Bytes(loaded, new_record + 3, 10, "bytes 3 to 12");
  FLASH_HostAge(addr + 8, 1);
  TEST_Check(FLASH_Copy(addr, loaded, 16, &corrected) == OK && corrected, "one aged bit");
  TEST_Bytes(loaded, new_record, 16, "corrected as written");
  FLASH_HostAge(addr, 2);
  TEST_Check(FLASH_Copy(addr, loaded, 16, NULL) == ERR, "two aged bits");
  TEST_Check(FLASH_Copy(addr + 8, loaded, 8, NULL) == OK, "the doubleword beside them");
}

//-------------------------------------------------------------------------------------- Power cuts

#define OLD_SIZE 3000 // two pages, overwritten by a new record of two pages
#define NEW_SIZE 3500

// Board holding the old record, the power on for good
static void board_with_old_record(void)
{
  FLASH_HostBlank();
  pattern(old_record, OLD_SIZE, 1);
  pattern(new_record, NEW_SIZE, 2);
  TEST_Check(FLASH_Save(PAGE, old_record, OLD_SIZE) == OK, "old record");
}

// Old record, no record or the new one, and the compare agrees with the load
static void old_none_or_new(uint32_t cut)
{
  uint16_t size = FLASH_Load(PAGE, loaded);
  bool old_one = size == OLD_SIZE && !memcmp(loaded, old_record, OLD_SIZE);
  bool new_one = size == NEW_SIZE && !memcmp(loaded, new_record, NEW_SIZE);
  TEST_Check(!size || old_one || new_one, "cut:%u loads %u bytes, %02X %02X %02X %02X",
    cut, size, loaded[0], loaded[1], loaded[2], loaded[3]);
  if(size) TEST_Check(FLASH_Compare(PAGE, loaded, size), "cut:%u", cut);
}

static void save_new(void) { FLASH_Save(PAGE, new_record, NEW_SIZE); }

// Every cut point of a save, the cut landing where `where` says
static void sweep(FLASH_Cut_t where)
{
  board_with_old_record();
  uint32_t before = FLASH_HostCycles();
  TEST_Check(FLASH_Save(PAGE, new_record, NEW_SIZE) == OK, "new record");
  uint32_t total = FLASH_HostCycles() - before;
  for(uint32_t cut = 0; cut <= total; cut++) {
    board_with_old_record();
    FLASH_HostCut((int32_t)cut, where, save_new);
    old_none_or_new(cut);
  }
}

static void save_survives_a_cut(void) { sweep(FLASH_Cut_Before); }
static void save_survives_a_torn_cut(void) { sweep(FLASH_Cut_Unreadable); }
static void save_survives_a_miscorrected_cut(void) { sweep(FLASH_Cut_Corrected); }

// A record opening on ones, its save cut anywhere on a blank page: the next save lands.
// Ones program nothing, so the page never reads blank while holding a doubleword written.
static void save_after_a_cut_lands(void)
{
  FLASH_HostBlank();
  pattern(new_record, NEW_SIZE, 2);
  memset(new_record, 0xFF, 8);
  uint32_t before = FLASH_HostCycles();
  TEST_Check(FLASH_Save(PAGE, new_record, NEW_SIZE) == OK, "first save");
  uint32_t total = FLASH_HostCycles() - before;
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Corrected; where++) {
    for(uint32_t cut = 0; cut <= total; cut++) {
      FLASH_HostBlank();
      FLASH_HostCut((int32_t)cut, where, save_new);
      TEST_Check(FLASH_Save(PAGE, new_record, NEW_SIZE) == OK, "where:%d cut:%u", where, cut);
      TEST_Check(FLASH_Load(PAGE, loaded) == NEW_SIZE, "where:%d cut:%u", where, cut);
    }
  }
}

static void erase_page(void) { FLASH_Erase(PAGE); }

// An erase cut halfway can leave a page that reads blank only through ECC:
// the next erase takes it, so its doublewords take writes again
static void erase_after_a_cut_lands(void)
{
  FLASH_HostBlank();
  uint32_t addr = FLASH_GetAddress(PAGE, 0);
  TEST_Check(FLASH_Write(addr, 0xFFFFFFFFu, 0x5A5A5A5Au) == OK);
  FLASH_HostCut(0, FLASH_Cut_Corrected, erase_page);
  bool corrected;
  uint32_t word[2];
  TEST_Check(FLASH_Copy(addr, word, 8, &corrected) == OK && corrected, "ones through ECC");
  TEST_Check(word[0] == 0xFFFFFFFFu && word[1] == 0xFFFFFFFFu, "ones through ECC");
  TEST_Check(FLASH_Erase(PAGE) == OK);
  TEST_Check(FLASH_Write(addr, 1, 2) == OK, "write after the erase");
}

// An erase cut late leaves a page reading blank, ECC clean, that holds no program:
// `FLASH_Erase` spares it, `FLASH_EraseFull` erases it whole and writes land again.
static void late_erase_cut_takes_a_full_erase(void)
{
  FLASH_HostBlank();
  uint32_t addr = FLASH_GetAddress(PAGE, 0);
  TEST_Check(FLASH_Write(addr, 1, 2) == OK);
  FLASH_HostCut(0, FLASH_Cut_Late, erase_page);
  bool corrected;
  uint32_t word[2];
  TEST_Check(FLASH_Copy(addr, word, 8, &corrected) == OK && !corrected, "reads clean");
  TEST_Check(word[0] == 0xFFFFFFFFu && word[1] == 0xFFFFFFFFu, "reads erased");
  uint32_t cycles = FLASH_HostCycles();
  TEST_Check(FLASH_Erase(PAGE) == OK);
  TEST_Equal(FLASH_HostCycles(), cycles, "spared, it reads blank");
  FLASH_HostSave();
  TEST_Check(FLASH_Write(addr, 3, 4) == ERR, "no program holds");
  TEST_Check(FLASH_Copy(addr, word, 8, NULL) == ERR, "the write it took fails ECC");
  FLASH_HostRestore();
  TEST_Check(FLASH_EraseFull(PAGE) == OK);
  TEST_Check(FLASH_Write(addr, 3, 4) == OK, "write after the full erase");
  TEST_Check(FLASH_Copy(addr, word, 8, NULL) == OK && word[0] == 3 && word[1] == 4);
}

static void write_doubleword(void) { FLASH_Write(FLASH_GetAddress(PAGE, 0), 5, 6); }

// An unstable cut leaves the doubleword of a program erased and unreadable by turns, erased first:
// a write over it is refused, zeros go over it and it reads them clean.
static void unstable_cut_reads_by_turns(void)
{
  FLASH_HostBlank();
  uint32_t addr = FLASH_GetAddress(PAGE, 0);
  FLASH_HostCut(0, FLASH_Cut_Unstable, write_doubleword);
  uint32_t word[2];
  for(uint8_t turn = 0; turn < 4; turn++) {
    bool erased = FLASH_Copy(addr, word, 8, NULL) == OK;
    TEST_Check(erased == !(turn % 2), "turn:%u", turn);
    if(erased) TEST_Check(word[0] == 0xFFFFFFFFu && word[1] == 0xFFFFFFFFu, "turn:%u", turn);
  }
  TEST_Check(FLASH_Write(addr, 5, 6) == ERR, "a write over it");
  TEST_Check(FLASH_Write(addr, 0, 0) == OK, "zeros over it");
  bool corrected;
  for(uint8_t turn = 0; turn < 2; turn++) {
    TEST_Check(FLASH_Copy(addr, word, 8, &corrected) == OK && !corrected, "turn:%u", turn);
    TEST_Check(!word[0] && !word[1], "turn:%u reads zeros", turn);
  }
}

// Zeros over a doubleword ECC cannot read leave it reading them clean
static void zeros_go_over_an_unreadable_doubleword(void)
{
  FLASH_HostBlank();
  uint32_t addr = FLASH_GetAddress(PAGE, 0);
  TEST_Check(FLASH_Write(addr, 7, 8) == OK);
  FLASH_HostAge(addr, 2);
  uint32_t word[2];
  TEST_Check(FLASH_Copy(addr, word, 8, NULL) == ERR, "two aged bits");
  TEST_Check(FLASH_Write(addr, 0, 0) == OK, "zeros over it");
  bool corrected;
  TEST_Check(FLASH_Copy(addr, word, 8, &corrected) == OK && !corrected, "reads clean");
  TEST_Check(!word[0] && !word[1], "reads zeros");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(record_loads_back_whole);
  TEST(shorter_record_replaces_longer);
  TEST(refuses_what_does_not_fit);
  TEST(copy_reads_through_ecc);
  TEST(save_survives_a_cut);
  TEST(save_survives_a_torn_cut);
  TEST(save_survives_a_miscorrected_cut);
  TEST(save_after_a_cut_lands);
  TEST(erase_after_a_cut_lands);
  TEST(late_erase_cut_takes_a_full_erase);
  TEST(unstable_cut_reads_by_turns);
  TEST(zeros_go_over_an_unreadable_doubleword);
}

// .tests/test_jrn.c
// Journal `JRN_t` over PDB on the host flash: a record of 16B, byte for byte as it goes out,
// little-endian number, time, code, data and CRC-32 last.
// Its time comes from RTC, `0` while the clock is not set.
// Records come back newest or oldest first, by code, counted, and squeezed to their timestamps.

// A G081: 128kB of flash in pages of 2kB
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64

#include <string.h>

#include "test.h"
#include "rtc.h"
#include "flash.c"
#include "pdb.c"
#include "jrn.c"

#define PAGE 60 // first page of the journal

static PDB_t journal;
static uint8_t mbb_data[16 * sizeof(JRN_t)];
static MBB_t mbb = { .name = "jrn", .buffer = mbb_data, .limit = sizeof(mbb_data) };

// Power-up over whatever the flash holds: a new descriptor, as after a reset
static void power_up(void)
{
  journal = (PDB_t){ .page_start = PAGE, .page_count = 2, .kind = 0x4A52 };
  TEST_Check(JRN_Init(&journal) == OK, "init");
}

// Record 1, code `10`, data `0x1234`, no clock, CRC-32 worked out apart from this code
static void record_lays_out_as_it_goes_out(void)
{
  static const uint8_t RECORD[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x34, 0x12, 0xDB, 0xE8, 0x5E, 0xC7,
  };
  RtcInit = false;
  FLASH_HostBlank();
  power_up();
  TEST_Equal(sizeof(JRN_t), 16, "record size");
  TEST_Check(JRN_Insert(10, 0x1234) == OK);
  TEST_Bytes(FLASH_Ref(FLASH_GetAddress(PAGE, 16)), RECORD, sizeof(RECORD), "first record");
  TEST_Check(JRN_Init(NULL) == ERR, "no database");
}

// Time of a set clock; `0` without a clock and while it shows the date an RTC starts at
static void time_from_a_set_clock(void)
{
  RtcInit = false;
  FLASH_HostBlank();
  power_up();
  TEST_Check(JRN_Insert(1, 0) == OK, "no clock");
  RTC_Init();
  RTC_SetTimestamp(946684800); // 2000-01-01
  TEST_Check(JRN_Insert(2, 0) == OK, "clock never set");
  RTC_SetTimestamp(1800000000);
  TEST_Check(JRN_Insert(3, 0) == OK, "set clock");
  power_up();
  PDB_Query_t query = { .dir = PDB_Asc };
  TEST_Equal(JRN_Select(&query, &mbb), 3, "records");
  JRN_t *record = (JRN_t *)mbb.buffer;
  for(uint8_t i = 0; i < 3; i++) {
    TEST_Check(record[i].number == i + 1u && record[i].code == i + 1u, "record %u", i);
  }
  TEST_Equal(record[0].time, 0, "no clock");
  TEST_Equal(record[1].time, 0, "clock never set");
  TEST_Check(record[2].time >= 1800000000u && record[2].time < 1800000010u, "set clock");
}

// Ten records over three codes: newest first by code, counted, squeezed to their timestamps,
// and gone after a delete.
static void records_by_code_and_timestamps(void)
{
  FLASH_HostBlank();
  power_up();
  RTC_Init();
  RTC_SetTimestamp(1800000000);
  for(uint16_t i = 0; i < 10; i++) TEST_Check(JRN_Insert(i % 3, i) == OK, "record %u", i);
  PDB_Query_t query = { .dir = PDB_Desc };
  TEST_Equal(JRN_Count(&query), 10, "records");
  TEST_Equal(JRN_SelectCode(&query, 1, &mbb), 3, "code 1");
  TEST_Equal(mbb.size, 3 * sizeof(JRN_t), "buffer holds three");
  JRN_t *record = (JRN_t *)mbb.buffer;
  TEST_Check(record[0].data == 7 && record[1].data == 4 && record[2].data == 1, "newest first");
  JRN_GetTimestamp(&mbb);
  TEST_Equal(mbb.size, 3 * sizeof(uint32_t), "three timestamps");
  uint32_t *time = (uint32_t *)mbb.buffer;
  for(uint8_t i = 0; i < 3; i++) {
    TEST_Check(time[i] >= 1800000000u && time[i] < 1800000010u, "timestamp %u", i);
  }
  TEST_Check(JRN_Delete() == OK);
  TEST_Equal(JRN_Count(&query), 0, "deleted");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(record_lays_out_as_it_goes_out);
  TEST(time_from_a_set_clock);
  TEST(records_by_code_and_timestamps);
}

// .tests/test_mbb.c
// Byte buffer `MBB_t`: a writer appends all of its text or none of it and returns how much,
// floats print as fixed point rounded half away from zero, records shift and drop in balance,
// and the flash mirror loads back what it saved.

#include <stdint.h>
#include <string.h>

#include "test.h"
#include "flash.c"
#include "mbb.c"

static uint8_t memory[64];

// Empty buffer of `limit` bytes over `memory`
static MBB_t buffer(uint16_t limit)
{
  return (MBB_t){ .name = "test", .buffer = memory, .limit = limit, ._base = memory };
}

// Content of a buffer as text
static const char *text(const MBB_t *mbb)
{
  static char copy[sizeof(memory) + 1];
  memcpy(copy, mbb->buffer, mbb->size);
  copy[mbb->size] = 0;
  return copy;
}

//----------------------------------------------------------------------------------------- Writers

static const RTC_Datetime_t moment = {
  .year = 26, .month = 10, .month_day = 9, .week_day = 5,
  .hour = 8, .minute = 5, .second = 7, .ms = 42
};

static const RTC_AlarmCfg_t alarm = {
  .week = true, .day = 3, .hour_mask = true, .minute = 5, .second = 7
};

static const RTC_AlarmCfg_t monthly = { .day = 15, .hour = 1, .minute = 5, .second = 7 };

static int32_t char16(MBB_t *mbb) { return MBB_Char16(mbb, 0x4142); }
static int32_t char32(MBB_t *mbb) { return MBB_Char32(mbb, 0x41424344); }
static int32_t char64(MBB_t *mbb) { return MBB_Char64(mbb, 0x3031323334353637ull); }
static int32_t string(MBB_t *mbb) { return MBB_String(mbb, "abc"); }
static int32_t enter(MBB_t *mbb) { return MBB_Enter(mbb); }
static int32_t boolean(MBB_t *mbb) { return MBB_Bool(mbb, false); }
static int32_t integer(MBB_t *mbb) { return MBB_Int(mbb, -42, 10, true, 0, 6); }
static int32_t dec(MBB_t *mbb) { return MBB_Dec(mbb, INT64_MIN); }
static int32_t udec(MBB_t *mbb) { return MBB_uDec(mbb, UINT64_MAX); }
static int32_t hex32(MBB_t *mbb) { return MBB_Hex32(mbb, 0xBEEF); }
static int32_t bin8(MBB_t *mbb) { return MBB_Bin8(mbb, 5); }
static int32_t number(MBB_t *mbb) { return MBB_Float(mbb, -3.14159f, 2, 0, 7); }
static int32_t not_number(MBB_t *mbb) { return MBB_Float(mbb, NaN, 2, 0, 5); }
static int32_t date(MBB_t *mbb) { return MBB_Date(mbb, &moment); }
static int32_t time_of_day(MBB_t *mbb) { return MBB_Time(mbb, &moment); }
static int32_t time_ms(MBB_t *mbb) { return MBB_TimeMs(mbb, &moment); }
static int32_t datetime(MBB_t *mbb) { return MBB_Datetime(mbb, &moment); }
static int32_t datetime_ms(MBB_t *mbb) { return MBB_DatetimeMs(mbb, &moment); }
static int32_t alarm_time(MBB_t *mbb) { return MBB_AlarmTime(mbb, &alarm); }
static int32_t alarm_day(MBB_t *mbb) { return MBB_Alarm(mbb, &alarm); }
static int32_t alarm_month(MBB_t *mbb) { return MBB_Alarm(mbb, &monthly); }

// Every writer of the buffer and the text it appends; multi-byte values land big-endian
static const struct {
  int32_t (*Write)(MBB_t *mbb);
  const char *text;
} writers[] = {
  { char16, "AB" },
  { char32, "ABCD" },
  { char64, "01234567" },
  { string, "abc" },
  { enter, "\r\n" },
  { boolean, "false" },
  { integer, "   -42" },
  { dec, "-9223372036854775808" },
  { udec, "18446744073709551615" },
  { hex32, "0000BEEF" },
  { bin8, "00000101" },
  { number, "  -3.14" },
  { not_number, "    -" },
  { date, "2026-10-09" },
  { time_of_day, "08:05:07" },
  { time_ms, "08:05:07.042" },
  { datetime, "2026-10-09 08:05:07" },
  { datetime_ms, "2026-10-09 08:05:07.042" },
  { alarm_time, "**:05:07" },
  { alarm_day, "Wednesday **:05:07" },
  { alarm_month, "15 01:05:07" },
};

static void writers_return_what_they_append(void)
{
  for(uint8_t n = 0; n < array_len(writers); n++) {
    MBB_t mbb = buffer(sizeof(memory));
    int32_t len = writers[n].Write(&mbb);
    TEST_Check(!strcmp(text(&mbb), writers[n].text), "\"%s\" written as \"%s\"",
      writers[n].text, text(&mbb));
    TEST_Check(len == mbb.size, "\"%s\" returns %d", writers[n].text, len);
  }
}

// A buffer with exactly the room of the text takes all of it
static void writer_fits_a_buffer_of_its_length(void)
{
  for(uint8_t n = 0; n < array_len(writers); n++) {
    uint16_t len = strlen(writers[n].text);
    MBB_t mbb = buffer(len);
    writers[n].Write(&mbb);
    TEST_Check(!strcmp(text(&mbb), writers[n].text), "\"%s\" in %u bytes written as \"%s\"",
      writers[n].text, len, text(&mbb));
  }
}

// One byte short of the text, a writer appends nothing and returns `0`
static void writer_short_of_room_appends_nothing(void)
{
  for(uint8_t n = 0; n < array_len(writers); n++) {
    uint16_t len = strlen(writers[n].text) - 1;
    MBB_t mbb = buffer(len);
    int32_t ret = writers[n].Write(&mbb);
    TEST_Check(!ret && !mbb.size, "\"%s\" in %u bytes appends \"%s\", returns %d",
      writers[n].text, len, text(&mbb), ret);
  }
}

//------------------------------------------------------------------------------------------ Floats

// Text of one float, places and width as in `MBB_Float`
static const char *float_text(float value, uint8_t places)
{
  static MBB_t mbb;
  mbb = buffer(sizeof(memory));
  MBB_Float(&mbb, value, places, 0, 0);
  return text(&mbb);
}

static void floats_round_half_away_from_zero(void)
{
  TEST_Equal(float_text(0.01f, 2), "0.01", "scaled just below the step");
  TEST_Equal(float_text(2.5f, 0), "3", "half up");
  TEST_Equal(float_text(-2.5f, 0), "-3", "half down");
  TEST_Equal(float_text(0.125f, 2), "0.13", "exact half of a place");
  TEST_Equal(float_text(-0.125f, 2), "-0.13", "exact half of a place, negative");
  TEST_Equal(float_text(1234567.0f, 1), "1234567.0", "seven digits and one place");
}

// A negative value that rounds to zero prints zero, with its sign or without
static void negative_rounding_to_zero_prints_zero(void)
{
  const char *two_places = float_text(-0.004f, 2);
  TEST_Check(!strcmp(two_places, "0.00") || !strcmp(two_places, "-0.00"),
    "-0.004 to two places printed as \"%s\"", two_places);
  const char *no_places = float_text(-0.4f, 0);
  TEST_Check(!strcmp(no_places, "0") || !strcmp(no_places, "-0"),
    "-0.4 to no places printed as \"%s\"", no_places);
}

// Every digit of a large float prints, however many places follow
static void large_float_prints_its_digits(void)
{
  TEST_Equal(float_text(3000000.0f, 3), "3000000.000", "three million to three places");
  TEST_Equal(float_text(5e9f, 0), "5000000000", "five billion");
}

//----------------------------------------------------------------------------------------- Records

// Records added at the end, the oldest shifted out, the newest dropped,
// and the byte counts the calls return sum to the size held.
static void records_shift_and_drop_in_balance(void)
{
  MBB_t mbb = buffer(64);
  mbb.struct_size = 6;
  int32_t sum = 0;
  for(uint8_t i = 0; i < 11; i++) {
    uint8_t record[6] = { i, i, i, i, i, i };
    sum += MBB_StructAdd(&mbb, record);
  }
  TEST_Equal(MBB_StructCount(&mbb), 10, "ten records of six bytes in 64");
  TEST_Check(MBB_StructPeek(&mbb, 9)[5] == 9 && !MBB_StructPeek(&mbb, 10), "last record");
  sum += MBB_StructShift(&mbb, 3);
  TEST_Check(MBB_StructCount(&mbb) == 7 && MBB_StructPeek(&mbb, 0)[0] == 3, "oldest three gone");
  sum += MBB_StructDrop(&mbb, 2);
  uint8_t last[6];
  TEST_Check(MBB_StructGet(&mbb, 4, last) == 6 && last[0] == 7, "newest two gone");
  TEST_Equal(sum, mbb.size, "returns sum to the size held");
  TEST_Equal(MBB_StructFree(&mbb, 10), 4, "nine fit beside a margin of ten, five held");
  sum += MBB_StructShift(&mbb, 100);
  TEST_Check(!mbb.size && !sum, "every record shifted out");
}

//------------------------------------------------------------------------------------------- Flash

// The mirror loads back what it saved, spares the flash a save of the bytes it holds,
// and refuses a record larger than the buffer it loads into.
static void flash_mirror_loads_back_what_it_saved(void)
{
  FLASH_HostBlank();
  MBB_t saved = buffer(sizeof(memory));
  saved.flash_page = 20;
  MBB_String(&saved, "settings");
  TEST_Check(MBB_FlashSave(&saved) == OK);
  uint32_t spent = FLASH_HostCycles();
  TEST_Check(MBB_FlashSave(&saved) == OK);
  TEST_Equal(FLASH_HostCycles(), spent, "same bytes saved again, no flash cycle");
  static uint8_t other[16];
  MBB_t loaded = { .buffer = other, .limit = sizeof(other), .flash_page = 20, ._base = other };
  TEST_Check(MBB_FlashLoad(&loaded) == OK);
  TEST_Check(loaded.size == 8 && !memcmp(other, "settings", 8), "loaded");
  MBB_t small = { .buffer = other, .limit = 4, .flash_page = 20, ._base = other };
  TEST_Check(MBB_FlashLoad(&small) == ERR && !small.size, "record larger than the buffer");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(writers_return_what_they_append);
  TEST(writer_fits_a_buffer_of_its_length);
  TEST(writer_short_of_room_appends_nothing);
  TEST(floats_round_half_away_from_zero);
  TEST(negative_rounding_to_zero_prints_zero);
  TEST(large_float_prints_its_digits);
  TEST(records_shift_and_drop_in_balance);
  TEST(flash_mirror_loads_back_what_it_saved);
}

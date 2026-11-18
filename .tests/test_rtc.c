// .tests/test_rtc.c

// RTC model on the host as the chip runs: a cold domain shows 2000-01-01 with `RtcReady` down,
// a set before `RTC_Init` is dropped, a repeated init keeps the calendar,
// a set raises `RtcReady` and a reset drops it.
// Calendar arithmetic comes back exact, an interval alarm wraps at midnight,
// a date far from a set clock is refused, and an alarm fires once, on the second it matches.

#include "test.h"
#include "rtc.h"

// 2026-11-18 13:05:07, a Wednesday
#define WEDNESDAY_ts 1795007107u
// 2027-01-15 23:59:50
#define MIDNIGHT_ts 1800057590u

// The calendar shows 2000-01-01 00:00
static bool at_2000(void)
{
  RTC_Datetime_t now = RTC_Datetime();
  return now.year == 0 && now.month == 1 && now.month_day == 1 && now.hour == 0 && !now.minute;
}

//------------------------------------------------------------------------------------------ Domain

static void cold_domain_shows_2000(void)
{
  TEST_Check(RTC_SetTimestamp(WEDNESDAY_ts) == ERR, "set before init dropped");
  TEST_Check(RTC_Init() == OK, "init");
  TEST_Check(at_2000(), "2000-01-01");
  TEST_Check(RtcInit && !RtcReady, "running, never set");
}

static void set_raises_ready_and_init_keeps_the_calendar(void)
{
  TEST_Check(RTC_SetTimestamp(WEDNESDAY_ts) == OK, "set");
  TEST_Check(RtcReady, "ready");
  TEST_Check(RTC_Init() == OK, "init again");
  uint64_t now = RTC_Timestamp();
  TEST_Check(now >= WEDNESDAY_ts && now < WEDNESDAY_ts + 5, "calendar kept");
  TEST_Equal(RTC_Datetime().week_day, 3, "Wednesday");
}

static void reset_drops_ready(void)
{
  TEST_Check(RTC_Reset() == OK, "reset");
  TEST_Check(!RtcReady, "ready dropped");
  TEST_Check(at_2000(), "2000-01-01");
}

//---------------------------------------------------------------------------------------- Calendar

static void calendar_comes_back_exact(void)
{
  RTC_Datetime_t date = {
    .year = 26, .month = 11, .month_day = 18,
    .hour = 13, .minute = 5, .second = 7
  };
  TEST_Check(RTC_DatetimeToUnix(&date) == WEDNESDAY_ts, "timestamp");
  RTC_Datetime_t back = RTC_UnixToDatetime(WEDNESDAY_ts);
  TEST_Check(back.year == 26 && back.month == 11 && back.month_day == 18, "date");
  TEST_Check(back.hour == 13 && back.minute == 5 && back.second == 7, "time");
  TEST_Equal(back.week_day, 3, "Wednesday");
  TEST_Check(RTC_SetDatetime(&date) == OK, "set");
  TEST_Equal(date.week_day, 3, "weekday computed into the date");
}

static void date_far_from_a_set_clock_refused(void)
{
  TEST_Check(RTC_SetTimestamp(WEDNESDAY_ts) == OK, "set");
  RTC_Datetime_t near = RTC_UnixToDatetime(WEDNESDAY_ts + 600);
  RTC_Datetime_t far = RTC_UnixToDatetime(WEDNESDAY_ts + 7200);
  TEST_Check(RTC_DatetimeIsCorrect(&near, 0), "ten minutes off");
  TEST_Check(!RTC_DatetimeIsCorrect(&far, 0), "two hours off");
  TEST_Check(RTC_DatetimeIsCorrect(&far, 8), "two hours off, given at UTC+2");
}

//------------------------------------------------------------------------------------------- Alarm

static void interval_alarm_wraps_at_midnight(void)
{
  TEST_Check(RTC_SetTimestamp(MIDNIGHT_ts) == OK, "23:59:50");
  RTC_AlarmIntervalEnable(RTC_Alarm_A, 20);
  TEST_Equal(RTC_AlarmDaystamp(RTC_Alarm_A), 10, "00:00:10");
  RTC_AlarmDisable(RTC_Alarm_A);
}

// Each unmasked field has to match, the day as a weekday or as a day of the month
static void alarm_fires_once_on_its_second(void)
{
  TEST_Check(RTC_SetTimestamp(WEDNESDAY_ts) == OK, "set");
  RTC_AlarmCfg_t thursday = RTC_WeekstampToAlarm(RTC_Weekstamp() + 86400);
  RTC_AlarmEnable(RTC_Alarm_B, &thursday);
  TEST_Check(!RTC_Event(RTC_Alarm_B), "another weekday");
  RTC_AlarmCfg_t monthly = {
    .day = 18,
    .hour_mask = true, .minute_mask = true, .second_mask = true
  };
  RTC_AlarmEnable(RTC_Alarm_B, &monthly);
  TEST_Check(RTC_Event(RTC_Alarm_B), "day of the month");
  RTC_AlarmDaystampEnable(RTC_Alarm_A, RTC_Daystamp());
  TEST_Check(RTC_Event(RTC_Alarm_A), "fires on its second");
  TEST_Check(!RTC_Event(RTC_Alarm_A), "once");
  RTC_AlarmDisable(RTC_Alarm_A);
  RTC_AlarmDisable(RTC_Alarm_B);
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(cold_domain_shows_2000); // first: the domain starts with it
  TEST(set_raises_ready_and_init_keeps_the_calendar);
  TEST(reset_drops_ready);
  TEST(calendar_comes_back_exact);
  TEST(date_far_from_a_set_clock_refused);
  TEST(interval_alarm_wraps_at_midnight);
  TEST(alarm_fires_once_on_its_second);
}

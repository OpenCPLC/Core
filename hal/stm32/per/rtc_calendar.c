// hal/stm32/per/rtc_calendar.c

// Calendar and alarm arithmetic over the RTC API, one code for the chip and its host model

#include "rtc.h"

//--------------------------------------------------------------------------------------- Constants

#define RTC_OFFSET_YEAR       1970
#define RTC_SECONDS_IN_WEEK   604800
#define RTC_SECONDS_IN_DAY    86400
#define RTC_SECONDS_IN_HOUR   3600
#define RTC_SECONDS_IN_MIN    60

// Minimum sensible year offset from 2000, used as `RtcReady` sanity check
#define RTC_YEAR_MIN  26

#define RTC_LEAP_YEAR(y)    ((((y) % 4 == 0) && ((y) % 100 != 0)) || ((y) % 400 == 0))
#define RTC_DAYS_IN_YEAR(y) (RTC_LEAP_YEAR(y) ? 366 : 365)
#define RTC_YEAR_VALID(y)   ((y) >= RTC_YEAR_MIN)

//--------------------------------------------------------------------------------------- Variables

static const uint8_t RTC_DAYS_IN_MONTH[2][12] = {
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

#if(RTC_WEEKDAYS_LONGNAMES)
  const char *RtcWeekdays[8] = {
    "Everyday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday", "Sunday"
  };
#else
  const char *RtcWeekdays[8] = {
    "Evd", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
  };
#endif

bool RtcReady;
bool RtcInit;

//---------------------------------------------------------------------------------------- Internal

// `stamp_alarm` inside `[stamp_min, stamp_max]`, a window that may wrap at `stamp_reload`
static bool rtc_check_base(int32_t stamp_min, int32_t stamp_max,
  int32_t stamp_alarm, uint32_t stamp_reload)
{
  if(stamp_min < 0) {
    stamp_min += stamp_reload;
    return (stamp_alarm > stamp_min || stamp_alarm < stamp_max);
  }
  if(stamp_max >= (int32_t)stamp_reload) {
    stamp_max -= stamp_reload;
    return (stamp_alarm > stamp_min || stamp_alarm < stamp_max);
  }
  return (stamp_alarm > stamp_min && stamp_alarm < stamp_max);
}

//----------------------------------------------------------------------------------------- Convert

RTC_Datetime_t RTC_UnixToDatetime(uint64_t timestamp)
{
  RTC_Datetime_t result = {0};
  uint16_t year = RTC_OFFSET_YEAR;
  result.second = timestamp % 60;
  timestamp /= 60;
  result.minute = timestamp % 60;
  timestamp /= 60;
  result.hour = timestamp % 24;
  timestamp /= 24;
  result.week_day = (timestamp + 3) % 7 + 1;
  while(timestamp >= (uint32_t)RTC_DAYS_IN_YEAR(year)) {
    timestamp -= RTC_DAYS_IN_YEAR(year);
    year++;
  }
  for(uint8_t month = 0; month < 12; month++) {
    uint8_t days = RTC_DAYS_IN_MONTH[RTC_LEAP_YEAR(year)][month];
    if(timestamp < days) {
      result.month = month + 1;
      break;
    }
    timestamp -= days;
  }
  result.month_day = timestamp + 1;
  result.year = (year < 2000) ? 0 : (year % 100);
  return result;
}

uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date)
{
  uint32_t days = 0;
  uint16_t year = (uint16_t)(date->year + 2000);
  if(year < RTC_OFFSET_YEAR) return 0;
  for(uint16_t i = RTC_OFFSET_YEAR; i < year; i++) days += RTC_DAYS_IN_YEAR(i);
  for(uint16_t i = 1; i < date->month; i++) {
    days += RTC_DAYS_IN_MONTH[RTC_LEAP_YEAR(year)][i - 1];
  }
  days += date->month_day - 1;
  return (uint64_t)days * RTC_SECONDS_IN_DAY
    + date->hour * RTC_SECONDS_IN_HOUR
    + date->minute * RTC_SECONDS_IN_MIN
    + date->second;
}

const char *RTC_WeekDayString(void)
{
  return RtcWeekdays[RTC_Datetime().week_day];
}

bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone)
{
  if(RtcReady) {
    uint64_t now = RTC_Timestamp();
    uint64_t new_ts = RTC_DatetimeToUnix(date) - ((time_zone * 15) * 60);
    uint64_t diff = (now > new_ts) ? (now - new_ts) : (new_ts - now);
    if(diff > 3600) return false;
  }
  return RTC_YEAR_VALID(date->year)
    && date->second < 60
    && date->minute < 60
    && date->hour <= 23
    && date->month > 0 && date->month <= 12
    && date->month_day > 0 && date->month_day <= 31;
}

//----------------------------------------------------------------------------------- Convert Alarm

RTC_AlarmCfg_t RTC_DaystampToAlarm(uint32_t stamp)
{
  return (RTC_AlarmCfg_t){
    .week = false, .day_mask = true, .day = 0,
    .hour_mask = false, .hour = (stamp / RTC_SECONDS_IN_HOUR) % 24,
    .minute_mask = false, .minute = (stamp / RTC_SECONDS_IN_MIN) % 60,
    .second_mask = false, .second = stamp % 60
  };
}

RTC_AlarmCfg_t RTC_WeekstampToAlarm(uint32_t stamp)
{
  return (RTC_AlarmCfg_t){
    .week = true, .day_mask = false, .day = (stamp / RTC_SECONDS_IN_DAY) + 1,
    .hour_mask = false, .hour = (stamp / RTC_SECONDS_IN_HOUR) % 24,
    .minute_mask = false, .minute = (stamp / RTC_SECONDS_IN_MIN) % 60,
    .second_mask = false, .second = stamp % 60
  };
}

uint32_t RTC_AlarmToDaystamp(const RTC_AlarmCfg_t *alarm)
{
  return alarm->hour * RTC_SECONDS_IN_HOUR
    + alarm->minute * RTC_SECONDS_IN_MIN
    + alarm->second;
}

uint32_t RTC_AlarmToWeekstamp(const RTC_AlarmCfg_t *alarm)
{
  return (alarm->day - 1) * RTC_SECONDS_IN_DAY + RTC_AlarmToDaystamp(alarm);
}

//--------------------------------------------------------------------------------------------- Set

status_t RTC_SetTimestamp(uint64_t timestamp)
{
  RTC_Datetime_t datetime = RTC_UnixToDatetime(timestamp);
  return RTC_SetDatetime(&datetime);
}

status_t RTC_Reset(void)
{
  RTC_Datetime_t datetime = {
    .year = 0, .month = 1, .month_day = 1,
    .hour = 0, .minute = 0, .second = 0
  };
  status_t status = RTC_SetDatetime(&datetime);
  RtcReady = false;
  return status;
}

//--------------------------------------------------------------------------------------------- Get

uint64_t RTC_Timestamp(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return RTC_DatetimeToUnix(&dt);
}

uint64_t RTC_TimestampMs(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return RTC_DatetimeToUnix(&dt) * 1000 + dt.ms;
}

uint32_t RTC_Daystamp(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return dt.hour * RTC_SECONDS_IN_HOUR + dt.minute * RTC_SECONDS_IN_MIN + dt.second;
}

uint32_t RTC_Weekstamp(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return (dt.week_day - 1) * RTC_SECONDS_IN_DAY
    + dt.hour * RTC_SECONDS_IN_HOUR
    + dt.minute * RTC_SECONDS_IN_MIN
    + dt.second;
}

//--------------------------------------------------------------------------------------- Alarm Get

uint32_t RTC_AlarmDaystamp(RTC_Alarm_t alarm)
{
  RTC_AlarmCfg_t cfg = RTC_Alarm(alarm);
  return RTC_AlarmToDaystamp(&cfg);
}

//----------------------------------------------------------------------------------- Alarm Control

void RTC_AlarmDaystampEnable(RTC_Alarm_t alarm, uint32_t stamp)
{
  RTC_AlarmCfg_t cfg = RTC_DaystampToAlarm(stamp);
  RTC_AlarmEnable(alarm, &cfg);
}

void RTC_AlarmWeekstampEnable(RTC_Alarm_t alarm, uint32_t stamp)
{
  RTC_AlarmCfg_t cfg = RTC_WeekstampToAlarm(stamp);
  RTC_AlarmEnable(alarm, &cfg);
}

void RTC_AlarmIntervalEnable(RTC_Alarm_t alarm, uint32_t interval_sec)
{
  uint32_t stamp = (RTC_Timestamp() + interval_sec) % RTC_SECONDS_IN_DAY;
  RTC_AlarmDaystampEnable(alarm, stamp);
}

//------------------------------------------------------------------------------------- Alarm Check

bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  int32_t daystamp = (int32_t)RTC_Daystamp();
  return rtc_check_base(daystamp - offset_min_sec, daystamp + offset_max_sec,
    (int32_t)stamp_alarm, RTC_SECONDS_IN_DAY);
}

bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  int32_t weekstamp = (int32_t)RTC_Weekstamp();
  return rtc_check_base(weekstamp - offset_min_sec, weekstamp + offset_max_sec,
    (int32_t)stamp_alarm, RTC_SECONDS_IN_WEEK);
}

bool RTC_AlarmCheck(RTC_Alarm_t alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  if(!RTC_AlarmIsEnabled(alarm)) return false;
  RTC_AlarmCfg_t cfg = RTC_Alarm(alarm);
  if(cfg.day_mask) {
    return RTC_CheckDaystamp(RTC_AlarmToDaystamp(&cfg), offset_min_sec, offset_max_sec);
  }
  return RTC_CheckWeekstamp(RTC_AlarmToWeekstamp(&cfg), offset_min_sec, offset_max_sec);
}

//-------------------------------------------------------------------------------------------------

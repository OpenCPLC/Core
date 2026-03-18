// hal/host/rtc.c

#include "rtc.h"
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
#else
  #include <sys/time.h>
#endif

//------------------------------------------------------------------------------------------------- Globals

bool RtcReady = false;
bool RtcInit = false;

#if(RTC_WEEKDAYS_LONGNAMES)
  const char *RtcWeekdays[8] = { "Everyday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
#else
  const char *RtcWeekdays[8] = { "Evd", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
#endif

//------------------------------------------------------------------------------------------------- Internal

static int64_t rtc_offset_sec = 0; // offset from system time
static RTC_AlarmCfg_t rtc_alarms[RTC_ALARM_COUNT];
static bool rtc_alarm_enabled[RTC_ALARM_COUNT];
static bool rtc_alarm_event[RTC_ALARM_COUNT];
static bool rtc_wakeup_event = false;

static uint64_t rtc_get_system_ms(void)
{
  #if defined(_WIN32) || defined(_WIN64)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (t / 10000) - 11644473600000ULL; // convert to Unix ms
  #else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
  #endif
}

static const uint8_t days_in_month[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static bool is_leap_year(uint16_t year)
{
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint8_t get_days_in_month(uint8_t month, uint16_t year)
{
  if(month == 2 && is_leap_year(year)) return 29;
  return days_in_month[month];
}

// Convert tm weekday (0=Sun) to RTC weekday (1=Mon, 7=Sun)
static uint8_t tm_wday_to_rtc(int wday)
{
  return wday == 0 ? 7 : (uint8_t)wday;
}

//-------------------------------------------------------------------------------------------------

void RTC_Init(void)
{
  memset(rtc_alarms, 0, sizeof(rtc_alarms));
  memset(rtc_alarm_enabled, 0, sizeof(rtc_alarm_enabled));
  memset(rtc_alarm_event, 0, sizeof(rtc_alarm_event));
  rtc_offset_sec = 0;
  RtcInit = true;
  RtcReady = true;
}

//------------------------------------------------------------------------------------------------- Convert

RTC_Datetime_t RTC_UnixToDatetime(uint64_t timestamp)
{
  RTC_Datetime_t dt = {0};
  time_t t = (time_t)timestamp;
  struct tm *tm = gmtime(&t);
  if(tm) {
    dt.year = (uint8_t)(tm->tm_year - 100); // tm_year is years since 1900
    dt.month = (uint8_t)(tm->tm_mon + 1);
    dt.month_day = (uint8_t)tm->tm_mday;
    dt.week_day = tm_wday_to_rtc(tm->tm_wday);
    dt.hour = (uint8_t)tm->tm_hour;
    dt.minute = (uint8_t)tm->tm_min;
    dt.second = (uint8_t)tm->tm_sec;
    dt.ms = 0;
  }
  return dt;
}

uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date)
{
  struct tm tm = {0};
  tm.tm_year = date->year + 100;
  tm.tm_mon = date->month - 1;
  tm.tm_mday = date->month_day;
  tm.tm_hour = date->hour;
  tm.tm_min = date->minute;
  tm.tm_sec = date->second;
  #if defined(_WIN32) || defined(_WIN64)
    return (uint64_t)_mkgmtime(&tm);
  #else
    return (uint64_t)timegm(&tm);
  #endif
}

const char *RTC_WeekDayString(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  if(dt.week_day >= 1 && dt.week_day <= 7) return RtcWeekdays[dt.week_day];
  return RtcWeekdays[0];
}

bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone)
{
  (void)time_zone;
  if(date->month < 1 || date->month > 12) return false;
  if(date->month_day < 1 || date->month_day > get_days_in_month(date->month, 2000 + date->year)) return false;
  if(date->hour > 23) return false;
  if(date->minute > 59) return false;
  if(date->second > 59) return false;
  return true;
}

//------------------------------------------------------------------------------------------------- Convert alarm

RTC_AlarmCfg_t RTC_DaystampToAlarm(uint32_t stamp)
{
  RTC_AlarmCfg_t alarm = {0};
  alarm.week = false;
  alarm.day_mask = true;
  alarm.second = stamp % 60; stamp /= 60;
  alarm.minute = stamp % 60; stamp /= 60;
  alarm.hour = stamp % 24;
  return alarm;
}

RTC_AlarmCfg_t RTC_WeekstampToAlarm(uint32_t stamp)
{
  RTC_AlarmCfg_t alarm = {0};
  alarm.week = true;
  alarm.second = stamp % 60; stamp /= 60;
  alarm.minute = stamp % 60; stamp /= 60;
  alarm.hour = stamp % 24; stamp /= 24;
  alarm.day = (stamp % 7) + 1;
  return alarm;
}

uint32_t RTC_AlarmToDaystamp(const RTC_AlarmCfg_t *alarm)
{
  return (uint32_t)alarm->hour * 3600 + (uint32_t)alarm->minute * 60 + alarm->second;
}

uint32_t RTC_AlarmToWeekstamp(const RTC_AlarmCfg_t *alarm)
{
  return ((uint32_t)(alarm->day - 1) * 86400) + RTC_AlarmToDaystamp(alarm);
}

//------------------------------------------------------------------------------------------------- Set

void RTC_SetDatetime(RTC_Datetime_t *datetime)
{
  uint64_t target = RTC_DatetimeToUnix(datetime);
  uint64_t now = rtc_get_system_ms() / 1000;
  rtc_offset_sec = (int64_t)target - (int64_t)now;
}

void RTC_SetTimestamp(uint64_t timestamp)
{
  uint64_t now = rtc_get_system_ms() / 1000;
  rtc_offset_sec = (int64_t)timestamp - (int64_t)now;
}

void RTC_Reset(void)
{
  rtc_offset_sec = 0;
  memset(rtc_alarm_enabled, 0, sizeof(rtc_alarm_enabled));
  memset(rtc_alarm_event, 0, sizeof(rtc_alarm_event));
}

//------------------------------------------------------------------------------------------------- Get

RTC_Datetime_t RTC_Datetime(void)
{
  uint64_t ms = rtc_get_system_ms();
  uint64_t ts = ms / 1000 + rtc_offset_sec;
  RTC_Datetime_t dt = RTC_UnixToDatetime(ts);
  dt.ms = ms % 1000;
  return dt;
}

uint64_t RTC_Timestamp(void)
{
  return rtc_get_system_ms() / 1000 + rtc_offset_sec;
}

uint64_t RTC_TimestampMs(void)
{
  return rtc_get_system_ms() + rtc_offset_sec * 1000;
}

uint32_t RTC_Daystamp(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return (uint32_t)dt.hour * 3600 + (uint32_t)dt.minute * 60 + dt.second;
}

uint32_t RTC_Weekstamp(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return ((uint32_t)(dt.week_day - 1) * 86400) + RTC_Daystamp();
}

//------------------------------------------------------------------------------------------------- Alarm get

RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARM_COUNT) return rtc_alarms[alarm];
  return (RTC_AlarmCfg_t){0};
}

uint32_t RTC_AlarmDaystamp(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARM_COUNT) return RTC_AlarmToDaystamp(&rtc_alarms[alarm]);
  return 0;
}

//------------------------------------------------------------------------------------------------- Alarm control

bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARM_COUNT) return rtc_alarm_enabled[alarm];
  return false;
}

void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg)
{
  if(alarm < RTC_ALARM_COUNT) {
    rtc_alarms[alarm] = *cfg;
    rtc_alarm_enabled[alarm] = true;
    rtc_alarm_event[alarm] = false;
  }
}

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
  uint32_t now = RTC_Daystamp();
  RTC_AlarmDaystampEnable(alarm, now + interval_sec);
}

void RTC_AlarmDisable(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARM_COUNT) {
    rtc_alarm_enabled[alarm] = false;
  }
}

//------------------------------------------------------------------------------------------------- Wakeup timer (stub)

void RTC_WakeupTimerEnable(uint32_t sec)
{
  (void)sec;
  // stub - no hardware wakeup on desktop
}

void RTC_WakeupTimerDisable(void)
{
  // stub
}

//------------------------------------------------------------------------------------------------- Check

bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  uint32_t now = RTC_Daystamp();
  int32_t diff = (int32_t)now - (int32_t)stamp_alarm;
  if(diff < 0) diff += 86400; // wrap around midnight
  return (diff >= (int32_t)offset_min_sec && diff <= (int32_t)offset_max_sec);
}

bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  uint32_t now = RTC_Weekstamp();
  int32_t diff = (int32_t)now - (int32_t)stamp_alarm;
  if(diff < 0) diff += 604800; // wrap around week
  return (diff >= (int32_t)offset_min_sec && diff <= (int32_t)offset_max_sec);
}

bool RTC_AlarmCheck(RTC_Alarm_t alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  if(alarm >= RTC_ALARM_COUNT || !rtc_alarm_enabled[alarm]) return false;
  RTC_AlarmCfg_t *cfg = &rtc_alarms[alarm];
  if(cfg->week) {
    return RTC_CheckWeekstamp(RTC_AlarmToWeekstamp(cfg), offset_min_sec, offset_max_sec);
  }
  return RTC_CheckDaystamp(RTC_AlarmToDaystamp(cfg), offset_min_sec, offset_max_sec);
}

//------------------------------------------------------------------------------------------------- Event

bool RTC_Event(RTC_Alarm_t alarm)
{
  if(alarm >= RTC_ALARM_COUNT) return false;
  // Check if alarm should fire (within 1 second window)
  if(rtc_alarm_enabled[alarm] && !rtc_alarm_event[alarm]) {
    if(RTC_AlarmCheck(alarm, 0, 0)) {
      rtc_alarm_event[alarm] = true;
    }
  }
  bool event = rtc_alarm_event[alarm];
  rtc_alarm_event[alarm] = false;
  return event;
}

bool RTC_EventWakeupTimer(void)
{
  bool event = rtc_wakeup_event;
  rtc_wakeup_event = false;
  return event;
}

void RTC_Force(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARM_COUNT) rtc_alarm_event[alarm] = true;
}

void RTC_ForceWakeupTimer(void)
{
  rtc_wakeup_event = true;
}

//-------------------------------------------------------------------------------------------------
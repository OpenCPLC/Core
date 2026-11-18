// hal/host/per/rtc.c

// Model of the RTC on the system clock, its calendar arithmetic the one of the chip.
// The calendar counts on from what was set, and survives a repeated `RTC_Init` as the backup
// domain does. An alarm fires on the second the calendar matches its unmasked fields.

#include "rtc.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
#else
  #include <sys/time.h>
#endif

#include "../../stm32/per/rtc_calendar.c"

//---------------------------------------------------------------------------------------- Internal

#define RTC_ALARMS 2 // slots `A` and `B`, as on the chip

// Calendar the registers of a domain reset hold, 2000-01-01 00:00:00 [ms]
#define RTC_DOMAIN_RESET_ms 946684800000ll

static bool rtc_running; // backup domain clocked: the calendar counts through `RTC_Init`
static int64_t rtc_offset_ms; // the calendar against the system clock
static RTC_AlarmCfg_t rtc_alarms[RTC_ALARMS];
static bool rtc_alarm_enabled[RTC_ALARMS];
static bool rtc_alarm_flag[RTC_ALARMS];
// Second each alarm last fired at, so one match yields one event
static uint64_t rtc_alarm_stamp[RTC_ALARMS];
static bool rtc_wakeup_flag;

static int64_t rtc_get_system_ms(void)
{
  #if defined(_WIN32) || defined(_WIN64)
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  return (int64_t)(t / 10000) - 11644473600000ll; // 1601 to 1970
  #else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
  #endif
}

// Calendar the registers show from now on, the subseconds restarting as the prescaler does
static void rtc_calendar_set(int64_t calendar_ms)
{
  rtc_offset_ms = calendar_ms - rtc_get_system_ms();
}

// The chip matches each unmasked field, the day as a weekday or as a day of the month
static bool rtc_alarm_matches(const RTC_AlarmCfg_t *cfg, const RTC_Datetime_t *now)
{
  if(!cfg->second_mask && cfg->second != now->second) return false;
  if(!cfg->minute_mask && cfg->minute != now->minute) return false;
  if(!cfg->hour_mask && cfg->hour != now->hour) return false;
  if(cfg->day_mask) return true;
  return cfg->day == (cfg->week ? now->week_day : now->month_day);
}

//-------------------------------------------------------------------------------------------- Init

status_t RTC_Init(void)
{
  if(!rtc_running) {
    rtc_calendar_set(RTC_DOMAIN_RESET_ms);
    memset(rtc_alarms, 0, sizeof(rtc_alarms));
    memset(rtc_alarm_enabled, 0, sizeof(rtc_alarm_enabled));
    rtc_running = true;
  }
  RtcInit = true;
  return OK;
}

//--------------------------------------------------------------------------------------------- Set

status_t RTC_SetDatetime(RTC_Datetime_t *datetime)
{
  datetime->week_day = RTC_UnixToDatetime(RTC_DatetimeToUnix(datetime)).week_day;
  if(!rtc_running) return ERR;
  rtc_calendar_set((int64_t)RTC_DatetimeToUnix(datetime) * 1000);
  RtcReady = true;
  return OK;
}

//--------------------------------------------------------------------------------------------- Get

RTC_Datetime_t RTC_Datetime(void)
{
  int64_t ms = rtc_get_system_ms() + rtc_offset_ms;
  RTC_Datetime_t dt = RTC_UnixToDatetime((uint64_t)(ms / 1000));
  dt.ms = (uint16_t)(ms % 1000);
  return dt;
}

//--------------------------------------------------------------------------------------- Alarm Get

RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm)
{
  if(alarm >= RTC_ALARMS) return (RTC_AlarmCfg_t){0};
  return rtc_alarms[alarm];
}

//----------------------------------------------------------------------------------- Alarm Control

bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm)
{
  return alarm < RTC_ALARMS && rtc_alarm_enabled[alarm];
}

void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg)
{
  if(alarm >= RTC_ALARMS) return;
  rtc_alarms[alarm] = *cfg;
  rtc_alarm_enabled[alarm] = true;
}

void RTC_AlarmDisable(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARMS) rtc_alarm_enabled[alarm] = false;
}

//------------------------------------------------------------------------------------ Wakeup Timer

void RTC_WakeupTimerEnable(uint32_t sec)
{
  unused(sec);
  rtc_wakeup_flag = false;
}

void RTC_WakeupTimerDisable(void) {}

//------------------------------------------------------------------------------------------- Event

// The chip latches the flag at the match, the model looks when asked
bool RTC_Event(RTC_Alarm_t alarm)
{
  if(alarm >= RTC_ALARMS) return false;
  if(rtc_alarm_enabled[alarm]) {
    RTC_Datetime_t now = RTC_Datetime();
    uint64_t stamp = RTC_DatetimeToUnix(&now);
    if(rtc_alarm_stamp[alarm] != stamp && rtc_alarm_matches(&rtc_alarms[alarm], &now)) {
      rtc_alarm_stamp[alarm] = stamp;
      rtc_alarm_flag[alarm] = true;
    }
  }
  if(!rtc_alarm_flag[alarm]) return false;
  rtc_alarm_flag[alarm] = false;
  return true;
}

bool RTC_EventWakeupTimer(void)
{
  if(!rtc_wakeup_flag) return false;
  rtc_wakeup_flag = false;
  return true;
}

void RTC_Force(RTC_Alarm_t alarm)
{
  if(alarm < RTC_ALARMS) rtc_alarm_flag[alarm] = true;
}

void RTC_ForceWakeupTimer(void)
{
  rtc_wakeup_flag = true;
}

//-------------------------------------------------------------------------------------------------

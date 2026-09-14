// hal/stm32/per/rtc.h

#ifndef RTC_H_
#define RTC_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif

#include "irq.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef RTC_IRQ_PRIORITY
  // RTC interrupt priority (alarms, wakeup timer)
  #define RTC_IRQ_PRIORITY IRQ_Priority_Low
#endif

#ifndef RTC_WEEKDAYS_LONGNAMES
  // `1` = full weekday names ("Monday"), `0` = short ("Mon")
  #define RTC_WEEKDAYS_LONGNAMES 1
#endif

#ifndef RTC_LSE_RETRY
  // Spin budget for LSE start-up. Counted in loop passes, not milliseconds: `RTC_Init` runs
  // before any tick source is guaranteed, so there is nothing yet to measure time with.
  // A healthy crystal settles well inside this; exhausting it means no crystal is present.
  #define RTC_LSE_RETRY 4000000
#endif

#ifndef RTC_SYNC_RETRY
  // Spin budget for the RTC synchronisation flags `INITF`, `ALRxWF` and `WUTWF`.
  // They answer within two RTCCLK periods, so exhausting this means RTCCLK has stopped.
  #define RTC_SYNC_RETRY 100000
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  RTC_Weekday_Error = -1,
  RTC_Weekday_Everyday = 0,
  RTC_Weekday_Monday = 1,
  RTC_Weekday_Tuesday = 2,
  RTC_Weekday_Wednesday = 3,
  RTC_Weekday_Thursday = 4,
  RTC_Weekday_Friday = 5,
  RTC_Weekday_Saturday = 6,
  RTC_Weekday_Sunday = 7
} RTC_Weekday_t;

typedef enum {
  RTC_Alarm_A = 0,
  RTC_Alarm_B = 1
} RTC_Alarm_t;

/**
 * @brief Calendar date and time.
 * @param year Years since 2000, `0..99`
 * @param month Month `1..12`
 * @param month_day Day of the month `1..31`
 * @param week_day Day of the week `1..7`, Monday first
 * @param hour Hour `0..23`
 * @param minute Minute `0..59`
 * @param second Second `0..59`
 * @param ms Milliseconds `0..999`
 */
typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t month_day;
  uint8_t week_day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t ms;
} RTC_Datetime_t;

/**
 * @brief Alarm match, each masked field matches anything.
 * @param week `day` is a weekday `1..7`, otherwise a day of the month
 * @param day_mask Ignore `day`
 * @param day Weekday or day of the month
 * @param hour_mask Ignore `hour`
 * @param hour Hour `0..23`
 * @param minute_mask Ignore `minute`
 * @param minute Minute `0..59`
 * @param second_mask Ignore `second`
 * @param second Second `0..59`
 */
typedef struct {
  bool week;
  bool day_mask;
  uint8_t day;
  bool hour_mask;
  uint8_t hour;
  bool minute_mask;
  uint8_t minute;
  bool second_mask;
  uint8_t second;
} RTC_AlarmCfg_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Start the `LSE` crystal, weakest drive first. Called by `RTC_Init`, and by
 *   anything else that needs the crystal without the calendar.
 *   The backup domain must be unlocked (`DBP`) before the call.
 * @return `OK` when the crystal oscillates, `ERR` when no drive level starts it
 */
status_t RTC_StartLSE(void);

/**
 * @brief Clock the RTC from `LSE` and arm its interrupts, once before use.
 *   On `ERR` both `RtcInit` and `RtcReady` stay `false` and no clock source is substituted:
 *   a wrong time is worse than no time for anything scheduling on it. The caller decides
 *   between a degraded mode and `panic`.
 * @return `OK` when the RTC runs on `LSE`, `ERR` when the crystal never started
 */
status_t RTC_Init(void);

//----------------------------------------------------------------------------------------- Convert

/**
 * @brief Calendar of a Unix timestamp.
 * @param[in] timestamp Seconds since 1970-01-01 00:00:00 UTC
 * @return Date and time
 */
RTC_Datetime_t RTC_UnixToDatetime(uint64_t timestamp);

/**
 * @brief Unix timestamp of a calendar.
 * @param[in] date Date and time
 * @return Seconds since 1970-01-01
 */
uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date);

// Name of the current weekday, from `RtcWeekdays`
const char *RTC_WeekDayString(void);

/**
 * @brief Field ranges and agreement with the running RTC, within an hour.
 * @param[in] date Date and time
 * @param[in] time_zone Offset in quarter hours, `4` = UTC+1
 * @return `true` when every field is valid and the time is near the RTC
 */
bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone);

//----------------------------------------------------------------------------------- Convert Alarm

// Daystamp: seconds of the day, the day masked; weekstamp: seconds of the week
RTC_AlarmCfg_t RTC_DaystampToAlarm(uint32_t stamp);
RTC_AlarmCfg_t RTC_WeekstampToAlarm(uint32_t stamp);
uint32_t RTC_AlarmToDaystamp(const RTC_AlarmCfg_t *alarm);
uint32_t RTC_AlarmToWeekstamp(const RTC_AlarmCfg_t *alarm);

//--------------------------------------------------------------------------------------------- Set

// Set the calendar, the weekday is computed into `datetime`
// `ERR` when the RTC never entered init mode: the write was dropped and `RtcReady` stays down
status_t RTC_SetDatetime(RTC_Datetime_t *datetime);
status_t RTC_SetTimestamp(uint64_t timestamp);
// Back to 2000-01-01 00:00:00, `RtcReady` cleared
void RTC_Reset(void);

//--------------------------------------------------------------------------------------------- Get

// Current calendar, Unix timestamp and its milliseconds, seconds of the day and the week
RTC_Datetime_t RTC_Datetime(void);
uint64_t RTC_Timestamp(void);
uint64_t RTC_TimestampMs(void);
uint32_t RTC_Daystamp(void);
uint32_t RTC_Weekstamp(void);

//--------------------------------------------------------------------------------------- Alarm Get

// Match of an alarm slot, as configured or as seconds of the day
RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm);
uint32_t RTC_AlarmDaystamp(RTC_Alarm_t alarm);

//----------------------------------------------------------------------------------- Alarm Control

// Arm an alarm slot on a match, a daystamp, a weekstamp or an interval from now
bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm);
void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg);
void RTC_AlarmDaystampEnable(RTC_Alarm_t alarm, uint32_t stamp);
void RTC_AlarmWeekstampEnable(RTC_Alarm_t alarm, uint32_t stamp);
void RTC_AlarmIntervalEnable(RTC_Alarm_t alarm, uint32_t interval_sec);
void RTC_AlarmDisable(RTC_Alarm_t alarm);

//------------------------------------------------------------------------------------ Wakeup Timer

// Periodic wakeup every `sec` seconds
void RTC_WakeupTimerEnable(uint32_t sec);
void RTC_WakeupTimerDisable(void);

//------------------------------------------------------------------------------------- Alarm Check

/**
 * @brief Stamp inside the window `[now - offset_min_sec, now + offset_max_sec]`,
 *   wrapping at the day or week end.
 * @param[in] stamp_alarm Seconds of the day, or of the week
 * @param[in] offset_min_sec Window start before now
 * @param[in] offset_max_sec Window end after now
 * @return `true` when the stamp is in the window
 */
bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);
bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

/**
 * @brief Alarm slot armed and inside the window around now, see `RTC_CheckDaystamp`.
 * @param[in] alarm Alarm slot
 * @param[in] offset_min_sec Window start before now
 * @param[in] offset_max_sec Window end after now
 * @return `true` when armed and in the window
 */
bool RTC_AlarmCheck(RTC_Alarm_t alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

//------------------------------------------------------------------------------------------- Event

// Event flags, cleared on read; `Force` raises one from software
bool RTC_Event(RTC_Alarm_t alarm);
bool RTC_EventWakeupTimer(void);
void RTC_Force(RTC_Alarm_t alarm);
void RTC_ForceWakeupTimer(void);

//----------------------------------------------------------------------------------------- Globals

extern const char *RtcWeekdays[];
extern bool RtcReady;
extern bool RtcInit;

//-------------------------------------------------------------------------------------------------

#endif

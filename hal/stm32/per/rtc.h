// hal/stm32/per/rtc.h

#ifndef RTC_H_
#define RTC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
 * @brief RTC datetime structure.
 * @param[in] year Year offset from 2000 (`0`-`99` = 2000-2099)
 * @param[in] month Month `1`-`12`
 * @param[in] month_day Day of month `1`-`31`
 * @param[in] week_day Day of week `1`-`7` (Mon-Sun)
 * @param[in] hour Hour `0`-`23`
 * @param[in] minute Minute `0`-`59`
 * @param[in] second Second `0`-`59`
 * @param[in] ms Milliseconds `0`-`999`
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
 * @brief RTC alarm configuration.
 * @param[in] week `true` = day field is weekday (1-7), `false` = month day
 * @param[in] day_mask `true` = ignore day field
 * @param[in] day Day value (weekday or month day)
 * @param[in] hour_mask `true` = ignore hour field
 * @param[in] hour Hour `0`-`23`
 * @param[in] minute_mask `true` = ignore minute field
 * @param[in] minute Minute `0`-`59`
 * @param[in] second_mask `true` = ignore second field
 * @param[in] second Second `0`-`59`
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

// Initialize RTC peripheral, enable LSE, configure NVIC — call once before use
void RTC_Init(void);

//----------------------------------------------------------------------------------------- Convert

/**
 * @brief Convert Unix timestamp to datetime structure.
 * @param[in] timestamp Seconds since 1970-01-01 00:00:00 UTC
 * @return Decoded datetime
 */
RTC_Datetime_t RTC_UnixToDatetime(uint64_t timestamp);

/**
 * @brief Convert datetime to Unix timestamp.
 * @param[in] date Datetime to encode
 * @return Seconds since 1970-01-01, or `0` if year < 1970
 */
uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date);

// Current weekday name from `RtcWeekdays` table
const char *RTC_WeekDayString(void);

/**
 * @brief Sanity-check datetime against current RTC and field ranges.
 * @param[in] date Datetime to check
 * @param[in] time_zone Quarter-hour offset (e.g. `4` = UTC+1)
 * @return `true` if all fields valid and within 1h of current RTC time
 */
bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone);

//----------------------------------------------------------------------------------- Convert Alarm

// Convert seconds-of-day to alarm config (day masked)
RTC_AlarmCfg_t RTC_DaystampToAlarm(uint32_t stamp);
// Convert seconds-of-week to alarm config (day = weekday)
RTC_AlarmCfg_t RTC_WeekstampToAlarm(uint32_t stamp);
// Encode alarm config to seconds-of-day (ignores day field)
uint32_t RTC_AlarmToDaystamp(const RTC_AlarmCfg_t *alarm);
// Encode alarm config to seconds-of-week
uint32_t RTC_AlarmToWeekstamp(const RTC_AlarmCfg_t *alarm);

//--------------------------------------------------------------------------------------------- Set

// Write datetime to RTC, computes weekday automatically
void RTC_SetDatetime(RTC_Datetime_t *datetime);
// Write Unix timestamp to RTC
void RTC_SetTimestamp(uint64_t timestamp);
// Reset RTC to 2000-01-01 00:00:00 and clear `RtcReady`
void RTC_Reset(void);

//--------------------------------------------------------------------------------------------- Get

RTC_Datetime_t RTC_Datetime(void);
uint64_t RTC_Timestamp(void);
uint64_t RTC_TimestampMs(void);
uint32_t RTC_Daystamp(void);
uint32_t RTC_Weekstamp(void);

//--------------------------------------------------------------------------------------- Alarm Get

RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm);
uint32_t RTC_AlarmDaystamp(RTC_Alarm_t alarm);

//----------------------------------------------------------------------------------- Alarm Control

bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm);
void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg);
void RTC_AlarmDaystampEnable(RTC_Alarm_t alarm, uint32_t stamp);
void RTC_AlarmWeekstampEnable(RTC_Alarm_t alarm, uint32_t stamp);
void RTC_AlarmIntervalEnable(RTC_Alarm_t alarm, uint32_t interval_sec);
void RTC_AlarmDisable(RTC_Alarm_t alarm);

//------------------------------------------------------------------------------------- Wakeup Timer

void RTC_WakeupTimerEnable(uint32_t sec);
void RTC_WakeupTimerDisable(void);

//------------------------------------------------------------------------------------- Alarm Check

/**
 * @brief Check if alarm daystamp is within `[now - min, now + max]` window.
 * @param[in] stamp_alarm Alarm time (seconds-of-day)
 * @param[in] offset_min_sec Window start offset before now
 * @param[in] offset_max_sec Window end offset after now
 * @return `true` if alarm is in window
 */
bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

/**
 * @brief Check if alarm weekstamp is within `[now - min, now + max]` window.
 * @param[in] stamp_alarm Alarm time (seconds-of-week)
 * @param[in] offset_min_sec Window start offset before now
 * @param[in] offset_max_sec Window end offset after now
 * @return `true` if alarm is in window
 */
bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

/**
 * @brief Check if RTC alarm config is within time window of current RTC.
 * @param[in] alarm Alarm slot to check
 * @param[in] offset_min_sec Window start offset before now
 * @param[in] offset_max_sec Window end offset after now
 * @return `true` if alarm enabled and in window
 */
bool RTC_AlarmCheck(RTC_Alarm_t alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

//------------------------------------------------------------------------------------------- Event

// Consume alarm event flag (one-shot)
bool RTC_Event(RTC_Alarm_t alarm);
// Consume wakeup-timer event flag (one-shot)
bool RTC_EventWakeupTimer(void);
// Set alarm event flag manually (software trigger)
void RTC_Force(RTC_Alarm_t alarm);
// Set wakeup-timer event flag manually
void RTC_ForceWakeupTimer(void);

//----------------------------------------------------------------------------------------- Globals

extern const char *RtcWeekdays[];
extern bool RtcReady;
extern bool RtcInit;

//---------------------------------------------------------------------------------------------

#endif
// hal/host/rtc.h

#ifndef RTC_H_
#define RTC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "xdef.h"

//------------------------------------------------------------------------------------------------- Config

#ifndef RTC_WEEKDAYS_LONGNAMES
  #define RTC_WEEKDAYS_LONGNAMES ON
#endif

#ifndef RTC_ALARM_COUNT
  #define RTC_ALARM_COUNT 2
#endif

//------------------------------------------------------------------------------------------------- Types

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
 * @param[in] year Year (2-digit, 00-99 = 2000-2099)
 * @param[in] month Month (1-12)
 * @param[in] month_day Day of month (1-31)
 * @param[in] week_day Day of week (1-7, Mon-Sun)
 * @param[in] hour Hour (0-23)
 * @param[in] minute Minute (0-59)
 * @param[in] second Second (0-59)
 * @param[in] ms Milliseconds (0-999)
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
 * @param[in] week `true` = day is weekday (1-7), `false` = day is month day
 * @param[in] day_mask `true` = ignore day field
 * @param[in] day Day value (weekday or month day)
 * @param[in] hour_mask `true` = ignore hour field
 * @param[in] hour Hour (0-23)
 * @param[in] minute_mask `true` = ignore minute field
 * @param[in] minute Minute (0-59)
 * @param[in] second_mask `true` = ignore second field
 * @param[in] second Second (0-59)
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

//------------------------------------------------------------------------------------------------- API

void RTC_Init(void);

// Convert
RTC_Datetime_t RTC_UnixToDatetime(uint64_t timestamp);
uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date);
const char *RTC_WeekDayString(void);
bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone);

// Convert alarm
RTC_AlarmCfg_t RTC_DaystampToAlarm(uint32_t stamp);
RTC_AlarmCfg_t RTC_WeekstampToAlarm(uint32_t stamp);
uint32_t RTC_AlarmToDaystamp(const RTC_AlarmCfg_t *alarm);
uint32_t RTC_AlarmToWeekstamp(const RTC_AlarmCfg_t *alarm);

// Set
void RTC_SetDatetime(RTC_Datetime_t *datetime);
void RTC_SetTimestamp(uint64_t timestamp);
void RTC_Reset(void);

// Get
RTC_Datetime_t RTC_Datetime(void);
uint64_t RTC_Timestamp(void);
uint64_t RTC_TimestampMs(void);
uint32_t RTC_Daystamp(void);
uint32_t RTC_Weekstamp(void);

// Alarm get
RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm);
uint32_t RTC_AlarmDaystamp(RTC_Alarm_t alarm);

// Alarm control
bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm);
void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg);
void RTC_AlarmDaystampEnable(RTC_Alarm_t alarm, uint32_t stamp);
void RTC_AlarmWeekstampEnable(RTC_Alarm_t alarm, uint32_t stamp);
void RTC_AlarmIntervalEnable(RTC_Alarm_t alarm, uint32_t interval_sec);
void RTC_AlarmDisable(RTC_Alarm_t alarm);

// Wakeup timer (stub on desktop)
void RTC_WakeupTimerEnable(uint32_t sec);
void RTC_WakeupTimerDisable(void);

// Check
bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);
bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);
bool RTC_AlarmCheck(RTC_Alarm_t alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

// Event
bool RTC_Event(RTC_Alarm_t alarm);
bool RTC_EventWakeupTimer(void);
void RTC_Force(RTC_Alarm_t alarm);
void RTC_ForceWakeupTimer(void);

//------------------------------------------------------------------------------------------------- Globals

extern const char *RtcWeekdays[];
extern bool RtcReady;
extern bool RtcInit;

//-------------------------------------------------------------------------------------------------
#endif
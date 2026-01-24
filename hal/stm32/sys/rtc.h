// lib/per/rtc.h

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

//-------------------------------------------------------------------------------------------------

#ifndef RTC_IRQ_PRIORITY
  #define RTC_IRQ_PRIORITY IRQ_Priority_Low
#endif

#ifndef RTC_WEEKDAYS_LONGNAMES
  #define RTC_WEEKDAYS_LONGNAMES 1
#endif

//-------------------------------------------------------------------------------------------------

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
} RTC_Weekday_e;

typedef enum {
  RTC_Alarm_A = 0,
  RTC_Alarm_B = 1
} RTC_Alarm_e;

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
} RTC_Alarm_t;

//-------------------------------------------------------------------------------------------------

void RTC_Init(void);

// Convert
RTC_Datetime_t RTC_UnixToDatetime(uint64_t timestamp);
uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date);
const char *RTC_WeekDayString(void);
bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone);

// Convert alarm
RTC_Alarm_t RTC_DaystampToAlarm(uint32_t stamp);
RTC_Alarm_t RTC_WeekstampToAlarm(uint32_t stamp);
uint32_t RTC_AlarmToDaystamp(const RTC_Alarm_t *alarm);
uint32_t RTC_AlarmToWeekstamp(const RTC_Alarm_t *alarm);

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
RTC_Alarm_t RTC_Alarm(RTC_Alarm_e alarm);
uint32_t RTC_AlarmDaystamp(RTC_Alarm_e alarm);

// Alarm control
bool RTC_AlarmIsEnabled(RTC_Alarm_e alarm);
void RTC_AlarmEnable(RTC_Alarm_e alarm, const RTC_Alarm_t *cfg);
void RTC_AlarmDaystampEnable(RTC_Alarm_e alarm, uint32_t stamp);
void RTC_AlarmWeekstampEnable(RTC_Alarm_e alarm, uint32_t stamp);
void RTC_AlarmIntervalEnable(RTC_Alarm_e alarm, uint32_t interval_sec);
void RTC_AlarmDisable(RTC_Alarm_e alarm);

// Wakeup timer
void RTC_WakeupTimerEnable(uint32_t sec);
void RTC_WakeupTimerDisable(void);

// Check
bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);
bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);
bool RTC_AlarmCheck(RTC_Alarm_e alarm, uint32_t offset_min_sec, uint32_t offset_max_sec);

// Event
bool RTC_Event(RTC_Alarm_e alarm);
bool RTC_EventWakeupTimer(void);
void RTC_Force(RTC_Alarm_e alarm);
void RTC_ForceWakeupTimer(void);

//-------------------------------------------------------------------------------------------------

extern const char *RtcWeekdays[];
extern bool RtcReady;
extern bool RtcInit;

//-------------------------------------------------------------------------------------------------
#endif

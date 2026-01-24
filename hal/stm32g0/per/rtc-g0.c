// lib/per/rtc.c

#include "rtc.h"

//------------------------------------------------------------------------------------------------- DEFINE

#define RTC_LSE_FREQUENCY_Hz  32768
#define PREDIV_SYNC           255
#define PREDIV_ASYNC          127
#define RTC_LEAP_YEAR(year)   ((((year) % 4 == 0) && ((year) % 100 != 0)) || ((year) % 400 == 0))
#define RTC_DAYS_IN_YEAR(x)   (RTC_LEAP_YEAR(x) ? 366 : 365)
#define RTC_OFFSET_YEAR       1970
#define RTC_SECONDS_IN_WEEK   604800
#define RTC_SECONDS_IN_DAY    86400
#define RTC_SECONDS_IN_HOUR   3600
#define RTC_SECONDS_IN_MIN    60

#define RTC_WPR_KEY1  0xCA
#define RTC_WPR_KEY2  0x53
#define RTC_WPR_LOCK  0xFF

#define RTC_YEAR_MIN        55
#define RTC_YEAR_VALID(y)   ((y) >= RTC_YEAR_MIN)

// ms = (PREDIV_SYNC - SSR) * 1000 / (PREDIV_SYNC + 1)
#define RTC_SSR_TO_MS(ssr)  (((PREDIV_SYNC - (ssr)) * 1000) / (PREDIV_SYNC + 1))

//------------------------------------------------------------------------------------------------- VAR

static volatile struct {
  bool alarm_a;
  bool alarm_b;
  bool wakeup_timer;
} rtc_flags;

static const uint8_t RTC_DAYS_IN_MONTH[2][12] = {
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },  // Not leap year
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }   // Leap year
};

#if(RTC_WEEKDAYS_LONGNAMES)
  const char *RtcWeekdays[8] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
#else
  const char *RtcWeekdays[8] = { "Evd", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
#endif

bool RtcReady;
bool RtcInit;

//------------------------------------------------------------------------------------------------- STATIC

static inline void rtc_unlock(void)
{
  RTC->WPR = RTC_WPR_KEY1;
  RTC->WPR = RTC_WPR_KEY2;
}

static inline void rtc_lock(void)
{
  RTC->WPR = RTC_WPR_LOCK;
}

static inline volatile uint32_t *rtc_alarm_reg(RTC_Alarm_e alarm)
{
  return (alarm == RTC_Alarm_A) ? &RTC->ALRMAR : &RTC->ALRMBR;
}

static inline uint32_t rtc_alarm_mask(RTC_Alarm_e alarm)
{
  return (alarm == RTC_Alarm_A) ? RTC_CR_ALRAE : RTC_CR_ALRBE;
}

/**
 * @brief Calculate week day from datetime
 * Computes `week_day` field based on Unix timestamp.
 * @param[in,out] datetime Pointer to `RTC_Datetime_t`, `week_day` field is updated
 * @return Computed week day `1`-`7` (Mon-Sun)
 */
static uint8_t rtc_weekday_calc(RTC_Datetime_t *datetime)
{
  uint64_t timestamp = RTC_DatetimeToUnix(datetime);
  datetime->week_day = (timestamp / RTC_SECONDS_IN_DAY + 3) % 7 + 1;
  return datetime->week_day;
}

/**
 * @brief Build DR register value from datetime
 * @param[in] date Pointer to `RTC_Datetime_t` source
 * @return Value for `RTC->DR` register
 */
static uint32_t rtc_date_register(const RTC_Datetime_t *date)
{
  return ((date->year / 10)      << RTC_DR_YT_Pos)  |
         ((date->year % 10)      << RTC_DR_YU_Pos)  |
         ((date->week_day)       << RTC_DR_WDU_Pos) |
         ((date->month / 10)     << RTC_DR_MT_Pos)  |
         ((date->month % 10)     << RTC_DR_MU_Pos)  |
         ((date->month_day / 10) << RTC_DR_DT_Pos)  |
         ((date->month_day % 10) << RTC_DR_DU_Pos);
}

/**
 * @brief Build TR register value from datetime
 * @param[in] date Pointer to `RTC_Datetime_t` source
 * @return Value for `RTC->TR` register
 */
static uint32_t rtc_time_register(const RTC_Datetime_t *date)
{
  return ((date->hour / 10)   << RTC_TR_HT_Pos)  |
         ((date->hour % 10)   << RTC_TR_HU_Pos)  |
         ((date->minute / 10) << RTC_TR_MNT_Pos) |
         ((date->minute % 10) << RTC_TR_MNU_Pos) |
         ((date->second / 10) << RTC_TR_ST_Pos)  |
         ((date->second % 10) << RTC_TR_SU_Pos);
}

/**
 * @brief Base function for daystamp/weekstamp range check
 * Handles wrap-around at midnight/week boundary.
 * @param[in] stamp Current stamp value
 * @param[in] stamp_min Lower bound (can be negative for wrap)
 * @param[in] stamp_max Upper bound
 * @param[in] stamp_alarm Alarm stamp to check
 * @param[in] stamp_reload Wrap value (`RTC_SECONDS_IN_DAY` or `RTC_SECONDS_IN_WEEK`)
 * @return `true` if `stamp_alarm` falls within range
 */
static bool rtc_check_base(int32_t stamp, int32_t stamp_min, int32_t stamp_max,  int32_t stamp_alarm, uint32_t stamp_reload)
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

//------------------------------------------------------------------------------------------------- INIT

/**
 * @brief Initialize RTC peripheral
 * Enables LSE, configures prescalers, enables alarm interrupts.
 * Must be called before any other RTC function.
 */
void RTC_Init(void)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN;
  RCC->BDCR |= RCC_BDCR_BDRST;
  RCC->BDCR &= ~RCC_BDCR_BDRST;
  PWR->CR1 |= PWR_CR1_DBP;
  RCC->BDCR |= RCC_BDCR_LSEON;
  while(!(RCC->BDCR & RCC_BDCR_LSERDY)) __DSB();
  RCC->BDCR |= RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN;
  rtc_unlock();
  RTC->CR |= RTC_CR_TSIE | RTC_CR_WUTIE | RTC_CR_ALRBIE | RTC_CR_ALRAIE;
  rtc_lock();
  NVIC_ClearPendingIRQ(RTC_TAMP_IRQn);
  NVIC_EnableIRQ(RTC_TAMP_IRQn);
  NVIC_SetPriority(RTC_TAMP_IRQn, RTC_IRQ_PRIORITY);
  RtcInit = true;
}

//------------------------------------------------------------------------------------------------- CONVERT

/**
 * @brief Convert Unix timestamp to datetime structure
 * @param[in] timestamp Unix timestamp (seconds since 1970-01-01)
 * @return `RTC_Datetime_t` with year as 2-digit (00-99), `ms` = 0
 */
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

/**
 * @brief Convert datetime to Unix timestamp
 * @param[in] date Pointer to `RTC_Datetime_t` (year as 2-digit, assumes 20xx)
 * @return Unix timestamp, `0` if year < 1970
 */
uint64_t RTC_DatetimeToUnix(const RTC_Datetime_t *date)
{
  uint32_t days = 0;
  uint16_t year = (uint16_t)(date->year + 2000);
  if(year < RTC_OFFSET_YEAR) return 0;
  for(uint16_t i = RTC_OFFSET_YEAR; i < year; i++) days += RTC_DAYS_IN_YEAR(i);
  for(uint16_t i = 1; i < date->month; i++) days += RTC_DAYS_IN_MONTH[RTC_LEAP_YEAR(year)][i - 1];
  days += date->month_day - 1;
  return (uint64_t)days * RTC_SECONDS_IN_DAY +
         date->hour * RTC_SECONDS_IN_HOUR +
         date->minute * RTC_SECONDS_IN_MIN +
         date->second;
}

/**
 * @brief Get weekday name string
 * @return Pointer to weekday string for current RTC time
 */
const char *RTC_WeekDayString(void)
{
  return RtcWeekdays[RTC_Datetime().week_day];
}

/**
 * @brief Validate datetime
 * Checks if `date` is plausible and within 1 hour of current RTC time.
 * @param[in] date Datetime to validate
 * @param[in] time_zone Timezone offset in 15-minute units
 * @return `true` if is correct and sync
 */
bool RTC_DatetimeIsCorrect(const RTC_Datetime_t *date, int8_t time_zone)
{
  if(RtcReady) {
    uint64_t now = RTC_Timestamp();
    uint64_t new_ts = RTC_DatetimeToUnix(date) - ((time_zone * 15) * 60);
    uint64_t diff = (now > new_ts) ? (now - new_ts) : (new_ts - now);
    if(diff > 3600) return false;
  }
  return RTC_YEAR_VALID(date->year) &&
    date->second < 60 &&
    date->hour <= 23 &&
    date->month > 0 && date->month <= 12 &&
    date->month_day > 0 && date->month_day <= 31;
}

//------------------------------------------------------------------------------------------------- CONVERT-ALARM

/**
 * @brief Convert daystamp to alarm config
 * Creates alarm that triggers daily at given time.
 * @param[in] stamp Seconds from midnight
 * @return `RTC_Alarm_t` with `day_mask = 1` (ignore day)
 */
RTC_Alarm_t RTC_DaystampToAlarm(uint32_t stamp)
{
  RTC_Alarm_t alarm = {0};
  alarm.day_mask = true;
  stamp %= RTC_SECONDS_IN_DAY;
  alarm.hour = stamp / RTC_SECONDS_IN_HOUR;
  stamp %= RTC_SECONDS_IN_HOUR;
  alarm.minute = stamp / RTC_SECONDS_IN_MIN;
  alarm.second = stamp % RTC_SECONDS_IN_MIN;
  return alarm;
}

/**
 * @brief Convert weekstamp to alarm config
 * Creates alarm that triggers weekly at given day/time.
 * @param[in] stamp Seconds from Monday 00:00
 * @return `RTC_Alarm_t` with `week = true`, `day` = weekday
 */
RTC_Alarm_t RTC_WeekstampToAlarm(uint32_t stamp)
{
  RTC_Alarm_t alarm = {0};
  alarm.week = true;
  stamp %= RTC_SECONDS_IN_WEEK;
  alarm.day = (stamp / RTC_SECONDS_IN_DAY) + 1;
  stamp %= RTC_SECONDS_IN_DAY;
  alarm.hour = stamp / RTC_SECONDS_IN_HOUR;
  stamp %= RTC_SECONDS_IN_HOUR;
  alarm.minute = stamp / RTC_SECONDS_IN_MIN;
  alarm.second = stamp % RTC_SECONDS_IN_MIN;
  return alarm;
}

/**
 * @brief Convert alarm to daystamp
 * @param[in] alarm Pointer to `RTC_Alarm_t`
 * @return Seconds from midnight
 */
uint32_t RTC_AlarmToDaystamp(const RTC_Alarm_t *alarm)
{
  return alarm->hour * RTC_SECONDS_IN_HOUR + 
         alarm->minute * RTC_SECONDS_IN_MIN + 
         alarm->second;
}

/**
 * @brief Convert alarm to weekstamp
 * @param[in] alarm Pointer to `RTC_Alarm_t`
 * @return Seconds from Monday 00:00
 */
uint32_t RTC_AlarmToWeekstamp(const RTC_Alarm_t *alarm)
{
  uint32_t weekstamp = RTC_AlarmToDaystamp(alarm);
  if(!alarm->day_mask) weekstamp += (alarm->day - 1) * RTC_SECONDS_IN_DAY;
  return weekstamp;
}

//------------------------------------------------------------------------------------------------- SET

/**
 * @brief Set RTC datetime
 * Calculates weekday automatically. Sets `RtcReady` if year >= `RTC_YEAR_MIN`.
 * @param[in,out] datetime Pointer to `RTC_Datetime_t`, `week_day` is updated
 */
void RTC_SetDatetime(RTC_Datetime_t *datetime)
{
  rtc_weekday_calc(datetime);
  uint32_t tr = rtc_time_register(datetime);
  uint32_t dr = rtc_date_register(datetime);
  rtc_unlock();
  RTC->ICSR |= RTC_ICSR_INIT;
  while(!(RTC->ICSR & RTC_ICSR_INITF)) __DSB();
  RTC->TR = tr;
  RTC->DR = dr;
  RTC->ICSR &= ~RTC_ICSR_INIT;
  while(!(RTC->ICSR & RTC_ICSR_RSF));
  rtc_lock();
  RtcReady = RTC_YEAR_VALID(datetime->year);
}

/**
 * @brief Set RTC from Unix timestamp
 * @param[in] timestamp Unix timestamp
 */
void RTC_SetTimestamp(uint64_t timestamp)
{
  RTC_Datetime_t date = RTC_UnixToDatetime(timestamp);
  RTC_SetDatetime(&date);
}

/**
 * @brief Reset RTC to 2020-01-01 00:00:00
 * Clears `RtcReady` flag.
 */
void RTC_Reset(void)
{
  RTC_Datetime_t date = { .year = 20, .month = 1, .month_day = 1 };
  RTC_SetDatetime(&date);
  RtcReady = false;
}

//------------------------------------------------------------------------------------------------- GET

/**
 * @brief Read current RTC datetime
 * Updates `RtcReady` if year >= `RTC_YEAR_MIN`.
 * @return Current `RTC_Datetime_t` with milliseconds
 */
RTC_Datetime_t RTC_Datetime(void)
{
  RTC_Datetime_t datetime;
  RTC->ICSR &= ~RTC_ICSR_RSF;
  while(!(RTC->ICSR & RTC_ICSR_RSF)) __DSB();
  uint32_t subsecond = RTC->SSR;
  uint32_t time = RTC->TR;
  uint32_t date = RTC->DR;
  datetime.year      = ((date & RTC_DR_YT) >> RTC_DR_YT_Pos) * 10 + ((date & RTC_DR_YU) >> RTC_DR_YU_Pos);
  datetime.week_day  = (date & RTC_DR_WDU) >> RTC_DR_WDU_Pos;
  datetime.month     = ((date & RTC_DR_MT) >> RTC_DR_MT_Pos) * 10 + ((date & RTC_DR_MU) >> RTC_DR_MU_Pos);
  datetime.month_day = ((date & RTC_DR_DT) >> RTC_DR_DT_Pos) * 10 + ((date & RTC_DR_DU) >> RTC_DR_DU_Pos);
  datetime.hour      = ((time & RTC_TR_HT) >> RTC_TR_HT_Pos) * 10 + ((time & RTC_TR_HU) >> RTC_TR_HU_Pos);
  datetime.minute    = ((time & RTC_TR_MNT) >> RTC_TR_MNT_Pos) * 10 + ((time & RTC_TR_MNU) >> RTC_TR_MNU_Pos);
  datetime.second    = ((time & RTC_TR_ST) >> RTC_TR_ST_Pos) * 10 + ((time & RTC_TR_SU) >> RTC_TR_SU_Pos);
  datetime.ms        = RTC_SSR_TO_MS(subsecond);
  if(!RtcReady) RtcReady = RTC_YEAR_VALID(datetime.year);
  return datetime;
}

/**
 * @brief Get current Unix timestamp
 * @return Seconds since 1970-01-01
 */
uint64_t RTC_Timestamp(void)
{
  RTC_Datetime_t date = RTC_Datetime();
  return RTC_DatetimeToUnix(&date);
}

/**
 * @brief Get current Unix timestamp with milliseconds
 * @return Milliseconds since 1970-01-01
 */
uint64_t RTC_TimestampMs(void)
{
  RTC_Datetime_t date = RTC_Datetime();
  return RTC_DatetimeToUnix(&date) * 1000 + date.ms;
}

/**
 * @brief Get seconds since midnight
 * @return `0` to `86399`
 */
uint32_t RTC_Daystamp(void)
{
  return RTC_Timestamp() % RTC_SECONDS_IN_DAY;
}

/**
 * @brief Get seconds since Monday 00:00
 * @return `0` to `604799`
 */
uint32_t RTC_Weekstamp(void)
{
  return (3 * RTC_SECONDS_IN_DAY + RTC_Timestamp()) % RTC_SECONDS_IN_WEEK;
}

//------------------------------------------------------------------------------------------------- ALARM-GET

/**
 * @brief Read alarm configuration
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @return Current `RTC_Alarm_t` settings
 */
RTC_Alarm_t RTC_Alarm(RTC_Alarm_e alarm)
{
  uint32_t reg = *rtc_alarm_reg(alarm);
  return (RTC_Alarm_t){
    .week        = !!(reg & RTC_ALRMAR_WDSEL),
    .day_mask    = !!(reg & RTC_ALRMAR_MSK4),
    .hour_mask   = !!(reg & RTC_ALRMAR_MSK3),
    .minute_mask = !!(reg & RTC_ALRMAR_MSK2),
    .second_mask = !!(reg & RTC_ALRMAR_MSK1),
    .day    = ((reg >> RTC_ALRMAR_DT_Pos) & 0x03) * 10 + ((reg >> RTC_ALRMAR_DU_Pos) & 0x0F),
    .hour   = ((reg >> RTC_ALRMAR_HT_Pos) & 0x03) * 10 + ((reg >> RTC_ALRMAR_HU_Pos) & 0x0F),
    .minute = ((reg >> RTC_ALRMAR_MNT_Pos) & 0x07) * 10 + ((reg >> RTC_ALRMAR_MNU_Pos) & 0x0F),
    .second = ((reg >> RTC_ALRMAR_ST_Pos) & 0x07) * 10 + ((reg >> RTC_ALRMAR_SU_Pos) & 0x0F),
  };
}

/**
 * @brief Get alarm as daystamp
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @return Seconds from midnight
 */
uint32_t RTC_AlarmDaystamp(RTC_Alarm_e alarm)
{
  RTC_Alarm_t cfg = RTC_Alarm(alarm);
  return RTC_AlarmToDaystamp(&cfg);
}

//------------------------------------------------------------------------------------------------- ALARM-ENABLE

/**
 * @brief Check if alarm is enabled
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @return `true` if enabled
 */
bool RTC_AlarmIsEnabled(RTC_Alarm_e alarm)
{
  return !!(RTC->CR & rtc_alarm_mask(alarm));
}

/**
 * @brief Enable and configure alarm
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @param[in] cfg Pointer to `RTC_Alarm_t` configuration
 */
void RTC_AlarmEnable(RTC_Alarm_e alarm, const RTC_Alarm_t *cfg)
{
  uint32_t mask = rtc_alarm_mask(alarm);
  rtc_unlock();
  RTC->CR &= ~mask;
  while(!(RTC->ICSR & RTC_ICSR_ALRAWF)) __DSB();
  *rtc_alarm_reg(alarm) =
    (cfg->week         << RTC_ALRMAR_WDSEL_Pos) |
    (cfg->day_mask     << RTC_ALRMAR_MSK4_Pos)  |
    (cfg->hour_mask    << RTC_ALRMAR_MSK3_Pos)  |
    (cfg->minute_mask  << RTC_ALRMAR_MSK2_Pos)  |
    (cfg->second_mask  << RTC_ALRMAR_MSK1_Pos)  |
    ((cfg->day / 10)    << RTC_ALRMAR_DT_Pos)   |
    ((cfg->day % 10)    << RTC_ALRMAR_DU_Pos)   |
    ((cfg->hour / 10)   << RTC_ALRMAR_HT_Pos)   |
    ((cfg->hour % 10)   << RTC_ALRMAR_HU_Pos)   |
    ((cfg->minute / 10) << RTC_ALRMAR_MNT_Pos)  |
    ((cfg->minute % 10) << RTC_ALRMAR_MNU_Pos)  |
    ((cfg->second / 10) << RTC_ALRMAR_ST_Pos)   |
    ((cfg->second % 10) << RTC_ALRMAR_SU_Pos);
  RTC->CR |= mask;
  rtc_lock();
}

/**
 * @brief Enable alarm at daily time
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @param[in] stamp Seconds from midnight
 */
void RTC_AlarmDaystampEnable(RTC_Alarm_e alarm, uint32_t stamp)
{
  RTC_Alarm_t cfg = RTC_DaystampToAlarm(stamp);
  RTC_AlarmEnable(alarm, &cfg);
}

/**
 * @brief Enable alarm at weekly time
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @param[in] stamp Seconds from Monday 00:00
 */
void RTC_AlarmWeekstampEnable(RTC_Alarm_e alarm, uint32_t stamp)
{
  RTC_Alarm_t cfg = RTC_WeekstampToAlarm(stamp);
  RTC_AlarmEnable(alarm, &cfg);
}

/**
 * @brief Enable alarm after interval
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @param[in] interval_sec Seconds from now
 */
void RTC_AlarmIntervalEnable(RTC_Alarm_e alarm, uint32_t interval_sec)
{
  uint32_t stamp = (RTC_Timestamp() + interval_sec) % RTC_SECONDS_IN_DAY;
  RTC_AlarmDaystampEnable(alarm, stamp);
}

/**
 * @brief Disable alarm
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 */
void RTC_AlarmDisable(RTC_Alarm_e alarm)
{
  RTC->CR &= ~rtc_alarm_mask(alarm);
}

//------------------------------------------------------------------------------------------------- WAKEUP

/**
 * @brief Enable wakeup timer
 * @param[in] sec Wakeup interval in seconds
 */
void RTC_WakeupTimerEnable(uint32_t sec)
{
  rtc_flags.wakeup_timer = false;
  rtc_unlock();
  RTC->CR &= ~RTC_CR_WUTE;
  while(!(RTC->ICSR & RTC_ICSR_WUTWF));
  RTC->CR = (RTC->CR & ~RTC_CR_WUCKSEL_1) | RTC_CR_WUCKSEL_2;
  RTC->WUTR = sec - 1;
  RTC->CR |= RTC_CR_WUTE;
  rtc_lock();
}

/**
 * @brief Disable wakeup timer
 */
void RTC_WakeupTimerDisable(void)
{
  rtc_unlock();
  RTC->CR &= ~RTC_CR_WUTE;
  rtc_lock();
}

//------------------------------------------------------------------------------------------------- ALARM-CHECK

/**
 * @brief Check if daystamp is within time window
 * @param[in] stamp_alarm Daystamp to check
 * @param[in] offset_min_sec Seconds before now
 * @param[in] offset_max_sec Seconds after now
 * @return `true` if within range, handles midnight wrap
 */
bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  int32_t daystamp = (int32_t)RTC_Daystamp();
  return rtc_check_base(daystamp, daystamp - offset_min_sec, daystamp + offset_max_sec, 
                        (int32_t)stamp_alarm, RTC_SECONDS_IN_DAY);
}

/**
 * @brief Check if weekstamp is within time window
 * @param[in] stamp_alarm Weekstamp to check
 * @param[in] offset_min_sec Seconds before now
 * @param[in] offset_max_sec Seconds after now
 * @return `true` if within range, handles week wrap
 */
bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  int32_t weekstamp = (int32_t)RTC_Weekstamp();
  return rtc_check_base(weekstamp, weekstamp - offset_min_sec, weekstamp + offset_max_sec, 
                        (int32_t)stamp_alarm, RTC_SECONDS_IN_WEEK);
}

/**
 * @brief Check if alarm is active within time window
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @param[in] offset_min_sec Seconds before now
 * @param[in] offset_max_sec Seconds after now
 * @return `true` if alarm within `[now - offset_min, now + offset_max]`
 */
bool RTC_AlarmCheck(RTC_Alarm_e alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  if(!RTC_AlarmIsEnabled(alarm)) return false;
  RTC_Alarm_t cfg = RTC_Alarm(alarm);
  if(cfg.day_mask) return RTC_CheckDaystamp(RTC_AlarmToDaystamp(&cfg), offset_min_sec, offset_max_sec);
  return RTC_CheckWeekstamp(RTC_AlarmToWeekstamp(&cfg), offset_min_sec, offset_max_sec);
}

//------------------------------------------------------------------------------------------------- EVENT

/**
 * @brief Check and clear alarm event flag
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 * @return `true` if event occurred (clears flag)
 */
bool RTC_Event(RTC_Alarm_e alarm)
{
  volatile bool *flag = (alarm == RTC_Alarm_A) ? &rtc_flags.alarm_a : &rtc_flags.alarm_b;
  if(*flag) {
    *flag = false;
    return true;
  }
  return false;
}

/**
 * @brief Check and clear wakeup timer event flag
 * @return `true` if event occurred (clears flag)
 */
bool RTC_EventWakeupTimer(void)
{
  if(rtc_flags.wakeup_timer) {
    rtc_flags.wakeup_timer = false;
    return true;
  }
  return false;
}

/**
 * @brief Force alarm event flag
 * @param[in] alarm `RTC_Alarm_A` or `RTC_Alarm_B`
 */
void RTC_Force(RTC_Alarm_e alarm)
{
  if(alarm == RTC_Alarm_A) rtc_flags.alarm_a = true;
  else rtc_flags.alarm_b = true;
}

/**
 * @brief Force wakeup timer event flag
 */
void RTC_ForceWakeupTimer(void)
{
  rtc_flags.wakeup_timer = true;
}

//------------------------------------------------------------------------------------------------- IRQ

void RTC_STAMP_IRQHandler(void)
{
  if(RTC->SR & RTC_SR_ALRAF) {
    RTC->SCR = RTC_SCR_CALRAF;
    rtc_flags.alarm_a = true;
  }
  if(RTC->SR & RTC_SR_ALRBF) {
    RTC->SCR = RTC_SCR_CALRBF;
    rtc_flags.alarm_b = true;
  }
  if(RTC->SR & RTC_SR_WUTF) {
    RTC->SCR = RTC_SCR_CWUTF;
    rtc_flags.wakeup_timer = true;
  }
  NVIC_ClearPendingIRQ(RTC_TAMP_IRQn);
}

//-------------------------------------------------------------------------------------------------
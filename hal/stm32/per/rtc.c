// hal/stm32/per/rtc.c

#include "rtc.h"

//------------------------------------------------------------------------------------------------- Compatibility Layer

#if defined(STM32G0)
  // G0: Status in ICSR, clear via SCR (write-to-clear)
  #define RTC_SR            (RTC->ICSR)
  #define RTC_INITF         RTC_ICSR_INITF
  #define RTC_INIT          RTC_ICSR_INIT
  #define RTC_ALRAWF        RTC_ICSR_ALRAWF
  #define RTC_ALRBWF        RTC_ICSR_ALRBWF
  #define RTC_WUTWF         RTC_ICSR_WUTWF
  #define RTC_ALRAF         RTC_SR_ALRAF
  #define RTC_ALRBF         RTC_SR_ALRBF
  #define RTC_WUTF          RTC_SR_WUTF
  #define RTC_HAS_SCR       1
#elif defined(STM32WB)
  // WB: Status in ISR, clear via ISR (read-modify-write)
  #define RTC_SR            (RTC->ISR)
  #define RTC_INITF         RTC_ISR_INITF
  #define RTC_INIT          RTC_ISR_INIT
  #define RTC_ALRAWF        RTC_ISR_ALRAWF
  #define RTC_ALRBWF        RTC_ISR_ALRBWF
  #define RTC_WUTWF         RTC_ISR_WUTWF
  #define RTC_ALRAF         RTC_ISR_ALRAF
  #define RTC_ALRBF         RTC_ISR_ALRBF
  #define RTC_WUTF          RTC_ISR_WUTF
  #define RTC_HAS_SCR       0
#endif

//------------------------------------------------------------------------------------------------- Constants

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

#define RTC_SSR_TO_MS(ssr)  (((PREDIV_SYNC - (ssr)) * 1000) / (PREDIV_SYNC + 1))

//------------------------------------------------------------------------------------------------- Variables

static volatile struct {
  bool alarm_a;
  bool alarm_b;
  bool wakeup_timer;
} rtc_flags;

static const uint8_t RTC_DAYS_IN_MONTH[2][12] = {
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

#if(RTC_WEEKDAYS_LONGNAMES)
  const char *RtcWeekdays[8] = { "Everyday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
#else
  const char *RtcWeekdays[8] = { "Evd", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
#endif

bool RtcReady;
bool RtcInit;

//------------------------------------------------------------------------------------------------- Static Helpers

static inline void rtc_unlock(void)
{
  RTC->WPR = RTC_WPR_KEY1;
  RTC->WPR = RTC_WPR_KEY2;
}

static inline void rtc_lock(void)
{
  RTC->WPR = RTC_WPR_LOCK;
}

static inline volatile uint32_t *rtc_alarm_reg(RTC_Alarm_t alarm)
{
  return (alarm == RTC_Alarm_A) ? &RTC->ALRMAR : &RTC->ALRMBR;
}

static inline uint32_t rtc_alarm_mask(RTC_Alarm_t alarm)
{
  return (alarm == RTC_Alarm_A) ? RTC_CR_ALRAE : RTC_CR_ALRBE;
}

static inline uint32_t rtc_alarm_wf_mask(RTC_Alarm_t alarm)
{
  return (alarm == RTC_Alarm_A) ? RTC_ALRAWF : RTC_ALRBWF;
}

static inline void rtc_clear_flag(uint32_t flag)
{
  #if RTC_HAS_SCR
    // G0: write-to-clear SCR register
    RTC->SCR = flag;
  #else
    // WB: read-modify-write ISR (need unlock)
    rtc_unlock();
    RTC->ISR &= ~flag;
    rtc_lock();
  #endif
}

static uint8_t rtc_weekday_calc(RTC_Datetime_t *datetime)
{
  uint64_t timestamp = RTC_DatetimeToUnix(datetime);
  datetime->week_day = (timestamp / RTC_SECONDS_IN_DAY + 3) % 7 + 1;
  return datetime->week_day;
}

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

static uint32_t rtc_time_register(const RTC_Datetime_t *date)
{
  return ((date->hour / 10)   << RTC_TR_HT_Pos)  |
         ((date->hour % 10)   << RTC_TR_HU_Pos)  |
         ((date->minute / 10) << RTC_TR_MNT_Pos) |
         ((date->minute % 10) << RTC_TR_MNU_Pos) |
         ((date->second / 10) << RTC_TR_ST_Pos)  |
         ((date->second % 10) << RTC_TR_SU_Pos);
}

static bool rtc_check_base(int32_t stamp, int32_t stamp_min, int32_t stamp_max,
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

//------------------------------------------------------------------------------------------------- Init

void RTC_Init(void)
{
  // Enable clocks
  #if defined(STM32G0)
    RCC->APBENR1 |= RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN;
  #elif defined(STM32WB)
    RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;
  #endif
  // Backup domain reset
  RCC->BDCR |= RCC_BDCR_BDRST;
  RCC->BDCR &= ~RCC_BDCR_BDRST;
  PWR->CR1 |= PWR_CR1_DBP;
  // Enable LSE
  RCC->BDCR |= RCC_BDCR_LSEON;
  while(!(RCC->BDCR & RCC_BDCR_LSERDY)) __DSB();
  // Select LSE and enable RTC
  RCC->BDCR |= RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN;
  // Enable interrupts
  rtc_unlock();
  RTC->CR |= RTC_CR_TSIE | RTC_CR_WUTIE | RTC_CR_ALRBIE | RTC_CR_ALRAIE;
  rtc_lock();
  // Configure NVIC
  #if defined(STM32G0)
    NVIC_ClearPendingIRQ(RTC_TAMP_IRQn);
    NVIC_EnableIRQ(RTC_TAMP_IRQn);
    NVIC_SetPriority(RTC_TAMP_IRQn, RTC_IRQ_PRIORITY);
  #elif defined(STM32WB)
    NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
    NVIC_EnableIRQ(RTC_WKUP_IRQn);
    NVIC_SetPriority(RTC_WKUP_IRQn, RTC_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    NVIC_EnableIRQ(RTC_Alarm_IRQn);
    NVIC_SetPriority(RTC_Alarm_IRQn, RTC_IRQ_PRIORITY);
  #endif
  RtcInit = true;
}

//------------------------------------------------------------------------------------------------- Convert

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
  for(uint16_t i = 1; i < date->month; i++) days += RTC_DAYS_IN_MONTH[RTC_LEAP_YEAR(year)][i - 1];
  days += date->month_day - 1;
  return (uint64_t)days * RTC_SECONDS_IN_DAY +
         date->hour * RTC_SECONDS_IN_HOUR +
         date->minute * RTC_SECONDS_IN_MIN +
         date->second;
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
  return RTC_YEAR_VALID(date->year) &&
    date->second < 60 &&
    date->hour <= 23 &&
    date->month > 0 && date->month <= 12 &&
    date->month_day > 0 && date->month_day <= 31;
}

//------------------------------------------------------------------------------------------------- Convert Alarm

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
  return alarm->hour * RTC_SECONDS_IN_HOUR +
         alarm->minute * RTC_SECONDS_IN_MIN +
         alarm->second;
}

uint32_t RTC_AlarmToWeekstamp(const RTC_AlarmCfg_t *alarm)
{
  return (alarm->day - 1) * RTC_SECONDS_IN_DAY +
         alarm->hour * RTC_SECONDS_IN_HOUR +
         alarm->minute * RTC_SECONDS_IN_MIN +
         alarm->second;
}

//------------------------------------------------------------------------------------------------- Set

void RTC_SetDatetime(RTC_Datetime_t *datetime)
{
  rtc_weekday_calc(datetime);
  rtc_unlock();
  RTC_SR |= RTC_INIT;
  while(!(RTC_SR & RTC_INITF)) __DSB();
  RTC->PRER = (PREDIV_ASYNC << RTC_PRER_PREDIV_A_Pos) | PREDIV_SYNC;
  RTC->TR = rtc_time_register(datetime);
  RTC->DR = rtc_date_register(datetime);
  RTC_SR &= ~RTC_INIT;
  rtc_lock();
  RtcReady = true;
}

void RTC_SetTimestamp(uint64_t timestamp)
{
  RTC_Datetime_t datetime = RTC_UnixToDatetime(timestamp);
  RTC_SetDatetime(&datetime);
}

void RTC_Reset(void)
{
  RTC_Datetime_t datetime = {
    .year = 0, .month = 1, .month_day = 1,
    .hour = 0, .minute = 0, .second = 0
  };
  RTC_SetDatetime(&datetime);
  RtcReady = false;
}

//------------------------------------------------------------------------------------------------- Get

RTC_Datetime_t RTC_Datetime(void)
{
  uint32_t ssr = RTC->SSR;
  uint32_t tr = RTC->TR;
  uint32_t dr = RTC->DR;
  return (RTC_Datetime_t){
    .year      = ((dr >> RTC_DR_YT_Pos) & 0x0F) * 10 + ((dr >> RTC_DR_YU_Pos) & 0x0F),
    .month     = ((dr >> RTC_DR_MT_Pos) & 0x01) * 10 + ((dr >> RTC_DR_MU_Pos) & 0x0F),
    .month_day = ((dr >> RTC_DR_DT_Pos) & 0x03) * 10 + ((dr >> RTC_DR_DU_Pos) & 0x0F),
    .week_day  = (dr >> RTC_DR_WDU_Pos) & 0x07,
    .hour      = ((tr >> RTC_TR_HT_Pos) & 0x03) * 10 + ((tr >> RTC_TR_HU_Pos) & 0x0F),
    .minute    = ((tr >> RTC_TR_MNT_Pos) & 0x07) * 10 + ((tr >> RTC_TR_MNU_Pos) & 0x0F),
    .second    = ((tr >> RTC_TR_ST_Pos) & 0x07) * 10 + ((tr >> RTC_TR_SU_Pos) & 0x0F),
    .ms        = RTC_SSR_TO_MS(ssr)
  };
}

uint64_t RTC_Timestamp(void)
{
  return RTC_DatetimeToUnix(&(RTC_Datetime_t){
    .year      = ((RTC->DR >> RTC_DR_YT_Pos) & 0x0F) * 10 + ((RTC->DR >> RTC_DR_YU_Pos) & 0x0F),
    .month     = ((RTC->DR >> RTC_DR_MT_Pos) & 0x01) * 10 + ((RTC->DR >> RTC_DR_MU_Pos) & 0x0F),
    .month_day = ((RTC->DR >> RTC_DR_DT_Pos) & 0x03) * 10 + ((RTC->DR >> RTC_DR_DU_Pos) & 0x0F),
    .hour      = ((RTC->TR >> RTC_TR_HT_Pos) & 0x03) * 10 + ((RTC->TR >> RTC_TR_HU_Pos) & 0x0F),
    .minute    = ((RTC->TR >> RTC_TR_MNT_Pos) & 0x07) * 10 + ((RTC->TR >> RTC_TR_MNU_Pos) & 0x0F),
    .second    = ((RTC->TR >> RTC_TR_ST_Pos) & 0x07) * 10 + ((RTC->TR >> RTC_TR_SU_Pos) & 0x0F)
  });
}

uint64_t RTC_TimestampMs(void)
{
  RTC_Datetime_t dt = RTC_Datetime();
  return RTC_DatetimeToUnix(&dt) * 1000 + dt.ms;
}

uint32_t RTC_Daystamp(void)
{
  uint32_t tr = RTC->TR;
  uint32_t hour = ((tr >> RTC_TR_HT_Pos) & 0x03) * 10 + ((tr >> RTC_TR_HU_Pos) & 0x0F);
  uint32_t min = ((tr >> RTC_TR_MNT_Pos) & 0x07) * 10 + ((tr >> RTC_TR_MNU_Pos) & 0x0F);
  uint32_t sec = ((tr >> RTC_TR_ST_Pos) & 0x07) * 10 + ((tr >> RTC_TR_SU_Pos) & 0x0F);
  return hour * RTC_SECONDS_IN_HOUR + min * RTC_SECONDS_IN_MIN + sec;
}

uint32_t RTC_Weekstamp(void)
{
  uint32_t tr = RTC->TR;
  uint32_t dr = RTC->DR;
  uint32_t wday = (dr >> RTC_DR_WDU_Pos) & 0x07;
  uint32_t hour = ((tr >> RTC_TR_HT_Pos) & 0x03) * 10 + ((tr >> RTC_TR_HU_Pos) & 0x0F);
  uint32_t min = ((tr >> RTC_TR_MNT_Pos) & 0x07) * 10 + ((tr >> RTC_TR_MNU_Pos) & 0x0F);
  uint32_t sec = ((tr >> RTC_TR_ST_Pos) & 0x07) * 10 + ((tr >> RTC_TR_SU_Pos) & 0x0F);
  return (wday - 1) * RTC_SECONDS_IN_DAY + hour * RTC_SECONDS_IN_HOUR + min * RTC_SECONDS_IN_MIN + sec;
}

//------------------------------------------------------------------------------------------------- Alarm Get

RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm)
{
  uint32_t reg = *rtc_alarm_reg(alarm);
  return (RTC_AlarmCfg_t){
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

uint32_t RTC_AlarmDaystamp(RTC_Alarm_t alarm)
{
  RTC_AlarmCfg_t cfg = RTC_Alarm(alarm);
  return RTC_AlarmToDaystamp(&cfg);
}

//------------------------------------------------------------------------------------------------- Alarm Enable

bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm)
{
  return !!(RTC->CR & rtc_alarm_mask(alarm));
}

void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg)
{
  uint32_t mask = rtc_alarm_mask(alarm);
  rtc_unlock();
  RTC->CR &= ~mask;
  while(!(RTC_SR & rtc_alarm_wf_mask(alarm))) __DSB();
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

void RTC_AlarmDisable(RTC_Alarm_t alarm)
{
  RTC->CR &= ~rtc_alarm_mask(alarm);
}

//------------------------------------------------------------------------------------------------- Wakeup Timer

void RTC_WakeupTimerEnable(uint32_t sec)
{
  rtc_flags.wakeup_timer = false;
  rtc_unlock();
  RTC->CR &= ~RTC_CR_WUTE;
  while(!(RTC_SR & RTC_WUTWF));
  RTC->CR = (RTC->CR & ~RTC_CR_WUCKSEL_1) | RTC_CR_WUCKSEL_2;
  RTC->WUTR = sec - 1;
  RTC->CR |= RTC_CR_WUTE;
  rtc_lock();
}

void RTC_WakeupTimerDisable(void)
{
  rtc_unlock();
  RTC->CR &= ~RTC_CR_WUTE;
  rtc_lock();
}

//------------------------------------------------------------------------------------------------- Alarm Check

bool RTC_CheckDaystamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  int32_t daystamp = (int32_t)RTC_Daystamp();
  return rtc_check_base(daystamp, daystamp - offset_min_sec, daystamp + offset_max_sec,
                        (int32_t)stamp_alarm, RTC_SECONDS_IN_DAY);
}

bool RTC_CheckWeekstamp(uint32_t stamp_alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  int32_t weekstamp = (int32_t)RTC_Weekstamp();
  return rtc_check_base(weekstamp, weekstamp - offset_min_sec, weekstamp + offset_max_sec,
                        (int32_t)stamp_alarm, RTC_SECONDS_IN_WEEK);
}

bool RTC_AlarmCheck(RTC_Alarm_t alarm, uint32_t offset_min_sec, uint32_t offset_max_sec)
{
  if(!RTC_AlarmIsEnabled(alarm)) return false;
  RTC_AlarmCfg_t cfg = RTC_Alarm(alarm);
  if(cfg.day_mask) return RTC_CheckDaystamp(RTC_AlarmToDaystamp(&cfg), offset_min_sec, offset_max_sec);
  return RTC_CheckWeekstamp(RTC_AlarmToWeekstamp(&cfg), offset_min_sec, offset_max_sec);
}

//------------------------------------------------------------------------------------------------- Event

bool RTC_Event(RTC_Alarm_t alarm)
{
  volatile bool *flag = (alarm == RTC_Alarm_A) ? &rtc_flags.alarm_a : &rtc_flags.alarm_b;
  if(*flag) { *flag = false; return true; }
  return false;
}

bool RTC_EventWakeupTimer(void)
{
  if(rtc_flags.wakeup_timer) { rtc_flags.wakeup_timer = false; return true; }
  return false;
}

void RTC_Force(RTC_Alarm_t alarm)
{
  if(alarm == RTC_Alarm_A) rtc_flags.alarm_a = true;
  else rtc_flags.alarm_b = true;
}

void RTC_ForceWakeupTimer(void)
{
  rtc_flags.wakeup_timer = true;
}

//------------------------------------------------------------------------------------------------- IRQ Handlers

#if defined(STM32G0)
// G0: Single handler for all RTC interrupts
void RTC_TAMP_IRQHandler(void)
{
  if(RTC->SR & RTC_SR_ALRAF) {
    rtc_clear_flag(RTC_SCR_CALRAF);
    rtc_flags.alarm_a = true;
  }
  if(RTC->SR & RTC_SR_ALRBF) {
    rtc_clear_flag(RTC_SCR_CALRBF);
    rtc_flags.alarm_b = true;
  }
  if(RTC->SR & RTC_SR_WUTF) {
    rtc_clear_flag(RTC_SCR_CWUTF);
    rtc_flags.wakeup_timer = true;
  }
  NVIC_ClearPendingIRQ(RTC_TAMP_IRQn);
}

#elif defined(STM32WB)
// WB: Separate handlers for alarms and wakeup
void RTC_Alarm_IRQHandler(void)
{
  if(RTC->ISR & RTC_ISR_ALRAF) {
    rtc_clear_flag(RTC_ISR_ALRAF);
    rtc_flags.alarm_a = true;
  }
  if(RTC->ISR & RTC_ISR_ALRBF) {
    rtc_clear_flag(RTC_ISR_ALRBF);
    rtc_flags.alarm_b = true;
  }
  EXTI->PR1 = EXTI_PR1_PIF17; // Clear EXTI line 17 (RTC Alarm)
  NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
}

void RTC_WKUP_IRQHandler(void)
{
  if(RTC->ISR & RTC_ISR_WUTF) {
    rtc_clear_flag(RTC_ISR_WUTF);
    rtc_flags.wakeup_timer = true;
  }
  EXTI->PR1 = EXTI_PR1_PIF20; // Clear EXTI line 20 (RTC Wakeup)
  NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
}
#endif

//-------------------------------------------------------------------------------------------------
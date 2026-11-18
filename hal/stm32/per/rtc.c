// hal/stm32/per/rtc.c

#include "rtc.h"

#include "pwr.h"

//-------------------------------------------------------------------------------- Family registers

#if defined(STM32G0)
  // G0: status in `ICSR`, clear via `SCR` (write-to-clear)
  #define RTC_SR        (RTC->ICSR)
  #define RTC_INITF     RTC_ICSR_INITF
  #define RTC_INITS     RTC_ICSR_INITS
  #define RTC_INIT      RTC_ICSR_INIT
  #define RTC_ALRAWF    RTC_ICSR_ALRAWF
  #define RTC_ALRBWF    RTC_ICSR_ALRBWF
  #define RTC_WUTWF     RTC_ICSR_WUTWF
  #define RTC_RSF       RTC_ICSR_RSF
  #define RTC_HAS_SCR   1
#elif defined(STM32WB)
  // WB: status in `ISR`, clear via `ISR` (read-modify-write)
  #define RTC_SR        (RTC->ISR)
  #define RTC_INITF     RTC_ISR_INITF
  #define RTC_INITS     RTC_ISR_INITS
  #define RTC_INIT      RTC_ISR_INIT
  #define RTC_ALRAWF    RTC_ISR_ALRAWF
  #define RTC_ALRBWF    RTC_ISR_ALRBWF
  #define RTC_WUTWF     RTC_ISR_WUTWF
  #define RTC_RSF       RTC_ISR_RSF
  #define RTC_HAS_SCR   0
#endif

//--------------------------------------------------------------------------------------- Constants

#define RTC_PREDIV_SYNC       255
#define RTC_PREDIV_ASYNC      127

#define RTC_WPR_KEY1  0xCA
#define RTC_WPR_KEY2  0x53
#define RTC_WPR_LOCK  0xFF

#define RTC_SSR_TO_MS(ssr)  (((RTC_PREDIV_SYNC - (ssr)) * 1000) / (RTC_PREDIV_SYNC + 1))

//--------------------------------------------------------------------------------------- Variables

static volatile bool rtc_alarm_a_flag;
static volatile bool rtc_alarm_b_flag;
static volatile bool rtc_wakeup_flag;

//---------------------------------------------------------------------------------------- Internal

// `DBP` guards every RTC register, not only the backup ones, and anything writing a backup
// register may drop it again; taken here, every RTC write stands on its own
static inline void rtc_unlock(void)
{
  PWR->CR1 |= PWR_CR1_DBP;
  while(!(PWR->CR1 & PWR_CR1_DBP));
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

// Clear a status flag: `RTC_SCR_*` on G0, `RTC_ISR_*` on WB
static inline void rtc_clear_flag(uint32_t flag)
{
  #if RTC_HAS_SCR
  RTC->SCR = flag;
  #else
  rtc_unlock();
  RTC->ISR &= ~flag;
  rtc_lock();
  #endif
}

// BCD time register into hour, minute and second
static inline void rtc_decode_tr(uint32_t tr, RTC_Datetime_t *dt)
{
  dt->hour = ((tr >> RTC_TR_HT_Pos) & 0x03) * 10 + ((tr >> RTC_TR_HU_Pos) & 0x0F);
  dt->minute = ((tr >> RTC_TR_MNT_Pos) & 0x07) * 10 + ((tr >> RTC_TR_MNU_Pos) & 0x0F);
  dt->second = ((tr >> RTC_TR_ST_Pos) & 0x07) * 10 + ((tr >> RTC_TR_SU_Pos) & 0x0F);
}

// BCD date register into year, month, day and weekday
static inline void rtc_decode_dr(uint32_t dr, RTC_Datetime_t *dt)
{
  dt->year = ((dr >> RTC_DR_YT_Pos) & 0x0F) * 10 + ((dr >> RTC_DR_YU_Pos) & 0x0F);
  dt->month = ((dr >> RTC_DR_MT_Pos) & 0x01) * 10 + ((dr >> RTC_DR_MU_Pos) & 0x0F);
  dt->month_day = ((dr >> RTC_DR_DT_Pos) & 0x03) * 10 + ((dr >> RTC_DR_DU_Pos) & 0x0F);
  dt->week_day = (dr >> RTC_DR_WDU_Pos) & 0x07;
}

// BCD date register
static uint32_t rtc_date_register(const RTC_Datetime_t *date)
{
  return ((date->year / 10) << RTC_DR_YT_Pos)
    | ((date->year % 10) << RTC_DR_YU_Pos)
    | ((date->week_day) << RTC_DR_WDU_Pos)
    | ((date->month / 10) << RTC_DR_MT_Pos)
    | ((date->month % 10) << RTC_DR_MU_Pos)
    | ((date->month_day / 10) << RTC_DR_DT_Pos)
    | ((date->month_day % 10) << RTC_DR_DU_Pos);
}

// BCD time register
static uint32_t rtc_time_register(const RTC_Datetime_t *date)
{
  return ((date->hour / 10) << RTC_TR_HT_Pos)
    | ((date->hour % 10) << RTC_TR_HU_Pos)
    | ((date->minute / 10) << RTC_TR_MNT_Pos)
    | ((date->minute % 10) << RTC_TR_MNU_Pos)
    | ((date->second / 10) << RTC_TR_ST_Pos)
    | ((date->second % 10) << RTC_TR_SU_Pos);
}

//-------------------------------------------------------------------------------------------- Init

// Bounded wait on an RTCCLK-driven flag: a stopped clock must not park the caller
static status_t rtc_wait(volatile uint32_t *reg, uint32_t mask, uint32_t retry)
{
  while(retry--) {
    if((*reg & mask) == mask) return OK;
    __NOP();
  }
  return ERR;
}

// Running: clocked from `LSE`, enabled, and `LSCO` off
static bool rtc_running(void)
{
  uint32_t want = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN;
  uint32_t mask = want | RCC_BDCR_RTCSEL | RCC_BDCR_LSCOEN;
  return ((RCC->BDCR & mask) == want) && (RTC_SR & RTC_INITS);
}

// The crystal load decides the drive level, so it is measured rather than configured:
// the weakest level from `RTC_LSE_DRIVE` up that oscillates wins.
// `LSEDRV` takes a write only with the oscillator stopped, so every step restarts it.
// A cold start pays for the search once, later resets find the crystal already running.
status_t RTC_StartLSE(void)
{
  if(RCC->BDCR & RCC_BDCR_LSERDY) return OK;
  for(uint32_t drive = RTC_LSE_DRIVE; drive <= 3; drive++) {
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    for(uint32_t i = RTC_LSE_RETRY; i && (RCC->BDCR & RCC_BDCR_LSERDY); i--) __NOP();
    RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_LSEDRV) | (drive << RCC_BDCR_LSEDRV_Pos);
    RCC->BDCR |= RCC_BDCR_LSEON;
    if(!rtc_wait(&RCC->BDCR, RCC_BDCR_LSERDY, RTC_LSE_RETRY)) return OK;
  }
  return ERR;
}

uint8_t RTC_LseDrive(void)
{
  return (uint8_t)((RCC->BDCR & RCC_BDCR_LSEDRV) >> RCC_BDCR_LSEDRV_Pos);
}

status_t RTC_Init(void)
{
  #if defined(STM32G0)
  RCC->APBENR1 |= RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN;
  #elif defined(STM32WB)
  RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;
  #endif
  PWR->CR1 |= PWR_CR1_DBP;
  // A valid RTC survives resets, only a stopped or corrupt domain is rebuilt
  if(!rtc_running()) {
    BKP_DomainReset();
    if(RTC_StartLSE()) {
      RtcInit = false;
      RtcReady = false;
      return ERR;
    }
    RCC->BDCR |= RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN;
  }
  rtc_unlock();
  RTC->CR |= RTC_CR_TSIE | RTC_CR_WUTIE | RTC_CR_ALRBIE | RTC_CR_ALRAIE;
  rtc_lock();
  #if defined(STM32G0)
  NVIC_ClearPendingIRQ(RTC_TAMP_IRQn);
  NVIC_EnableIRQ(RTC_TAMP_IRQn);
  NVIC_SetPriority(RTC_TAMP_IRQn, RTC_IRQ_PRIORITY);
  #elif defined(STM32WB)
  // Alarms reach the core through EXTI line 17 and the wakeup timer through line 19.
  // Both lines stay masked after reset, so they are opened here on the rising edge.
  EXTI->RTSR1 |= EXTI_RTSR1_RT17 | EXTI_RTSR1_RT19;
  EXTI->IMR1 |= EXTI_IMR1_IM17 | EXTI_IMR1_IM19;
  NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
  NVIC_EnableIRQ(RTC_WKUP_IRQn);
  NVIC_SetPriority(RTC_WKUP_IRQn, RTC_IRQ_PRIORITY);
  NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
  NVIC_EnableIRQ(RTC_Alarm_IRQn);
  NVIC_SetPriority(RTC_Alarm_IRQn, RTC_IRQ_PRIORITY);
  #endif
  RtcInit = true;
  return OK;
}

//--------------------------------------------------------------------------------------------- Set

status_t RTC_SetDatetime(RTC_Datetime_t *datetime)
{
  datetime->week_day = RTC_UnixToDatetime(RTC_DatetimeToUnix(datetime)).week_day;
  rtc_unlock();
  RTC_SR |= RTC_INIT;
  // `INITF` needs a running RTCCLK; without it the calendar write would be dropped
  if(rtc_wait(&RTC_SR, RTC_INITF, RTC_SYNC_RETRY)) {
    RTC_SR &= ~RTC_INIT;
    rtc_lock();
    return ERR;
  }
  // The prescaler takes two accesses, synchronous part first
  RTC->PRER = RTC_PREDIV_SYNC;
  RTC->PRER |= RTC_PREDIV_ASYNC << RTC_PRER_PREDIV_A_Pos;
  RTC->TR = rtc_time_register(datetime);
  RTC->DR = rtc_date_register(datetime);
  RTC_SR &= ~RTC_INIT;
  // Shadow registers reload from the new calendar once `RSF` returns.
  // A read before that still shows the old time, while the calendar holds the new one.
  RTC_SR &= ~RTC_RSF;
  (void)rtc_wait(&RTC_SR, RTC_RSF, RTC_SYNC_RETRY);
  rtc_lock();
  RtcReady = true;
  return OK;
}

//--------------------------------------------------------------------------------------------- Get

RTC_Datetime_t RTC_Datetime(void)
{
  // `SSR`, `TR`, `DR` in this order: the shadow registers unlock on the `DR` read
  uint32_t ssr = RTC->SSR;
  uint32_t tr = RTC->TR;
  uint32_t dr = RTC->DR;
  RTC_Datetime_t dt;
  rtc_decode_tr(tr, &dt);
  rtc_decode_dr(dr, &dt);
  dt.ms = RTC_SSR_TO_MS(ssr);
  return dt;
}

//--------------------------------------------------------------------------------------- Alarm Get

RTC_AlarmCfg_t RTC_Alarm(RTC_Alarm_t alarm)
{
  uint32_t reg = *rtc_alarm_reg(alarm);
  return (RTC_AlarmCfg_t){
    .week        = !!(reg & RTC_ALRMAR_WDSEL),
    .day_mask    = !!(reg & RTC_ALRMAR_MSK4),
    .hour_mask   = !!(reg & RTC_ALRMAR_MSK3),
    .minute_mask = !!(reg & RTC_ALRMAR_MSK2),
    .second_mask = !!(reg & RTC_ALRMAR_MSK1),
    .day    = ((reg >> RTC_ALRMAR_DT_Pos)  & 0x03) * 10 + ((reg >> RTC_ALRMAR_DU_Pos)  & 0x0F),
    .hour   = ((reg >> RTC_ALRMAR_HT_Pos)  & 0x03) * 10 + ((reg >> RTC_ALRMAR_HU_Pos)  & 0x0F),
    .minute = ((reg >> RTC_ALRMAR_MNT_Pos) & 0x07) * 10 + ((reg >> RTC_ALRMAR_MNU_Pos) & 0x0F),
    .second = ((reg >> RTC_ALRMAR_ST_Pos)  & 0x07) * 10 + ((reg >> RTC_ALRMAR_SU_Pos)  & 0x0F),
  };
}

//----------------------------------------------------------------------------------- Alarm Control

bool RTC_AlarmIsEnabled(RTC_Alarm_t alarm)
{
  return !!(RTC->CR & rtc_alarm_mask(alarm));
}

void RTC_AlarmEnable(RTC_Alarm_t alarm, const RTC_AlarmCfg_t *cfg)
{
  uint32_t mask = rtc_alarm_mask(alarm);
  rtc_unlock();
  RTC->CR &= ~mask;
  // The alarm register is write-protected until its own write flag stands.
  if(rtc_wait(&RTC_SR, rtc_alarm_wf_mask(alarm), RTC_SYNC_RETRY)) {
    rtc_lock();
    return;
  }
  *rtc_alarm_reg(alarm) = (cfg->week << RTC_ALRMAR_WDSEL_Pos)
    | (cfg->day_mask << RTC_ALRMAR_MSK4_Pos)
    | (cfg->hour_mask << RTC_ALRMAR_MSK3_Pos)
    | (cfg->minute_mask << RTC_ALRMAR_MSK2_Pos)
    | (cfg->second_mask << RTC_ALRMAR_MSK1_Pos)
    | ((cfg->day / 10) << RTC_ALRMAR_DT_Pos)
    | ((cfg->day % 10) << RTC_ALRMAR_DU_Pos)
    | ((cfg->hour / 10) << RTC_ALRMAR_HT_Pos)
    | ((cfg->hour % 10) << RTC_ALRMAR_HU_Pos)
    | ((cfg->minute / 10) << RTC_ALRMAR_MNT_Pos)
    | ((cfg->minute % 10) << RTC_ALRMAR_MNU_Pos)
    | ((cfg->second / 10) << RTC_ALRMAR_ST_Pos)
    | ((cfg->second % 10) << RTC_ALRMAR_SU_Pos);
  RTC->CR |= mask;
  rtc_lock();
}

void RTC_AlarmDisable(RTC_Alarm_t alarm)
{
  rtc_unlock();
  RTC->CR &= ~rtc_alarm_mask(alarm);
  rtc_lock();
}

//------------------------------------------------------------------------------------ Wakeup Timer

void RTC_WakeupTimerEnable(uint32_t sec)
{
  rtc_wakeup_flag = false;
  rtc_unlock();
  RTC->CR &= ~RTC_CR_WUTE;
  // `WUTR` and `WUCKSEL` accept writes only while the timer reports itself idle
  if(rtc_wait(&RTC_SR, RTC_WUTWF, RTC_SYNC_RETRY)) {
    rtc_lock();
    return;
  }
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

//------------------------------------------------------------------------------------------- Event

bool RTC_Event(RTC_Alarm_t alarm)
{
  volatile bool *flag = (alarm == RTC_Alarm_A) ? &rtc_alarm_a_flag : &rtc_alarm_b_flag;
  if(*flag) { *flag = false; return true; }
  return false;
}

bool RTC_EventWakeupTimer(void)
{
  if(rtc_wakeup_flag) { rtc_wakeup_flag = false; return true; }
  return false;
}

void RTC_Force(RTC_Alarm_t alarm)
{
  if(alarm == RTC_Alarm_A) rtc_alarm_a_flag = true;
  else rtc_alarm_b_flag = true;
}

void RTC_ForceWakeupTimer(void)
{
  rtc_wakeup_flag = true;
}

//------------------------------------------------------------------------------------ IRQ Handlers

#if defined(STM32G0)
// G0: single handler for all RTC interrupts
void RTC_TAMP_IRQHandler(void)
{
  if(RTC->SR & RTC_SR_ALRAF) {
    rtc_clear_flag(RTC_SCR_CALRAF);
    rtc_alarm_a_flag = true;
  }
  if(RTC->SR & RTC_SR_ALRBF) {
    rtc_clear_flag(RTC_SCR_CALRBF);
    rtc_alarm_b_flag = true;
  }
  if(RTC->SR & RTC_SR_WUTF) {
    rtc_clear_flag(RTC_SCR_CWUTF);
    rtc_wakeup_flag = true;
  }
  NVIC_ClearPendingIRQ(RTC_TAMP_IRQn);
}

#elif defined(STM32WB)
// WB: separate handlers for alarms and wakeup
void RTC_Alarm_IRQHandler(void)
{
  if(RTC->ISR & RTC_ISR_ALRAF) {
    rtc_clear_flag(RTC_ISR_ALRAF);
    rtc_alarm_a_flag = true;
  }
  if(RTC->ISR & RTC_ISR_ALRBF) {
    rtc_clear_flag(RTC_ISR_ALRBF);
    rtc_alarm_b_flag = true;
  }
  EXTI->PR1 = EXTI_PR1_PIF17; // clear EXTI line 17 (RTC Alarm)
  NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
}

void RTC_WKUP_IRQHandler(void)
{
  if(RTC->ISR & RTC_ISR_WUTF) {
    rtc_clear_flag(RTC_ISR_WUTF);
    rtc_wakeup_flag = true;
  }
  EXTI->PR1 = EXTI_PR1_PIF19; // clear EXTI line 19 (RTC Wakeup)
  NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
}
#endif

//-------------------------------------------------------------------------------------------------

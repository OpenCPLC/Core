// hal/stm32/per/pwmi.c

#include "pwmi.h"

// Sentinel packed into `_count`: the round was just reconfigured and the next trigger
// still carries the previous channel's capture, so nothing may be accumulated yet
#define PWMI_CONFIG_NOT_READY 0xFFFF

//-------------------------------------------------------------------------------------------------

static void PWMI_Run(PWMI_t *pwmi);

static void PWMI_Interrupt(PWMI_t *pwmi)
{
  if(pwmi->reg->SR & TIM_SR_TIF) PWMI_Run(pwmi);
}

// Longest period the counter can still express, which is the point where waiting any
// longer tells us nothing new. Taken from `ARR` so it follows the configured span
static float PWMI_GetTimeoutMax_ms(PWMI_t *pwmi)
{
  return (float)pwmi->prescaler * (float)pwmi->reg->ARR * 1000.0f / (float)SystemCoreClock;
}

static void PWMI_Begin(PWMI_t *pwmi)
{
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    pwmi->frequency[chan] = NaN;
    pwmi->duty[chan] = NaN;
  }
  RCC_EnableTIM(pwmi->reg);
  if(!pwmi->prescaler) pwmi->prescaler = 1;
  pwmi->reg->PSC = pwmi->prescaler - 1;
  pwmi->reg->ARR = TIM_Is32bit(pwmi->reg) ? 0xFFFFFFFF : 0xFFFF;
  // CH3 and CH4 are unusable without their external trigger,
  // and a trigger without its channel must never reach `EXTI_Off`:
  // it was never initialized
  if(pwmi->channel[TIM_CH3] && !pwmi->trig3) pwmi->channel[TIM_CH3] = 0;
  if(pwmi->channel[TIM_CH4] && !pwmi->trig4) pwmi->channel[TIM_CH4] = 0;
  if(!pwmi->channel[TIM_CH3]) pwmi->trig3 = NULL;
  if(!pwmi->channel[TIM_CH4]) pwmi->trig4 = NULL;
  if(pwmi->channel[TIM_CH3] && pwmi->trig3) {
    pwmi->trig3->port = TIM_CHx_MAP[pwmi->channel[TIM_CH3]].port;
    pwmi->trig3->pin = TIM_CHx_MAP[pwmi->channel[TIM_CH3]].pin;
    pwmi->trig3->rise_detect = true;
    pwmi->trig3->fall_detect = false;
    pwmi->trig3->irq_priority = pwmi->irq_priority;
    pwmi->trig3->RiseHandler = (void (*)(void *))&PWMI_Run;
    pwmi->trig3->rise_arg = pwmi;
    EXTI_Init(pwmi->trig3);
  }
  if(pwmi->channel[TIM_CH4] && pwmi->trig4) {
    pwmi->trig4->port = TIM_CHx_MAP[pwmi->channel[TIM_CH4]].port;
    pwmi->trig4->pin = TIM_CHx_MAP[pwmi->channel[TIM_CH4]].pin;
    pwmi->trig4->rise_detect = true;
    pwmi->trig4->fall_detect = false;
    pwmi->trig4->irq_priority = pwmi->irq_priority;
    pwmi->trig4->RiseHandler = (void (*)(void *))&PWMI_Run;
    pwmi->trig4->rise_arg = pwmi;
    EXTI_Init(pwmi->trig4);
  }
  // Plain push-pull mapping like `pwm.c`: the front end defines the idle level,
  // and an internal pull-up would fight the opto-isolated inputs of the boards
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    if(pwmi->channel[chan]) GPIO_InitAlternate(&TIM_CHx_MAP[pwmi->channel[chan]], false);
  }
  IRQ_EnableTIM(pwmi->reg, pwmi->irq_priority, (void (*)(void *))&PWMI_Interrupt, pwmi);
  #if(PWMI_AUTO_OVERSAMPLING)
  if(!pwmi->threshold) {
    if(TIM_Is32bit(pwmi->reg)) pwmi->threshold = 0xFFFFFF;
    else pwmi->threshold = 0xFFFF;
  }
  #else
  if(!pwmi->oversampling) pwmi->oversampling = 1;
  #endif
  if(!pwmi->timeout_ms) pwmi->timeout_ms = PWMI_GetTimeoutMax_ms(pwmi);
}

// The software state goes first: a trigger latched during the previous pass would fire
// the handler the moment `TIE` is unmasked, and it must not land mid-reset
static void PWMI_Reset(PWMI_t *pwmi)
{
  pwmi->_count = PWMI_CONFIG_NOT_READY;
  pwmi->_inc = TIM_CH1;
  pwmi->_chan = TIM_CH1;
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    pwmi->_reload[chan] = 0;
    pwmi->_value[chan] = 0;
  }
  pwmi->reg->SR = ~TIM_SR_TIF;
  pwmi->reg->CR1 |= TIM_CR1_CEN;
  pwmi->reg->DIER |= TIM_DIER_TIE;
}

static void PWMI_Run(PWMI_t *pwmi)
{
  pwmi->_timeout_tick = tick_keep(pwmi->timeout_ms);
  // `_inc` already points at the next candidate,
  // so the samples arriving now belong to `_chan`.
  // Both the accumulator slot and the register roles follow the active channel
  TIM_Channel_t chan = pwmi->_chan;
  // The first trigger after a reconfiguration still carries the previous channel's
  // capture, and the sentinel marks the round where nothing has been configured yet
  if(pwmi->_count && pwmi->_count != PWMI_CONFIG_NOT_READY) {
    // The channel that drives the trigger captures the period,
    // its partner captures the pulse,
    // so the two registers swap roles between the odd and the even channel
    switch(chan) {
      case TIM_CH1:
        pwmi->_reload[chan] += pwmi->reg->CCR1; pwmi->_value[chan] += pwmi->reg->CCR2; break;
      case TIM_CH2:
        pwmi->_reload[chan] += pwmi->reg->CCR2; pwmi->_value[chan] += pwmi->reg->CCR1; break;
      case TIM_CH3:
        pwmi->_reload[chan] += pwmi->reg->CCR3; pwmi->_value[chan] += pwmi->reg->CCR4; break;
      case TIM_CH4:
        pwmi->_reload[chan] += pwmi->reg->CCR4; pwmi->_value[chan] += pwmi->reg->CCR3; break;
      default: break;
    }
  }
  pwmi->reg->CNT = 0;
  #if(PWMI_AUTO_OVERSAMPLING)
  if(pwmi->_count != PWMI_CONFIG_NOT_READY && pwmi->_reload[chan] < pwmi->threshold)
    pwmi->_count++;
  #else
  if(pwmi->_count < pwmi->oversampling) pwmi->_count++;
  #endif
  else {
    #if(PWMI_AUTO_OVERSAMPLING)
    // The sentinel is not a sample count and would scale the reported frequency by 0xFFFF
    if(pwmi->_count != PWMI_CONFIG_NOT_READY) pwmi->_oversampling[chan] = pwmi->_count;
    #endif
    pwmi->_count = 0;
    while(pwmi->_inc <= TIM_CH4 && !pwmi->channel[pwmi->_inc]) pwmi->_inc++;
    switch(pwmi->_inc) {
      case TIM_CH1:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_1 |
          (pwmi->filter << TIM_CCMR1_IC1F_Pos) | (pwmi->filter << TIM_CCMR1_IC2F_Pos);
        pwmi->reg->CCMR2 = 0;
        pwmi->reg->CCER = TIM_CCER_CC2P | TIM_CCER_CC1E | TIM_CCER_CC2E;
        pwmi->reg->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 | TIM_SMCR_SMS_2;
        break;
      case TIM_CH2:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = TIM_CCMR1_CC1S_1 | TIM_CCMR1_CC2S_0 |
          (pwmi->filter << TIM_CCMR1_IC1F_Pos) | (pwmi->filter << TIM_CCMR1_IC2F_Pos);
        pwmi->reg->CCMR2 = 0;
        pwmi->reg->CCER = TIM_CCER_CC1P | TIM_CCER_CC1E | TIM_CCER_CC2E;
        pwmi->reg->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_1 | TIM_SMCR_SMS_2;
        break;
      case TIM_CH3:
        pwmi->reg->CCER = 0;
        // CH3 and CH4 cannot drive the slave trigger,
        // so the counter is restarted from the EXTI handler instead.
        // The reset from TI1 or TI2 has to go:
        // a signal still present on the CH1 or CH2 pin keeps zeroing the counter here
        pwmi->reg->SMCR = 0;
        pwmi->reg->CCMR1 = 0;
        pwmi->reg->CCMR2 = TIM_CCMR2_CC3S_0 | TIM_CCMR2_CC4S_1 |
          (pwmi->filter << TIM_CCMR2_IC3F_Pos) | (pwmi->filter << TIM_CCMR2_IC4F_Pos);
        pwmi->reg->CCER = TIM_CCER_CC4P | TIM_CCER_CC3E | TIM_CCER_CC4E;
        if(pwmi->trig3) EXTI_On(pwmi->trig3);
        break;
      case TIM_CH4:
        if(pwmi->trig3) EXTI_Off(pwmi->trig3);
        pwmi->reg->CCER = 0;
        pwmi->reg->SMCR = 0;
        pwmi->reg->CCMR1 = 0;
        pwmi->reg->CCMR2 = TIM_CCMR2_CC3S_1 | TIM_CCMR2_CC4S_0 |
          (pwmi->filter << TIM_CCMR2_IC3F_Pos) | (pwmi->filter << TIM_CCMR2_IC4F_Pos);
        pwmi->reg->CCER = TIM_CCER_CC3P | TIM_CCER_CC3E | TIM_CCER_CC4E;
        if(pwmi->trig4) EXTI_On(pwmi->trig4);
        break;
      default:
        pwmi->reg->DIER &= ~TIM_DIER_TIE;
        pwmi->reg->CR1 &= ~TIM_CR1_CEN;
        if(pwmi->trig3) EXTI_Off(pwmi->trig3);
        if(pwmi->trig4) EXTI_Off(pwmi->trig4);
        pwmi->_inc = TIM_CH1;
        return;
    }
    pwmi->_chan = pwmi->_inc;
    pwmi->_inc++;
  }
  // Writing zero clears a flag and writing one leaves it alone, so assigning the mask
  // cannot drop an event the hardware raised while this handler was running
  pwmi->reg->SR = ~TIM_SR_TIF;
}

// A silent channel never raises a trigger, so the sequence is pushed on by hand:
// its partial sum is dropped and the sentinel steers the next run into the advance path
static void PWMI_Skip(PWMI_t *pwmi)
{
  TIM_Channel_t chan = pwmi->_chan;
  pwmi->_reload[chan] = 0;
  pwmi->_value[chan] = 0;
  pwmi->_count = PWMI_CONFIG_NOT_READY;
  PWMI_Run(pwmi);
}

// The trigger interrupt stays enabled for the whole pass
// and is switched off only once the last configured channel has been measured
static bool PWMI_IsRunning(PWMI_t *pwmi)
{
  return (pwmi->reg->DIER & TIM_DIER_TIE) != 0;
}

static float PWMI_GetFrequency(PWMI_t *pwmi, TIM_Channel_t chan)
{
  #if(PWMI_AUTO_OVERSAMPLING)
  uint16_t ovs = pwmi->_oversampling[chan];
  #else
  uint16_t ovs = pwmi->oversampling;
  #endif
  if(!pwmi->_reload[chan]) {
    pwmi->frequency[chan] = NaN;
    return NaN;
  }
  // `_reload` holds `ovs` full periods measured in prescaled timer ticks
  pwmi->frequency[chan] =
    (float)SystemCoreClock * ovs / pwmi->prescaler / pwmi->_reload[chan];
  return pwmi->frequency[chan];
}

static float PWMI_GetDuty(PWMI_t *pwmi, TIM_Channel_t chan)
{
  if(!pwmi->_reload[chan]) {
    pwmi->duty[chan] = NaN;
    return NaN;
  }
  pwmi->duty[chan] = 100.0f * pwmi->_value[chan] / pwmi->_reload[chan];
  return pwmi->duty[chan];
}

//-------------------------------------------------------------------------------------------------

void PWMI_Init(PWMI_t *pwmi)
{
  PWMI_Begin(pwmi);
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  PWMI_Reset(pwmi);
  PWMI_Run(pwmi);
  __set_PRIMASK(primask);
  pwmi->_init = true;
}

bool PWMI_Loop(PWMI_t *pwmi)
{
  if(!pwmi->_init) return false;
  // `PWMI_Run` is owned by the interrupts for the whole pass, so the thread-side steps
  // mask them to keep a trigger from landing mid-step. The publish loop stays outside:
  // the pass is over by then and nothing mutates the state it reads
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  // Only a sequence still in progress can stall: once it ends there is nothing to skip
  if(PWMI_IsRunning(pwmi) && tick_over(&pwmi->_timeout_tick)) PWMI_Skip(pwmi);
  bool done = !PWMI_IsRunning(pwmi);
  __set_PRIMASK(primask);
  if(!done) return false;
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    PWMI_GetFrequency(pwmi, chan);
    PWMI_GetDuty(pwmi, chan);
  }
  primask = __get_PRIMASK();
  __disable_irq();
  PWMI_Reset(pwmi);
  PWMI_Run(pwmi);
  __set_PRIMASK(primask);
  return true;
}

//-------------------------------------------------------------------------------------------------

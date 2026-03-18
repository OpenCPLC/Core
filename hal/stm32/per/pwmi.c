// hal/stm32/pwmi.c

#include "pwmi.h"

#define PWMI_CONFIG_NOT_READY 0xFFFF

//-------------------------------------------------------------------------------------------------

static uint16_t PWMI_Run(PWMI_t *pwmi);

static void PWMI_Interrupt(PWMI_t *pwmi)
{
  if(pwmi->reg->SR & TIM_SR_TIF) PWMI_Run(pwmi);
}

static float PWMI_GetTimeoutMax_ms(PWMI_t *pwmi)
{
  uint32_t auto_reload = TIM_Is32bit(pwmi->reg) ? 0xFFFFFFFF : 0xFFFF;
  return (float)pwmi->prescaler * (1 << pwmi->capture_prescaler) * auto_reload * 1000.0f / (float)SystemCoreClock;
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
  pwmi->reg->ARR = 0xFFFF;
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
    pwmi->trig3->RiseHandler = (void (*)(void *))&PWMI_Run;
    pwmi->trig3->rise_arg = pwmi;
    EXTI_Init(pwmi->trig4);
  }
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    if(pwmi->channel[chan]) GPIO_InitAlternate(&TIM_CHx_MAP[pwmi->channel[chan]], true);
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

static void PWMI_Reset(PWMI_t *pwmi)
{
  pwmi->reg->CR1 |= TIM_CR1_CEN;
  pwmi->reg->DIER |= TIM_DIER_TIE;
  pwmi->_count = PWMI_CONFIG_NOT_READY;
  pwmi->_inc = TIM_CH1;
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    pwmi->_reload[chan] = 0;
    pwmi->_value[chan] = 0;
  }
}

static uint16_t PWMI_Run(PWMI_t *pwmi)
{
  pwmi->_timeout_tick = tick_keep(pwmi->timeout_ms);
  TIM_Channel_t chan = pwmi->_inc;
  if(pwmi->_count) {
    switch(chan) {
      case TIM_CH1: pwmi->_reload[TIM_CH1] += pwmi->reg->CCR1; pwmi->_value[TIM_CH1] += pwmi->reg->CCR2; break;
      case TIM_CH2: pwmi->_reload[TIM_CH2] += pwmi->reg->CCR2; pwmi->_value[TIM_CH2] += pwmi->reg->CCR1; break;
      case TIM_CH3: pwmi->_reload[TIM_CH3] += pwmi->reg->CCR3; pwmi->_value[TIM_CH3] += pwmi->reg->CCR4; break;
      case TIM_CH4: pwmi->_reload[TIM_CH4] += pwmi->reg->CCR4; pwmi->_value[TIM_CH4] += pwmi->reg->CCR3; break;
      default: break;
    }
  }
  pwmi->reg->CNT = 0;
  #if(PWMI_AUTO_OVERSAMPLING)
  if(pwmi->_count != PWMI_CONFIG_NOT_READY && pwmi->_reload[chan] < pwmi->threshold) pwmi->_count++;
  #else
  if(pwmi->_count < pwmi->oversampling) pwmi->_count++;
  #endif
  else {
    #if(PWMI_AUTO_OVERSAMPLING)
    pwmi->_oversampling[chan] = pwmi->_count;
    #endif
    pwmi->_count = 0;
    while(pwmi->_inc <= TIM_CH4 && !pwmi->channel[pwmi->_inc]) pwmi->_inc++;
    switch(pwmi->_inc) {
      case TIM_CH1:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_1 |
          (pwmi->capture_prescaler << TIM_CCMR1_IC1PSC_Pos) | (pwmi->capture_prescaler << TIM_CCMR1_IC2PSC_Pos) |
          (pwmi->filter << TIM_CCMR1_IC1F_Pos) | (pwmi->filter << TIM_CCMR1_IC2F_Pos);
        pwmi->reg->CCMR2 = 0;
        pwmi->reg->CCER = TIM_CCER_CC2P | TIM_CCER_CC1E | TIM_CCER_CC2E;
        pwmi->reg->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 | TIM_SMCR_SMS_2;
        break;
      case TIM_CH2:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = TIM_CCMR1_CC1S_1 | TIM_CCMR1_CC2S_0 |
          (pwmi->capture_prescaler << TIM_CCMR1_IC1PSC_Pos) | (pwmi->capture_prescaler << TIM_CCMR1_IC2PSC_Pos) |
          (pwmi->filter << TIM_CCMR1_IC1F_Pos) | (pwmi->filter << TIM_CCMR1_IC2F_Pos);
        pwmi->reg->CCMR2 = 0;
        pwmi->reg->CCER = TIM_CCER_CC1P | TIM_CCER_CC1E | TIM_CCER_CC2E;
        pwmi->reg->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_1 | TIM_SMCR_SMS_2;
        break;
      case TIM_CH3:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = 0;
        pwmi->reg->CCMR2 = TIM_CCMR2_CC3S_0 | TIM_CCMR2_CC4S_1 |
          (pwmi->capture_prescaler << TIM_CCMR2_IC3PSC_Pos) | (pwmi->capture_prescaler << TIM_CCMR2_IC4PSC_Pos) |
          (pwmi->filter << TIM_CCMR2_IC3F_Pos) | (pwmi->filter << TIM_CCMR2_IC4F_Pos);
        pwmi->reg->CCER = TIM_CCER_CC4P | TIM_CCER_CC3E | TIM_CCER_CC4E;
        if(pwmi->trig3) EXTI_On(pwmi->trig3);
        break;
      case TIM_CH4:
        if(pwmi->trig3) EXTI_Off(pwmi->trig3);
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = 0;
        pwmi->reg->CCMR2 = TIM_CCMR2_CC3S_1 | TIM_CCMR2_CC4S_0 |
          (pwmi->capture_prescaler << TIM_CCMR2_IC3PSC_Pos) | (pwmi->capture_prescaler << TIM_CCMR2_IC4PSC_Pos) |
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
        return 0;
    }
    pwmi->_inc++;
  }
  pwmi->reg->SR &= ~TIM_SR_TIF;
  return pwmi->_count;
}

static void PWMI_Skip(PWMI_t *pwmi)
{
  TIM_Channel_t chan = pwmi->_inc - 1;
  while(!PWMI_Run(pwmi));
  PWMI_Run(pwmi);
  pwmi->_reload[chan] = 0;
  pwmi->_value[chan] = 0;
}

static bool PWMI_IsInterrupt(PWMI_t *pwmi)
{
  if(pwmi->reg->DIER & TIM_DIER_TIE) return true;
  return false;
}

static float PWMI_GetFrequency(PWMI_t *pwmi, TIM_Channel_t chan)
{
  #if(PWMI_AUTO_OVERSAMPLING)
  uint16_t ovs = pwmi->_oversampling[chan];
  #else
  uint16_t ovs = pwmi->oversampling;
  #endif
  pwmi->frequency[chan] = pwmi->_reload[chan] ?
    (float)SystemCoreClock * ovs / pwmi->prescaler / (1 << pwmi->capture_prescaler) / pwmi->_reload[chan] : NaN;
  return pwmi->frequency[chan];
}

static float PWMI_GetDuty(PWMI_t *pwmi, TIM_Channel_t chan)
{
  pwmi->duty[chan] = pwmi->_reload[chan] ? 100.0 * pwmi->_value[chan] / pwmi->_reload[chan] : NaN;
  return pwmi->duty[chan];
}

//-------------------------------------------------------------------------------------------------

void PWMI_Init(PWMI_t *pwmi)
{
  PWMI_Begin(pwmi);
  PWMI_Reset(pwmi);
  PWMI_Run(pwmi);
  pwmi->_init = true;
}

bool PWMI_Loop(PWMI_t *pwmi)
{
  if(!pwmi->_init) return false;
  if(tick_over(&pwmi->_timeout_tick)) PWMI_Skip(pwmi);
  if(!PWMI_IsInterrupt(pwmi)) {
    for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
      PWMI_GetFrequency(pwmi, chan);
      PWMI_GetDuty(pwmi, chan);
    }
    PWMI_Reset(pwmi);
    PWMI_Run(pwmi);
    return true;
  }
  return false;
}

//-------------------------------------------------------------------------------------------------
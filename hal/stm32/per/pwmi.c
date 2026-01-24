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
  uint32_t auto_reload = (pwmi->reg == TIM2) ? 0xFFFFFFFF : 0xFFFF;
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
    pwmi->trig3->irq_priority = pwmi->int_prioryty;
    pwmi->trig3->RiseHandler = (void (*)(void *))&PWMI_Run;
    pwmi->trig3->rise_arg = pwmi;
    EXTI_Init(pwmi->trig3);
  }
  if(pwmi->channel[TIM_CH4] && pwmi->trig4) {
    pwmi->trig4->port = TIM_CHx_MAP[pwmi->channel[TIM_CH4]].port;
    pwmi->trig4->pin = TIM_CHx_MAP[pwmi->channel[TIM_CH4]].pin;
    pwmi->trig4->rise_detect = true;
    pwmi->trig4->fall_detect = false;
    pwmi->trig4->irq_priority = pwmi->int_prioryty;
    pwmi->trig3->RiseHandler = (void (*)(void *))&PWMI_Run;
    pwmi->trig3->rise_arg = pwmi;
    EXTI_Init(pwmi->trig4);
  }
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    if(pwmi->channel[chan]) GPIO_InitAlternate(&TIM_CHx_MAP[pwmi->channel[chan]], true);
  }
  IRQ_EnableTIM(pwmi->reg, pwmi->int_prioryty, (void (*)(void *))&PWMI_Interrupt, pwmi);
  #if(PWMI_AUTO_OVERSAMPLING)
    if(!pwmi->threshold) {
      if(pwmi->reg == TIM2) pwmi->threshold = 0xFFFFFF;
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
  pwmi->count = PWMI_CONFIG_NOT_READY;
  pwmi->inc = TIM_CH1;
  for(uint8_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    pwmi->reload[chan] = 0;
    pwmi->value[chan] = 0;
  }
}

static uint16_t PWMI_Run(PWMI_t *pwmi)
{
  pwmi->timeout_tick = tick_keep(pwmi->timeout_ms);
  TIM_Channel_t chan = pwmi->inc;
  if(pwmi->count) {
    switch(chan) {
      case TIM_CH1: pwmi->reload[TIM_CH1] += pwmi->reg->CCR1; pwmi->value[TIM_CH1] += pwmi->reg->CCR2; break;
      case TIM_CH2: pwmi->reload[TIM_CH2] += pwmi->reg->CCR2; pwmi->value[TIM_CH2] += pwmi->reg->CCR1; break;
      case TIM_CH3: pwmi->reload[TIM_CH3] += pwmi->reg->CCR3; pwmi->value[TIM_CH3] += pwmi->reg->CCR4; break;
      case TIM_CH4: pwmi->reload[TIM_CH4] += pwmi->reg->CCR4; pwmi->value[TIM_CH4] += pwmi->reg->CCR3; break;
      default: break;
    }
  }
  pwmi->reg->CNT = 0;
  #if(PWMI_AUTO_OVERSAMPLING)
    if(pwmi->count != PWMI_CONFIG_NOT_READY && pwmi->reload[chan] < pwmi->threshold) pwmi->count++;
  #else
    if(pwmi->count < pwmi->oversampling) pwmi->count++;
  #endif
  else {
    #if(PWMI_AUTO_OVERSAMPLING)
      pwmi->oversampling[chan] = pwmi->count;
    #endif
    pwmi->count = 0;
    while(pwmi->inc <= TIM_CH4 && !pwmi->channel[pwmi->inc]) pwmi->inc++;
    switch(pwmi->inc) {
      case TIM_CH1:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_1 |
          (pwmi->capture_prescaler  << TIM_CCMR1_IC1PSC_Pos) | (pwmi->capture_prescaler  << TIM_CCMR1_IC2PSC_Pos) |
          (pwmi->filter  << TIM_CCMR1_IC1F_Pos) | (pwmi->filter  << TIM_CCMR1_IC2F_Pos);
        pwmi->reg->CCMR2 = 0;
        pwmi->reg->CCER = TIM_CCER_CC2P | TIM_CCER_CC1E | TIM_CCER_CC2E;
        pwmi->reg->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 | TIM_SMCR_SMS_2;
        break;
      case TIM_CH2:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = TIM_CCMR1_CC1S_1 | TIM_CCMR1_CC2S_0 |
          (pwmi->capture_prescaler  << TIM_CCMR1_IC1PSC_Pos) | (pwmi->capture_prescaler  << TIM_CCMR1_IC2PSC_Pos) |
          (pwmi->filter  << TIM_CCMR1_IC1F_Pos) | (pwmi->filter  << TIM_CCMR1_IC2F_Pos);
        pwmi->reg->CCMR2 = 0;
        pwmi->reg->CCER = TIM_CCER_CC1P | TIM_CCER_CC1E | TIM_CCER_CC2E;
        pwmi->reg->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_1 | TIM_SMCR_SMS_2;
        break;
      case TIM_CH3:
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = 0;
        pwmi->reg->CCMR2 = TIM_CCMR2_CC3S_0 | TIM_CCMR2_CC4S_1 |
          (pwmi->capture_prescaler  << TIM_CCMR2_IC3PSC_Pos) | (pwmi->capture_prescaler  << TIM_CCMR2_IC4PSC_Pos) |
          (pwmi->filter  << TIM_CCMR2_IC3F_Pos) | (pwmi->filter  << TIM_CCMR2_IC4F_Pos);
        pwmi->reg->CCER = TIM_CCER_CC4P | TIM_CCER_CC3E | TIM_CCER_CC4E;
        if(pwmi->trig3) EXTI_On(pwmi->trig3);
        break;
      case TIM_CH4:
        if(pwmi->trig3) EXTI_Off(pwmi->trig3);
        pwmi->reg->CCER = 0;
        pwmi->reg->CCMR1 = 0;
        pwmi->reg->CCMR2 = TIM_CCMR2_CC3S_1 | TIM_CCMR2_CC4S_0 |
          (pwmi->capture_prescaler  << TIM_CCMR2_IC3PSC_Pos) | (pwmi->capture_prescaler  << TIM_CCMR2_IC4PSC_Pos) |
          (pwmi->filter  << TIM_CCMR2_IC3F_Pos) | (pwmi->filter  << TIM_CCMR2_IC4F_Pos);
        pwmi->reg->CCER = TIM_CCER_CC3P | TIM_CCER_CC3E | TIM_CCER_CC4E;
        if(pwmi->trig4) EXTI_On(pwmi->trig4);
        break;
      default:
        pwmi->reg->DIER &= ~TIM_DIER_TIE;
        pwmi->reg->CR1 &= ~TIM_CR1_CEN;
        if(pwmi->trig3) EXTI_Off(pwmi->trig3);
        if(pwmi->trig4) EXTI_Off(pwmi->trig4);
        pwmi->inc = TIM_CH1;
        return 0;
    }
    pwmi->inc++;
  }
  pwmi->reg->SR &= ~TIM_SR_TIF;
  return pwmi->count;
}

static void PWMI_Skip(PWMI_t *pwmi)
{
  TIM_Channel_t chan = pwmi->inc - 1;
  while(!PWMI_Run(pwmi));
  PWMI_Run(pwmi);
  pwmi->reload[chan] = 0;
  pwmi->value[chan] = 0;
}

static bool PWMI_IsInterrupt(PWMI_t *pwmi)
{
  if(pwmi->reg->DIER & TIM_DIER_TIE) return true;
  return false;
}

static float PWMI_GetFrequency(PWMI_t *pwmi, TIM_Channel_t chan)
{
  #if(PWMI_AUTO_OVERSAMPLING)
    uint16_t ovs = pwmi->oversampling[chan];
  #else
    uint16_t ovs = pwmi->oversampling;
  #endif
  pwmi->frequency[chan] = pwmi->reload[chan] ?
     (float)SystemCoreClock * ovs / pwmi->prescaler / (1 << pwmi->capture_prescaler) / pwmi->reload[chan] : NaN;
  return pwmi->frequency[chan];
}

static float PWMI_GetDuty(PWMI_t *pwmi, TIM_Channel_t chan)
{
  pwmi->duty[chan] = pwmi->reload[chan] ? 100.0 * pwmi->value[chan] / pwmi->reload[chan] : NaN;
  return pwmi->duty[chan];
}

//-------------------------------------------------------------------------------------------------

void PWMI_Init(PWMI_t *pwmi)
{
  PWMI_Begin(pwmi);
  PWMI_Reset(pwmi);
  PWMI_Run(pwmi);
  pwmi->init_flag = true;
}

bool PWMI_Loop(PWMI_t *pwmi)
{
  if(!pwmi->init_flag) return false;
  if(tick_over(&pwmi->timeout_tick)) PWMI_Skip(pwmi);
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

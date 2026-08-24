// hal/stm32/per/pwm.c

#include "pwm.h"

//-------------------------------------------------------------------------------------------------

static bool PWM_HasBDTR(TIM_TypeDef *reg)
{
  if(reg == TIM1) return true;
  #ifdef TIM8
    if(reg == TIM8) return true;
  #endif
  #ifdef TIM15
    if(reg == TIM15) return true;
  #endif
  #ifdef TIM16
    if(reg == TIM16) return true;
  #endif
  #ifdef TIM17
    if(reg == TIM17) return true;
  #endif
  #ifdef TIM20
    if(reg == TIM20) return true;
  #endif
  return false;
}

//-------------------------------------------------------------------------------------------------

static void PWM_Interrupt(PWM_t *pwm)
{
  if(pwm->reg->SR & TIM_SR_UIF) {
    pwm->reg->SR = ~TIM_SR_UIF;
    if(pwm->UpdateCallback) pwm->UpdateCallback(pwm->update_arg);
  }
}

void PWM_InterruptEnable(PWM_t *pwm)
{
  pwm->reg->DIER |= TIM_DIER_UIE;
}

void PWM_InterruptDisable(PWM_t *pwm)
{
  pwm->reg->DIER &= ~TIM_DIER_UIE;
}

//-------------------------------------------------------------------------------------------------

void PWM_SetPrescaler(PWM_t *pwm, uint32_t prescaler)
{
  if(!prescaler) prescaler = 1;
  pwm->prescaler = prescaler;
  pwm->reg->PSC = pwm->prescaler - 1;
}

void PWM_SetAutoreload(PWM_t *pwm, uint32_t auto_reload)
{
  if(!auto_reload) return;
  pwm->auto_reload = auto_reload;
  pwm->reg->ARR = pwm->auto_reload;
}

void PWM_SetValue(PWM_t *pwm, TIM_Channel_t channel, uint32_t value)
{
  switch(channel) {
    case TIM_CH1: case TIM_CH1N: pwm->value[TIM_CH1] = value; pwm->reg->CCR1 = value; break;
    case TIM_CH2: case TIM_CH2N: pwm->value[TIM_CH2] = value; pwm->reg->CCR2 = value; break;
    case TIM_CH3: case TIM_CH3N: pwm->value[TIM_CH3] = value; pwm->reg->CCR3 = value; break;
    case TIM_CH4: case TIM_CH4N: pwm->value[TIM_CH4] = value; pwm->reg->CCR4 = value; break;
  }
}

uint32_t PWM_GetValue(PWM_t *pwm, TIM_Channel_t channel)
{
  switch(channel) {
    case TIM_CH1: case TIM_CH1N: return pwm->value[TIM_CH1];
    case TIM_CH2: case TIM_CH2N: return pwm->value[TIM_CH2];
    case TIM_CH3: case TIM_CH3N: return pwm->value[TIM_CH3];
    case TIM_CH4: case TIM_CH4N: return pwm->value[TIM_CH4];
  }
  return 0;
}

void PWM_SetDeadtime(PWM_t *pwm, uint16_t deadtime)
{
  if(!PWM_HasBDTR(pwm->reg)) return;
  // 1008 is the top of the coarsest `DTG` range; anything above cannot be encoded
  if(deadtime > 1008) deadtime = 1008;
  uint8_t dtg, temp;
  if(deadtime < 128) {
    dtg = deadtime;
    pwm->deadtime = deadtime;
  }
  else if(deadtime < 256) {
    temp = ((deadtime - 128) / 2);
    dtg = (0b10 << 6) | temp;
    pwm->deadtime = (uint16_t)temp * 2 + 128;
  }
  else if(deadtime < 512) {
    temp = ((deadtime - 256) / 8);
    dtg = (0b110 << 5) | temp;
    pwm->deadtime = (uint16_t)temp * 8 + 256;
  }
  else {
    temp = ((deadtime - 512) / 16);
    dtg = (0b111 << 5) | temp;
    pwm->deadtime = (uint16_t)temp * 16 + 512;
  }
  pwm->reg->BDTR = (pwm->reg->BDTR & ~TIM_BDTR_DTG_Msk) | dtg;
}

void PWM_SetAlign(PWM_t *pwm, PWM_Align_t align)
{
  if(align == pwm->align) return;
  // `CMS` is writable only with the counter stopped
  pwm->reg->CR1 &= ~TIM_CR1_CEN;
  pwm->reg->CNT = 0;
  pwm->align = align;
  pwm->reg->CR1 = (pwm->reg->CR1 & ~TIM_CR1_CMS_Msk)
    | ((uint32_t)align << TIM_CR1_CMS_Pos);
  pwm->reg->CR1 |= TIM_CR1_CEN;
}

static void PWM_ChannelInit(PWM_t *pwm, TIM_Channel_t channel)
{
  bool invert_pos = pwm->invert[channel];
  bool invert_neg = pwm->invert[channel + 4];
  switch(channel) {
    case TIM_CH1: pwm->reg->CCMR1 = (pwm->reg->CCMR1 & ~(TIM_CCMR1_OC1M_Msk|TIM_CCMR1_OC1PE))
      | (TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1); break;
    case TIM_CH2: pwm->reg->CCMR1 = (pwm->reg->CCMR1 & ~(TIM_CCMR1_OC2M_Msk|TIM_CCMR1_OC2PE))
      | (TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1); break;
    case TIM_CH3: pwm->reg->CCMR2 = (pwm->reg->CCMR2 & ~(TIM_CCMR2_OC3M_Msk|TIM_CCMR2_OC3PE))
      | (TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1); break;
    case TIM_CH4: pwm->reg->CCMR2 = (pwm->reg->CCMR2 & ~(TIM_CCMR2_OC4M_Msk|TIM_CCMR2_OC4PE))
      | (TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1); break;
    default: break;
  }
  PWM_SetValue(pwm, channel, pwm->value[channel]);
  uint32_t sh = 4u * channel;
  uint32_t set = ((invert_pos ? TIM_CCER_CC1P : 0) | TIM_CCER_CC1E) << sh;
  uint32_t mask = (TIM_CCER_CC1E | TIM_CCER_CC1P | TIM_CCER_CC1NE | TIM_CCER_CC1NP) << sh;
  bool has_neg = (pwm->channel[channel + 4] != 0);
  if(has_neg) set |= (TIM_CCER_CC1NE | (invert_neg ? TIM_CCER_CC1NP : 0)) << sh;
  pwm->_ccer_mask |= (TIM_CCER_CC1E << sh) | (has_neg ? (TIM_CCER_CC1NE << sh) : 0);
  pwm->reg->CCER = (pwm->reg->CCER & ~mask) | set;
}

static void PWM_ChannelsInit(PWM_t *pwm)
{
  pwm->_ccer_mask = 0;
  for(uint8_t i = 0; i < 4; i++) {
    bool init = false;
    if(pwm->channel[i]) {
      GPIO_InitAlternate(&TIM_CHx_MAP[pwm->channel[i]], false);
      init = true;
    }
    if(pwm->channel[i + 4]) {
      GPIO_InitAlternate(&TIM_CHx_MAP[pwm->channel[i + 4]], false);
      init = true;
    }
    if(init) PWM_ChannelInit(pwm, i);
  }
}

void PWM_OutputEnable(PWM_t *pwm, bool enable)
{
  if(PWM_HasBDTR(pwm->reg)) {
    if(enable) pwm->reg->BDTR |= TIM_BDTR_MOE;
    else pwm->reg->BDTR &= ~TIM_BDTR_MOE;
  }
  else {
    if(enable) pwm->reg->CCER |= pwm->_ccer_mask;
    else pwm->reg->CCER &= ~pwm->_ccer_mask;
  }
}

void PWM_Init(PWM_t *pwm)
{
  RCC_EnableTIM(pwm->reg);
  pwm->reg->CR1 &= ~TIM_CR1_CEN;
  pwm->reg->CCER = 0;
  pwm->reg->CCMR1 = 0;
  pwm->reg->CCMR2 = 0;
  PWM_ChannelsInit(pwm);
  PWM_SetPrescaler(pwm, pwm->prescaler);
  pwm->reg->ARR = pwm->auto_reload;
  pwm->reg->CR1 = TIM_CR1_ARPE | ((uint32_t)pwm->align << TIM_CR1_CMS_Pos);
  PWM_SetDeadtime(pwm, pwm->deadtime);
  // DMA or Interrupt
  pwm->reg->DIER &= ~(TIM_DIER_UIE | TIM_DIER_UDE);
  if(pwm->dma_trig) pwm->reg->DIER |= TIM_DIER_UDE;
  if(pwm->UpdateCallback) {
    IRQ_EnableTIM(pwm->reg, pwm->irq_priority, (void (*)(void *))PWM_Interrupt, pwm);
    pwm->reg->DIER |= TIM_DIER_UIE;
  }
  if(PWM_HasBDTR(pwm->reg)) pwm->reg->BDTR |= TIM_BDTR_MOE;
  // `EGR` reads back as zero, so events are fired with a plain write
  pwm->reg->EGR = TIM_EGR_UG;
  pwm->reg->CR1 |= TIM_CR1_CEN;
}

//-------------------------------------------------------------------------------------------------

float PWM_GetFrequency(const PWM_t *pwm)
{
  return (float)SystemCoreClock / pwm->prescaler / pwm->auto_reload /
    pwm_align_div(pwm->align);
}

float PWM_Frequency(PWM_t *pwm, float frequency)
{
  if(frequency <= 0.0f) return PWM_GetFrequency(pwm);
  uint32_t reload_prev = pwm->auto_reload;
  float ticks = (float)SystemCoreClock / frequency / pwm_align_div(pwm->align);
  uint32_t prescaler = 1;
  uint32_t auto_reload = (uint32_t)ticks;
  while(auto_reload > 0xFFFF && prescaler < 0xFFFF) {
    auto_reload = (uint32_t)(ticks / ++prescaler);
  }
  if(auto_reload > 0xFFFF) auto_reload = 0xFFFF; // below the achievable range
  PWM_SetPrescaler(pwm, prescaler);
  PWM_SetAutoreload(pwm, auto_reload);
  // Duty is compare over reload, so the reload ratio alone keeps it unchanged
  for(TIM_Channel_t chan = TIM_CH1; chan <= TIM_CH4; chan++) {
    if(pwm->value[chan] && reload_prev) {
      PWM_SetValue(pwm, chan, (uint32_t)(
        ((uint64_t)pwm->value[chan] * auto_reload + reload_prev / 2) / reload_prev));
    }
  }
  return PWM_GetFrequency(pwm);
}

void PWM_Trigger(PWM_t *pwm, TIM_Channel_t channel, uint16_t compare, bool irq)
{
  if(!IS_TIM_TRGO2_INSTANCE(pwm->reg)) return;
  // A complementary output carries its base channel's reference, only `CH1..CH4` trigger.
  // Their enum values sit past `CH4` and would index the registers below into `BDTR`.
  if(channel > TIM_CH4) return;
  // `CCR1..CCR4` are consecutive registers, the channel indexes them directly
  (&pwm->reg->CCR1)[channel] = compare;
  // PWM mode 2 on the trigger channel: `OCxREF` active while the counter sits
  // above the compare, one rising edge per period on the way up
  volatile uint32_t *ccmr = channel < TIM_CH3 ? &pwm->reg->CCMR1 : &pwm->reg->CCMR2;
  uint8_t pos = (channel & 1) ? 12 : 4;
  *ccmr = (*ccmr & ~(0x7u << pos)) | (0x7u << pos);
  // Route the reference to `TRGO2`, where the ADC external trigger listens
  pwm->reg->CR2 = (pwm->reg->CR2 & ~TIM_CR2_MMS2_Msk)
    | ((0x4u + channel) << TIM_CR2_MMS2_Pos);
  if(irq) pwm->reg->DIER |= TIM_DIER_CC1IE << channel;
}

//-------------------------------------------------------------------------------------------------

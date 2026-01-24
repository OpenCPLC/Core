#include "pwm.h"

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
    case TIM_CH1: case TIM_CH1N: return pwm->value[TIM_CH1]; break;
    case TIM_CH2: case TIM_CH2N: return pwm->value[TIM_CH2]; break;
    case TIM_CH3: case TIM_CH3N: return pwm->value[TIM_CH3]; break;
    case TIM_CH4: case TIM_CH4N: return pwm->value[TIM_CH4]; break;
  }
  return 0;
}

void PWM_SetDeadtime(PWM_t *pwm, uint16_t deadtime)
{
  if(deadtime >= 1024) deadtime = 1024;
  uint8_t dtg, temp;
  if(deadtime < 128) {
    dtg = deadtime;
    pwm->deadtime = deadtime;
  }
  else if(deadtime < 256) {
    temp = ((deadtime - 128) / 2);
    dtg = (0b10 << 6) | temp; // 2 * DTG + 128
    pwm->deadtime = (uint16_t)temp * 2 + 128;
  }
  else if(deadtime < 512) {
    temp = ((deadtime - 256) / 8);
    dtg = (0b110 << 5) | temp; // 8 * DTG + 256
    pwm->deadtime = (uint16_t)temp * 8 + 256;
  }
  else {
    temp = ((deadtime - 512) / 16);
    dtg = (0b111 << 5) | temp; // 16 * DTG + 512
    pwm->deadtime = (uint16_t)temp * 16 + 512;
  }
  pwm->reg->BDTR = (pwm->reg->BDTR & ~TIM_BDTR_DTG_Msk) | dtg;
}

void PWM_CenterAlign(PWM_t *pwm, bool enable)
{
  if(enable == pwm->center_aligned) return;
  pwm->reg->CR1 &= ~TIM_CR1_CEN; // OFF
  pwm->reg->CNT = 0;
  pwm->center_aligned = enable;
  uint8_t center_aligned = enable ? 0x03 << TIM_CR1_CMS_Pos : 0;
  pwm->reg->CR1 = (pwm->reg->CR1 & ~TIM_CR1_CMS_Msk) | center_aligned;
  pwm->reg->CR1 |= TIM_CR1_CEN; // ON
}

static void PWM_ChannelInit(PWM_t *pwm, TIM_Channel_t channel)
{
  bool invert_pos = pwm->invert[channel];
  bool invert_neg = pwm->invert[channel + 4];
  switch (channel) {
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
  pwm->ccer_mask |= (TIM_CCER_CC1E << sh) | (has_neg ? (TIM_CCER_CC1NE << sh) : 0);
  pwm->reg->CCER  = (pwm->reg->CCER & ~mask) | set;
}

static void PWM_ChannelsInit(PWM_t *pwm)
{
  pwm->ccer_mask = 0;
  for(uint8_t i = 0; i < 4; i++) {
    bool init = false;
    if(pwm->channel[i]) { GPIO_InitAlternate(&TIM_CHx_MAP[pwm->channel[i]], false); init = true; }
    if(pwm->channel[i + 4]) { GPIO_InitAlternate(&TIM_CHx_MAP[pwm->channel[i + 4]], false); init = true; }
    if(init) PWM_ChannelInit(pwm, i);
  }
}

void PWM_OutputEnable(PWM_t *pwm, bool enable)
{
  if(pwm->reg == TIM1) {
    if(enable) { pwm->reg->BDTR |= TIM_BDTR_MOE; }
    else { pwm->reg->BDTR &= ~TIM_BDTR_MOE; }
  }
  else {
    if(enable) { pwm->reg->CCER |=  pwm->ccer_mask; }
    else { pwm->reg->CCER &= ~pwm->ccer_mask; }
  }
}

void PWM_Init(PWM_t *pwm)
{
  RCC_EnableTIM(pwm->reg);
  pwm->reg->EGR &= ~TIM_EGR_UG;
  pwm->reg->CR1 &= ~TIM_CR1_CEN;
  pwm->reg->CCER = 0;
  pwm->reg->CCMR1 = 0;
  pwm->reg->CCMR2 = 0;
  PWM_ChannelsInit(pwm);
  PWM_SetPrescaler(pwm, pwm->prescaler);
  pwm->reg->ARR = pwm->auto_reload;
  uint8_t center_aligned = pwm->center_aligned ? 0x03 << TIM_CR1_CMS_Pos : 0;
  pwm->reg->CR1 = TIM_CR1_ARPE | center_aligned;
  PWM_SetDeadtime(pwm, pwm->deadtime);
  pwm->reg->DIER = (pwm->reg->DIER & ~TIM_DIER_UIE) | (pwm->dma_trig << TIM_DIER_UDE_Pos);
  pwm->reg->BDTR |= TIM_BDTR_MOE;
  pwm->reg->EGR |= TIM_EGR_UG;
  pwm->reg->CR1 |= TIM_CR1_CEN;
}

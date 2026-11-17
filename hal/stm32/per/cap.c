// hal/stm32/per/cap.c

#include "cap.h"

//---------------------------------------------------------------------------------------- Internal

// Reading the capture clears its flag, and an overcapture is one more edge, not an error
static void CAP_ISR(void *arg)
{
  CAP_t *cap = arg;
  uint32_t flag = TIM_SR_CC1IF << cap->channel;
  if(!(cap->reg->SR & flag)) return;
  (void)(&cap->reg->CCR1)[cap->channel];
  cap->reg->SR = ~(flag | (TIM_SR_CC1OF << cap->channel));
  if(cap->Callback) cap->Callback(cap->callback_arg);
}

//--------------------------------------------------------------------------------------------- API

void CAP_Init(CAP_t *cap)
{
  const GPIO_Map_t *map = &TIM_CHx_MAP[cap->pin];
  cap->_line.port = map->port;
  cap->_line.pin = map->pin;
  GPIO_InitAlternate(map, false);
  RCC_EnableTIM(cap->reg);
  // `CKD` at `0`: the filter samples the undivided timer clock
  cap->reg->CR1 = 0;
  cap->reg->PSC = 0;
  cap->reg->ARR = TIM_Is32bit(cap->reg) ? 0xFFFFFFFFu : 0xFFFFu;
  // The channel takes its mode with its capture off, two channels share a `CCMR` register
  uint32_t shift = 8 * (cap->channel % 2);
  volatile uint32_t *ccmr = cap->channel < TIM_CH3 ? &cap->reg->CCMR1 : &cap->reg->CCMR2;
  cap->reg->CCER &= ~(0xFu << (4 * cap->channel));
  *ccmr = (*ccmr & ~(0xFFu << shift))
    | ((TIM_CCMR1_CC1S_0 | ((uint32_t)cap->filter << TIM_CCMR1_IC1F_Pos)) << shift);
  cap->reg->CCER |= (TIM_CCER_CC1E | (cap->falling ? TIM_CCER_CC1P : 0)) << (4 * cap->channel);
  cap->reg->EGR = TIM_EGR_UG;
  cap->reg->SR = 0;
  cap->reg->DIER = TIM_DIER_CC1IE << cap->channel;
  IRQ_EnableTIM(cap->reg, cap->irq_priority, CAP_ISR, cap);
  cap->reg->CR1 |= TIM_CR1_CEN;
}

bool CAP_Level(CAP_t *cap)
{
  return GPIO_In(&cap->_line);
}

// hal/stm32/per/tim.c

#include "tim.h"

//------------------------------------------------------------------------------------------------- TIM Interface

inline void TIM_Enable(TIM_t *tim)
{
  tim->reg->CR1 |= TIM_CR1_CEN;
  tim->reg->SR &= ~TIM_SR_UIF;
  tim->enable = true;
}

inline void TIM_Disable(TIM_t *tim)
{
  tim->reg->CR1 &= ~TIM_CR1_CEN;
  tim->_event_cnt = 0;
  tim->enable = false;
}

inline void TIM_InterruptEnable(TIM_t *tim)
{
  tim->reg->DIER |= TIM_DIER_UIE;
  tim->enable_interrupt = true;
}

inline void TIM_InterruptDisable(TIM_t *tim)
{
  tim->reg->DIER &= ~TIM_DIER_UIE;
  tim->_event_cnt = 0;
  tim->enable_interrupt = false;
}

inline bool TIM_IsDisable(TIM_t *tim)
{
  return !(tim->reg->CR1 & TIM_CR1_CEN_Msk);
}

inline bool TIM_IsEnable(TIM_t *tim)
{
  return tim->reg->CR1 & TIM_CR1_CEN_Msk;
}

inline void TIM_ResetValue(TIM_t *tim)
{
  tim->reg->EGR = TIM_EGR_UG;
  tim->reg->SR = 0;
  tim->_event_cnt = 0;
}

inline uint32_t TIM_GetValue(TIM_t *tim)
{
  if(TIM_Is32bit(tim->reg)) return tim->reg->CNT;
  else return tim->reg->CNT & 0x0000FFFF;
}

void TIM_SetPrescaler(TIM_t *tim, uint32_t prescaler)
{
  if(!prescaler) prescaler = 1;
  tim->prescaler = prescaler;  
  tim->reg->PSC = tim->prescaler - 1;
}

void TIM_SetAutoreload(TIM_t *tim, uint32_t auto_reload)
{
  if(!auto_reload) return;
  tim->auto_reload = auto_reload;
  tim->reg->ARR = tim->auto_reload;
}

void TIM_MaxAutoreload(TIM_t *tim)
{
  if(TIM_Is32bit(tim->reg)) tim->auto_reload = 0xFFFFFFFF;
  else tim->auto_reload = 0xFFFF;
  tim->reg->ARR = tim->auto_reload;
}

uint16_t TIM_Event(TIM_t *tim)
{
  if(tim->_event_cnt) {
    uint16_t response = tim->_event_cnt;
    if(tim->one_pulse_mode) tim->enable = false;
    tim->_event_cnt = 0;
    return response;
  }
  return 0;
}

//------------------------------------------------------------------------------------------------- Init

static void TIM_Interrupt(TIM_t *tim)
{
  if(tim->reg->SR & TIM_SR_UIF) {
    tim->reg->SR &= ~TIM_SR_UIF;
    if(tim->Callback) tim->Callback(tim->callback_arg);
    tim->_event_cnt++;
  }
}

void TIM_Init(TIM_t *tim)
{
  RCC_EnableTIM(tim->reg);
  TIM_SetPrescaler(tim, tim->prescaler);
  if(!tim->auto_reload) TIM_MaxAutoreload(tim);
  else TIM_SetAutoreload(tim, tim->auto_reload);
  tim->reg->CR1 |= (!(tim->one_pulse_mode) << TIM_CR1_ARPE_Pos) | (tim->one_pulse_mode << TIM_CR1_OPM_Pos);
  tim->reg->DIER |= tim->dma_trig ? TIM_DIER_UDE : 0;
  if(tim->enable) TIM_Enable(tim);
  if(tim->enable_interrupt) {
    IRQ_EnableTIM(tim->reg, tim->irq_priority, (void (*)(void *))&TIM_Interrupt, tim);
    TIM_InterruptEnable(tim);
  }
}

void TIM_MasterMode(TIM_t *tim, TIM_MasterMode_t mode)
{
  tim->reg->CR1 &= ~TIM_CR1_UDIS;
  tim->reg->CR2 &= ~TIM_CR2_MMS_Msk;
  tim->reg->CR2 |= (mode << TIM_CR2_MMS_Pos);
}

//------------------------------------------------------------------------------------------------- Delay

void DELAY_Init(TIM_t *tim, TIM_BaseTime_t base_time)
{
  tim->_base_time = base_time;
  TIM_SetPrescaler(tim, (SystemCoreClock / base_time) - 1);
  TIM_Init(tim);
}

inline void DELAY_Wait(TIM_t *tim, uint32_t value)
{
  tim->reg->ARR = value;
  tim->reg->CR1 = TIM_CR1_CEN;
  while(!tim->reg->SR);
  tim->reg->SR = 0;
}

//-------------------------------------------------------------------------------------------------
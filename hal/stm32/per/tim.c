// hal/stm32/per/tim.c

#include "tim.h"

//--------------------------------------------------------------------------------------------- API

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
  tim->auto_reload = TIM_Is32bit(tim->reg) ? 0xFFFFFFFFu : 0xFFFFu;
  tim->reg->ARR = tim->auto_reload;
}

uint16_t TIM_Event(TIM_t *tim)
{
  uint16_t count = tim->_event_cnt;
  if(!count) return 0;
  if(tim->one_pulse_mode) tim->enable = false;
  tim->_event_cnt = 0;
  return count;
}

static void irq_handler(TIM_t *tim)
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
  // Buffered reload for a free-running timer, one-pulse stops on the first update
  if(tim->one_pulse_mode) tim->reg->CR1 |= TIM_CR1_OPM;
  else tim->reg->CR1 |= TIM_CR1_ARPE;
  if(tim->dma_trig) tim->reg->DIER |= TIM_DIER_UDE;
  if(tim->enable) TIM_Enable(tim);
  if(tim->enable_interrupt) {
    IRQ_EnableTIM(tim->reg, tim->irq_priority, (IRQ_Handler_t)irq_handler, tim);
    TIM_InterruptEnable(tim);
  }
}

void TIM_MasterMode(TIM_t *tim, TIM_MasterMode_t mode)
{
  tim->reg->CR1 &= ~TIM_CR1_UDIS;
  tim->reg->CR2 = (tim->reg->CR2 & ~TIM_CR2_MMS_Msk) | (mode << TIM_CR2_MMS_Pos);
}

//------------------------------------------------------------------------------------------- Delay

void DELAY_Init(TIM_t *tim, TIM_BaseTime_t base_time)
{
  tim->_base_time = base_time;
  TIM_SetPrescaler(tim, SystemCoreClock / base_time);
  TIM_Init(tim);
}

void DELAY_Wait(TIM_t *tim, uint32_t value)
{
  // `TIM_Init` leaves `ARPE` set, so `ARR` stays buffered until the next update event.
  // Stopping and rewinding gives the wait a full period
  tim->reg->CR1 = 0;
  tim->reg->ARR = value;
  tim->reg->CNT = 0;
  tim->reg->SR = 0;
  tim->reg->CR1 = TIM_CR1_CEN;
  while(!(tim->reg->SR & TIM_SR_UIF));
  tim->reg->CR1 = 0;
  tim->reg->SR = 0;
}

//-------------------------------------------------------------------------------------------------

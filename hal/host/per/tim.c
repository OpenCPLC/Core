// hal/host/per/tim.c

#include "tim.h"
#include "xdef.h"

TIM_TypeDef HostTim[HOST_TIM_COUNT];

//-------------------------------------------------------------------------------------------------

/**
 * A timer off-target is its register block and nothing else. `TIM_Init` lays the
 * configuration into the registers so code that reads them back sees what it set.
 */

void TIM_SetPrescaler(TIM_t *tim, uint32_t prescaler)
{
  unused(tim);
  unused(prescaler);
}

void TIM_SetAutoreload(TIM_t *tim, uint32_t auto_reload)
{
  unused(tim);
  unused(auto_reload);
}

uint16_t TIM_Event(TIM_t *tim)
{
  unused(tim);
  return 0;
}

void TIM_MasterMode(TIM_t *tim, TIM_MasterMode_t mode)
{
  unused(tim);
  unused(mode);
}

void DELAY_Init(TIM_t *tim, TIM_BaseTime_t base_time)
{
  unused(tim);
  unused(base_time);
}

void DELAY_Wait(TIM_t *tim, uint32_t value)
{
  unused(tim);
  unused(value);
}

void TIM_Init(TIM_t *tim)
{
  if(!tim->reg) return;
  tim->reg->PSC = tim->prescaler ? tim->prescaler - 1 : 0;
  tim->reg->ARR = tim->auto_reload;
}

void TIM_MaxAutoreload(TIM_t *tim)
{
  if(tim->reg) tim->reg->ARR = TIM_Is32bit(tim->reg) ? 0xFFFFFFFFu : 0xFFFFu;
}

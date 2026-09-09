// hal/host/per/pwm.c

#include "pwm.h"
#include "xdef.h"

//-------------------------------------------------------------------------------------------------

/**
 * The compare value firmware asked for lands in `CCRx` and `PWM_OutputEnable` moves `MOE`.
 * Those two are what a bridge is read back through, so they are stored. Dead time, align
 * and waveform stay in `PWM_t` where firmware put them and are acted on by nothing, and
 * frequency has no meaning without a clock to count.
 */

// Compare register of a channel, a complementary output sharing its channel register
static volatile uint32_t *compare(TIM_TypeDef *reg, TIM_Channel_t channel)
{
  if(!reg) return NULL;
  switch(channel) {
    case TIM_CH1: case TIM_CH1N: return &reg->CCR1;
    case TIM_CH2: case TIM_CH2N: return &reg->CCR2;
    case TIM_CH3: case TIM_CH3N: return &reg->CCR3;
    case TIM_CH4: case TIM_CH4N: return &reg->CCR4;
    default: return NULL;
  }
}

//--------------------------------------------------------------------------------------------- API

void PWM_Init(PWM_t *pwm)
{
  if(!pwm->reg) return;
  pwm->reg->ARR = pwm->auto_reload;
  pwm->reg->CCR1 = 0;
  pwm->reg->CCR2 = 0;
  pwm->reg->CCR3 = 0;
  pwm->reg->CCR4 = 0;
}

void PWM_SetPrescaler(PWM_t *pwm, uint32_t prescaler) { pwm->prescaler = prescaler; }
void PWM_SetAlign(PWM_t *pwm, PWM_Align_t align) { pwm->align = align; }
void PWM_SetDeadtime(PWM_t *pwm, uint16_t deadtime) { pwm->deadtime = deadtime; }

void PWM_SetAutoreload(PWM_t *pwm, uint32_t auto_reload)
{
  pwm->auto_reload = auto_reload;
  if(pwm->reg) pwm->reg->ARR = auto_reload;
}

void PWM_SetValue(PWM_t *pwm, TIM_Channel_t channel, uint32_t value)
{
  volatile uint32_t *ccr = compare(pwm->reg, channel);
  if(!ccr) return;
  *ccr = value;
  if(channel <= TIM_CH4) pwm->value[channel] = value;
}

uint32_t PWM_GetValue(PWM_t *pwm, TIM_Channel_t channel)
{
  volatile uint32_t *ccr = compare(pwm->reg, channel);
  return ccr ? *ccr : 0;
}

float PWM_GetFrequency(const PWM_t *pwm) { unused(pwm); return 0.0f; }

float PWM_Frequency(PWM_t *pwm, float frequency)
{
  unused(pwm);
  unused(frequency);
  return 0.0f;
}

void PWM_OutputEnable(PWM_t *pwm, bool enable)
{
  if(!pwm->reg) return;
  if(enable) pwm->reg->BDTR |= TIM_BDTR_MOE;
  else pwm->reg->BDTR &= ~TIM_BDTR_MOE;
}

void PWM_InterruptEnable(PWM_t *pwm) { unused(pwm); }
void PWM_InterruptDisable(PWM_t *pwm) { unused(pwm); }

void PWM_Trigger(PWM_t *pwm, TIM_Channel_t channel, uint16_t compare_value, bool irq)
{
  volatile uint32_t *ccr = compare(pwm->reg, channel);
  if(!ccr) return;
  *ccr = compare_value;
  unused(irq);
}

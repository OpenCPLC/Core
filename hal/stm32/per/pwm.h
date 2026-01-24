#ifndef PWM_H_
#define PWM_H_

#include "gpio.h"
#include "tim.h"

#define PWM_ARR_INIT(freq_Hz, clock_Hz, center_aligned) ((clock_Hz) / (freq_Hz) / ((center_aligned) + 1))

//-------------------------------------------------------------------------------------------------

typedef struct {
  TIM_TypeDef *reg;
  uint32_t prescaler;
  uint32_t auto_reload;
  TIM_CHx_t channel[8];
  bool invert[8];
  uint32_t value[4];
  bool center_aligned;
  uint16_t deadtime; // 0-1024 [tick]
  bool dma_trig;
  uint32_t ccer_mask;
} PWM_t;

void PWM_SetPrescaler(PWM_t *pwm, uint32_t prescaler);
void PWM_SetAutoreload(PWM_t *pwm, uint32_t auto_reload);
void PWM_SetValue(PWM_t *pwm, TIM_Channel_t channel, uint32_t value);
uint32_t PWM_GetValue(PWM_t *pwm, TIM_Channel_t channel);
void PWM_SetDeadtime(PWM_t *pwm, uint16_t deadtime);
void PWM_CenterAlign(PWM_t *pwm, bool enable);
void PWM_OutputEnable(PWM_t *pwm, bool enable);
void PWM_Init(PWM_t *pwm);

//-------------------------------------------------------------------------------------------------
#endif

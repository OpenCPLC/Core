// hal/stm32/per/pwm.h

#ifndef PWM_H_
#define PWM_H_

#include "gpio.h"
#include "tim.h"

//-------------------------------------------------------------------------------------------------

#define PWM_ARR(freq_Hz, clock_Hz, center_aligned) \
  ((clock_Hz) / (freq_Hz) / ((center_aligned) + 1))

/**
 * @brief PWM output configuration.
 * @param[in] reg Timer peripheral (TIM1, TIM2, etc.)
 * @param[in] prescaler Clock prescaler (1 = no division)
 * @param[in] auto_reload Period value (determines PWM frequency)
 * @param[in] channel[8] Channel pin mapping (CH1-4 at [0-3], CH1N-4N at [4-7])
 * @param[in] invert[8] Invert output polarity
 * @param[in] value[4] Initial duty cycle values
 * @param[in] center_aligned Center-aligned mode enable
 * @param[in] deadtime Dead-time in ticks (0-1024)
 * @param[in] dma_trig Enable DMA trigger on update
 * @param[in] UpdateCallback Update interrupt callback (NULL = disabled)
 * @param[in] update_arg Callback argument
 * @param[in] irq_priority Interrupt priority
 */
typedef struct {
  TIM_TypeDef *reg;
  uint32_t prescaler;
  uint32_t auto_reload;
  TIM_CHx_t channel[8];
  bool invert[8];
  uint32_t value[4];
  bool center_aligned;
  uint16_t deadtime;
  bool dma_trig;
  void (*UpdateCallback)(void *);
  void *update_arg;
  IRQ_Priority_t irq_priority;
  // internal
  uint32_t _ccer_mask;
} PWM_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize PWM output.
 * @param[in,out] pwm PWM instance
 */
void PWM_Init(PWM_t *pwm);

/**
 * @brief Set prescaler value.
 * @param[in,out] pwm PWM instance
 * @param[in] prescaler Prescaler (1 = no division)
 */
void PWM_SetPrescaler(PWM_t *pwm, uint32_t prescaler);

/**
 * @brief Set auto-reload value.
 * @param[in,out] pwm PWM instance
 * @param[in] auto_reload Period value
 */
void PWM_SetAutoreload(PWM_t *pwm, uint32_t auto_reload);

/**
 * @brief Set channel duty cycle value.
 * @param[in,out] pwm PWM instance
 * @param[in] channel Channel (TIM_CH1-4 or TIM_CH1N-4N)
 * @param[in] value Compare value
 */
void PWM_SetValue(PWM_t *pwm, TIM_Channel_t channel, uint32_t value);

/**
 * @brief Get channel duty cycle value.
 * @param[in] pwm PWM instance
 * @param[in] channel Channel
 * @return Compare value
 */
uint32_t PWM_GetValue(PWM_t *pwm, TIM_Channel_t channel);

/**
 * @brief Set dead-time for complementary outputs.
 * @param[in,out] pwm PWM instance
 * @param[in] deadtime Dead-time in ticks (0-1024)
 */
void PWM_SetDeadtime(PWM_t *pwm, uint16_t deadtime);

/**
 * @brief Enable/disable center-aligned mode.
 * @param[in,out] pwm PWM instance
 * @param[in] enable `true` for center-aligned, `false` for edge-aligned
 */
void PWM_CenterAlign(PWM_t *pwm, bool enable);

/**
 * @brief Enable/disable PWM output.
 * @param[in,out] pwm PWM instance
 * @param[in] enable Output state
 */
void PWM_OutputEnable(PWM_t *pwm, bool enable);

/**
 * @brief Enable update interrupt.
 * @param[in,out] pwm PWM instance
 */
void PWM_InterruptEnable(PWM_t *pwm);

/**
 * @brief Disable update interrupt.
 * @param[in,out] pwm PWM instance
 */
void PWM_InterruptDisable(PWM_t *pwm);

//-------------------------------------------------------------------------------------------------

#endif
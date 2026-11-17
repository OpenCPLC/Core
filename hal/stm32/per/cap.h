// hal/stm32/per/cap.h

#ifndef CAP_H_
#define CAP_H_

#include "gpio.h"
#include "tim.h"

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Edge capture on one channel of a general-purpose timer:
 *   an edge the digital filter lets through calls `Callback` from the interrupt.
 *   The filter drops pulses shorter than its window, which an EXTI line cannot.
 *   The timer is the channel's alone, counting at the undivided clock the filter samples.
 * @param[in] reg Timer peripheral, `TIM2`, `TIM3`, ...; not `TIM1`,
 *   whose captures come on a vector of their own
 * @param[in] pin Pin of the channel, its entry in `TIM_CHx_MAP`, taken without a pull
 * @param[in] channel Capture channel of the pin, `TIM_CH1..4`
 * @param[in] filter Digital filter of the input
 * @param[in] falling Capture on the falling edge, else on the rising one
 * @param[in] irq_priority Interrupt priority
 * @param[in] Callback Called on every captured edge, from the interrupt
 * @param[in] callback_arg Argument of `Callback`
 * Internal:
 * @param _line The pin as an input, for `CAP_Level`
 */
typedef struct {
  TIM_TypeDef *reg;
  TIM_CHx_t pin;
  TIM_Channel_t channel;
  TIM_Filter_t filter;
  bool falling;
  IRQ_Priority_t irq_priority;
  void (*Callback)(void *);
  void *callback_arg;
  // internal
  GPIO_t _line;
} CAP_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Clock the timer, give the pin to the channel and start capturing.
 * @param[in,out] cap Capture instance
 */
void CAP_Init(CAP_t *cap);

/**
 * @brief Level of the pin, read ahead of the filter.
 * @param[in] cap Capture instance
 * @return `true` when high
 */
bool CAP_Level(CAP_t *cap);

//-------------------------------------------------------------------------------------------------
#endif

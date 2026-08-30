// hal/stm32/per/pwmi.h

#ifndef PWMI_H_
#define PWMI_H_

#include "gpio.h"
#include "tim.h"
#include "xmath.h"
#include "vrts.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef PWMI_AUTO_OVERSAMPLING
  // Average a channel until its accumulated period passes `threshold`, so a slow signal
  // settles after few periods and a fast one after many. Clear it to average a fixed
  // `oversampling` count instead, which makes every channel take the same number of periods
  #define PWMI_AUTO_OVERSAMPLING 1
#endif

#ifndef PWMI_USED_TIM2
  // Widen the accumulators to 64 bits, which only the 32-bit timers can ever need
  #define PWMI_USED_TIM2 0
#endif

/**
 * @brief Frequency and duty measurement of up to four PWM inputs sharing one timer.
 * A timer has a single pair of capture units, so the channels take turns:
 * each one gets the timer to itself, is averaged over a number of periods,
 * and the driver moves on to the next.
 * Results refresh once per full pass, not per channel.
 * @param[in] reg Timer peripheral
 * @param[in] prescaler Timer clock divider, `0` means undivided
 * @param[in] timeout_ms How long a silent channel holds up the pass [ms],
 * `0` derives the longest period the counter can still express
 * @param[in] irq_priority Priority of the timer and external trigger interrupts
 * @param[in] filter Digital filter of the capture inputs
 * @param[in] channel Pin of each channel, a zero entry leaves that channel unused
 * @param[in] trig3 External trigger for CH3, required whenever CH3 is used
 * @param[in] trig4 External trigger for CH4, required whenever CH4 is used
 * @param[in] threshold Averaging ends once the accumulated period passes this value
 * @param[in] oversampling Number of periods to average, when auto oversampling is off
 * Outputs:
 * @param frequency Measured frequency of each channel [Hz], `NaN` without a signal
 * @param duty Measured duty of each channel [0-100%], `NaN` without a signal
 * Internal:
 * @param _init Driver has been started
 * @param _reload Accumulated periods of each channel
 * @param _value Accumulated pulse widths of each channel
 * @param _oversampling Periods each channel was averaged over
 * @param _count Periods accumulated so far for the active channel
 * @param _inc Next channel to arm
 * @param _chan Channel being measured right now
 * @param _timeout_tick Deadline for the next edge
 */
typedef struct {
  TIM_TypeDef *reg;
  uint32_t prescaler;
  uint32_t timeout_ms;
  IRQ_Priority_t irq_priority;
  TIM_Filter_t filter;
  TIM_CHx_t channel[4];
  EXTI_t *trig3;
  EXTI_t *trig4;
  #if(PWMI_AUTO_OVERSAMPLING)
  #if(PWMI_USED_TIM2)
  uint64_t threshold;
  #else
  uint32_t threshold;
  #endif
  #else
  uint16_t oversampling;
  #endif
  // outputs (read-only)
  float frequency[4];
  float duty[4];
  // internal
  bool _init;
  #if(PWMI_USED_TIM2)
  uint64_t _reload[4];
  uint64_t _value[4];
  #else
  uint32_t _reload[4];
  uint32_t _value[4];
  #endif
  #if(PWMI_AUTO_OVERSAMPLING)
  uint16_t _oversampling[4];
  #endif
  uint16_t _count;
  uint8_t _inc;
  uint8_t _chan;
  uint64_t _timeout_tick;
} PWMI_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Set up the timer, the capture pins and the external triggers,
 * then start the first pass.
 * Call once, after every channel pin has been assigned.
 * @param[in,out] pwmi PWMI instance
 */
void PWMI_Init(PWMI_t *pwmi);

/**
 * @brief Advance the measurement, call on every main loop pass. A channel that stays
 * silent is given up on after `timeout_ms` and the pass continues, so a dead input costs
 * time but never blocks the channels behind it.
 * @param[in,out] pwmi PWMI instance
 * @return `true` when a pass has just finished and the outputs were refreshed
 */
bool PWMI_Loop(PWMI_t *pwmi);

//-------------------------------------------------------------------------------------------------

#endif

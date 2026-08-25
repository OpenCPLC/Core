// hal/stm32/per/pwm.h

#ifndef PWM_H_
#define PWM_H_

#include "gpio.h"
#include "tim.h"

//-------------------------------------------------------------------------------------------------

// Counter alignment; values map directly to the `CMS` field.
// The center modes produce identical output waveforms
// and differ only in when compare interrupt flags fire:
// mode 1 on down-count, mode 2 on up-count, mode 3 in both directions.
// Mode 3 makes the compare interrupt cadence depend on the compare position
// (events near an extremum coalesce into one interrupt,
// mid-range ones fire twice per period),
// so time-critical users (`PWM_Trigger` interrupts) need mode 1, 2 or edge
typedef enum {
  PWM_Align_Edge = 0,
  PWM_Align_Center1 = 1,
  PWM_Align_Center2 = 2,
  PWM_Align_Center3 = 3
} PWM_Align_t;

// Counter ticks in one period: `ARR + 1` edge-aligned (`0..ARR` inclusive),
// `2 * ARR` center-aligned (`0..ARR..0`, the turning points pass once)
#define pwm_period_ticks(arr, align) ((align) ? (2u * (arr)) : ((arr) + 1u))

// `ARR` register value for a target frequency, inverse of `pwm_period_ticks`
#define PWM_ARR(freq_Hz, clock_Hz, align) \
  ((align) ? ((clock_Hz) / (freq_Hz) / 2u) : (((clock_Hz) / (freq_Hz)) - 1u))

/**
 * @brief PWM output configuration.
 * @param[in] reg Timer peripheral (TIM1, TIM2, etc.)
 * @param[in] prescaler Clock prescaler (1 = no division)
 * @param[in] auto_reload Period value (determines PWM frequency)
 * @param[in] channel[8] Channel pin mapping (CH1-4 at [0-3], CH1N-4N at [4-7])
 * @param[in] invert[8] Invert output polarity
 * @param[in] value[4] Compare values, kept current by `PWM_SetValue` and `PWM_Frequency`
 * @param[in] align Counter alignment (`PWM_Align_...`)
 * @param[in] deadtime Dead-time in ticks (0-1008)
 * @param[in] dma_trig Enable DMA trigger on update
 * @param[in] UpdateCallback Update interrupt callback (NULL = disabled)
 * @param[in] update_arg Callback argument
 * @param[in] irq_priority Interrupt priority
 * Internal:
 * @param _ccer_mask Enable bits of the configured outputs, for timers without `BDTR`
 */
typedef struct {
  TIM_TypeDef *reg;
  uint32_t prescaler;
  uint32_t auto_reload;
  TIM_CHx_t channel[8];
  bool invert[8];
  uint32_t value[4];
  PWM_Align_t align;
  uint16_t deadtime;
  bool dma_trig;
  void (*UpdateCallback)(void *);
  void *update_arg;
  IRQ_Priority_t irq_priority;
  // internal
  uint32_t _ccer_mask;
} PWM_t;

//--------------------------------------------------------------------------------------------- API

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
 * @brief Set dead-time for complementary outputs. Values snap down to the nearest
 * step the `DTG` encoding can express: 1 tick below 128, then 2, 8 and 16 ticks.
 * @param[in,out] pwm PWM instance
 * @param[in] deadtime Dead-time in ticks (0-1008)
 */
void PWM_SetDeadtime(PWM_t *pwm, uint16_t deadtime);

/**
 * @brief Current output frequency computed from the prescaler and reload settings.
 * @param[in] pwm PWM instance
 * @return Frequency [Hz]
 */
float PWM_GetFrequency(const PWM_t *pwm);

/**
 * @brief Retune the timer to a target frequency:
 * the smallest prescaler that fits the period in the 16-bit reload keeps duty resolution,
 * and every active compare value is rescaled so the duty of each channel survives.
 * One timer drives all its channels, so the change affects every output of this instance.
 * @param[in,out] pwm PWM instance
 * @param[in] frequency Target frequency [Hz]
 * @return Actually applied frequency [Hz]
 */
float PWM_Frequency(PWM_t *pwm, float frequency);

/**
 * @brief Change the counter alignment; briefly stops the counter (`CMS` demands it).
 * @param[in,out] pwm PWM instance
 * @param[in] align Counter alignment (`PWM_Align_...`)
 */
void PWM_SetAlign(PWM_t *pwm, PWM_Align_t align);

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

/**
 * @brief Route a spare compare channel to `TRGO2` as a hardware trigger point
 * (the ADC external trigger line).
 * `OCxREF` in PWM mode 2 rises exactly once per period at the up-count compare;
 * the reference lives independently of `CCxE` and `MOE`,
 * so it runs with the outputs disabled.
 * Advanced timers only (`TRGO2`); call after `PWM_Init`.
 * @param[in,out] pwm PWM instance
 * @param[in] channel Spare compare channel (`TIM_CH1..4`), not mapped to any pin
 * @param[in] compare Compare point in timer ticks;
 * a center-aligned compare at `auto_reload` never fires, keep it below
 * @param[in] irq Also raise the capture/compare interrupt on the event,
 * routed to a handler with `IRQ_EnableTIMCC`
 */
void PWM_Trigger(PWM_t *pwm, TIM_Channel_t channel, uint16_t compare, bool irq);

//-------------------------------------------------------------------------------------------------

#endif
// hal/stm32/per/pwmi.h

#ifndef PWMI_H_
#define PWMI_H_

#include "gpio.h"
#include "tim.h"
#include "gpio.h"
#include "xmath.h"
#include "vrts.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef PWMI_AUTO_OVERSAMPLING
  // When set, samples until `threshold` reached. When 0, fixed `oversampling` count.
  #define PWMI_AUTO_OVERSAMPLING 1
#endif


#ifndef PWMI_USED_TIM2
  // TIM2 is 32-bit, needs larger buffers.
  #define PWMI_USED_TIM2 0
#endif

typedef enum {
  PWMI_CapturePrescaler_1 = 0,
  PWMI_CapturePrescaler_2 = 1,
  PWMI_CapturePrescaler_4 = 2,
  PWMI_CapturePrescaler_8 = 3
} PWMI_CapturePrescaler_t;

/**
 * @brief PWM input measurement configuration.
 * @param[in] reg Timer peripheral (TIM1, TIM2, etc.)
 * @param[in] prescaler Clock prescaler
 * @param[in] timeout_ms Measurement timeout
 * @param[in] capture_prescaler Input capture prescaler
 * @param[in] irq_priority Interrupt priority
 * @param[in] filter Input filter
 * @param[in] channel[4] Channel pin mapping
 * @param[in] trig3 External trigger for CH3 (optional)
 * @param[in] trig4 External trigger for CH4 (optional)
 * @param[in] threshold Auto-oversampling threshold (when `PWMI_AUTO_OVERSAMPLING`)
 * @param[in] oversampling Fixed oversampling count (when not auto)
 */
typedef struct {
  TIM_TypeDef *reg;
  uint32_t prescaler;
  uint32_t timeout_ms;
  PWMI_CapturePrescaler_t capture_prescaler;
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
  uint64_t _timeout_tick;
} PWMI_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize PWM input measurement.
 * @param[in,out] pwmi PWMI instance
 */
void PWMI_Init(PWMI_t *pwmi);

/**
 * @brief Process PWM input measurement (call in main loop).
 * @param[in,out] pwmi PWMI instance
 * @return `true` if new measurement available
 */
bool PWMI_Loop(PWMI_t *pwmi);

//-------------------------------------------------------------------------------------------------

#endif
// hal/stm32g0/adc_g0.h

#ifndef ADC_G0_H_
#define ADC_G0_H_

#include "stm32g0xx.h"

//---------------------------------------------------------------------------------------------

typedef enum {
  ADC_IN_PA0 = 0,
  ADC_IN_PA1 = 1,
  ADC_IN_PA2 = 2,
  ADC_IN_PA3 = 3,
  ADC_IN_PA4 = 4,
  ADC_IN_PA5 = 5,
  ADC_IN_PA6 = 6,
  ADC_IN_PA7 = 7,
  ADC_IN_PB0 = 8,
  ADC_IN_PB1 = 9,
  ADC_IN_PB2 = 10,
  ADC_IN_PB10 = 11,
  ADC_IN_TSEN = 12,
  ADC_IN_VREFEN = 13,
  ADC_IN_VBATEN = 14,
  ADC_IN_PB11 = 15,
  ADC_IN_PB12 = 16,
  ADC_IN_PC4 = 17,
  ADC_IN_PC5 = 18
} ADC_IN_t;

// Total conversion time in ADC clock cycles (sampling + 12.5)
typedef enum {
  ADC_SamplingTime_14 = 0,
  ADC_SamplingTime_16 = 1,
  ADC_SamplingTime_20 = 2,
  ADC_SamplingTime_25 = 3,
  ADC_SamplingTime_32 = 4,
  ADC_SamplingTime_52 = 5,
  ADC_SamplingTime_92 = 6,
  ADC_SamplingTime_173 = 7
} ADC_SamplingTime_t;

typedef enum {
  ADC_ExtTrig_TIM1 = 0,
  ADC_ExtTrig_TIM1_CC4 = 1,
  ADC_ExtTrig_TIM2 = 2,
  ADC_ExtTrig_TIM3 = 3,
  ADC_ExtTrig_TIM15 = 4,
  ADC_ExtTrig_TIM6 = 5,
  ADC_ExtTrig_TIM4 = 6,
  ADC_ExtTrig_EXTI11 = 7
} ADC_ExtTrig_t;

//---------------------------------------------------------------------------------------------
#endif
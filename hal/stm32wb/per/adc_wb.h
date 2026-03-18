// hal/stm32wb/adc_wb.h

#ifndef ADC_WB_H_
#define ADC_WB_H_

#include "stm32wbxx.h"

//---------------------------------------------------------------------------------------------

typedef enum {
  ADC_IN_VREFEN = 0,
  ADC_IN_PC0 = 1,
  ADC_IN_PC1 = 2,
  ADC_IN_PC2 = 3,
  ADC_IN_PC3 = 4,
  ADC_IN_PA0 = 5,
  ADC_IN_PA1 = 6,
  ADC_IN_PA2 = 7,
  ADC_IN_PA3 = 8,
  ADC_IN_PA4 = 9,
  ADC_IN_PA5 = 10,
  ADC_IN_PA6 = 11,
  ADC_IN_PA7 = 12,
  ADC_IN_PC4 = 13,
  ADC_IN_PC5 = 14,
  ADC_IN_PA8 = 15,
  ADC_IN_PA9 = 16,
  ADC_IN_TSEN = 17,
  ADC_IN_VBATEN = 18
} ADC_IN_t;

// Total conversion time in ADC clock cycles (sampling + 12.5)
typedef enum {
  ADC_SamplingTime_15 = 0,
  ADC_SamplingTime_19 = 1,
  ADC_SamplingTime_25 = 2,
  ADC_SamplingTime_37 = 3,
  ADC_SamplingTime_60 = 4,
  ADC_SamplingTime_105 = 5,
  ADC_SamplingTime_260 = 6,
  ADC_SamplingTime_653 = 7
} ADC_SamplingTime_t;

typedef enum {
  ADC_ExtTrig_TIM1_TRGO2 = 0,
  ADC_ExtTrig_TIM1_CC4 = 1,
  ADC_ExtTrig_TIM1 = 2,
  ADC_ExtTrig_TIM2 = 3,
  ADC_ExtTrig_TIM2_CC2 = 4,
  ADC_ExtTrig_TIM2_CC4 = 5,
  ADC_ExtTrig_TIM2_CC3 = 6,
  ADC_ExtTrig_TIM2_CC1 = 7,
  ADC_ExtTrig_EXTI11 = 8,
  ADC_ExtTrig_TIM1_CC1 = 9,
  ADC_ExtTrig_TIM1_CC2 = 10,
  ADC_ExtTrig_TIM1_CC3 = 11,
  ADC_ExtTrig_LPTIM1 = 12,
  ADC_ExtTrig_LPTIM2 = 13
} ADC_ExtTrig_t;

//---------------------------------------------------------------------------------------------
#endif
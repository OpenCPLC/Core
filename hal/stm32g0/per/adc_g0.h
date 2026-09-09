// hal/stm32g0/per/adc_g0.h

#ifndef ADC_G0_H_
#define ADC_G0_H_

#include "stm32g0xx.h"

//-------------------------------------------------------------------------------------------------

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

// Kernel clock route, zero (`Default`) follows the framework clock tree
typedef enum {
  ADC_Clock_Default = 0, // HSI16: exact 16MHz whatever the system clock runs at
  ADC_Clock_SYSCLK = 1,
  ADC_Clock_PLLP = 2,    // frequency unknown to the framework
  ADC_Clock_HSI16 = 3
} ADC_Clock_t;

// Factory calibration, measured at VDDA = 3.0V on this family
#define ADC_CAL_VDDA_mV 3000
#define ADC_VREFINT_CAL (*(const uint16_t *)0x1FFF75AAu)
#define ADC_TS_CAL1     (*(const uint16_t *)0x1FFF75A8u) // temperature sensor at 30 deg C
#define ADC_TS_CAL2     (*(const uint16_t *)0x1FFF75CAu) // temperature sensor at 130 deg C

// Total conversion time in ADC clock cycles (sampling + 12.5)
typedef enum {
  ADC_SamplingTime_14 = 0,
  ADC_SamplingTime_16 = 1,
  ADC_SamplingTime_20 = 2,
  ADC_SamplingTime_25 = 3,
  ADC_SamplingTime_32 = 4,
  ADC_SamplingTime_52 = 5,
  ADC_SamplingTime_92 = 6,
  ADC_SamplingTime_173 = 7,
  ADC_SamplingTime_Max = ADC_SamplingTime_173
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

// G0 errata: hardware oversampling combined with the configurable sequencer corrupts
// the last conversion of a multi-channel sequence. The driver appends one sacrificial
// repeat of the last channel, so every scan carries one extra word: size record
// buffers and frame strides with this
#define adc_scan_len(channel_count, ovs_enable) \
  ((uint16_t)(channel_count) + (((ovs_enable) && (channel_count) > 1) ? 1 : 0))

//-------------------------------------------------------------------------------------------------
#endif

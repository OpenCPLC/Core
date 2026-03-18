// hal/stm32/per/dac.h

#ifndef DAC_H_
#define DAC_H_

#include <stdbool.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "vrts.h"

#define DAC_CH1 (DAC1->DHR12R1)
#define DAC_CH2 (DAC1->DHR12R2)
#define DAC_CH1_CH2 (DAC1->DHR12RD)
#define DAC_MASK 0x0FFF // 12-bit right-aligned
#define DAC_SET(ch1, ch2) (((uint32_t)((ch2) & DAC_MASK) << 16) | ((ch1) & DAC_MASK))

void DAC_Calib(bool ch1_pa4, bool ch2_pa5);
void DAC_Init(bool ch1_pa4, bool ch2_pa5);
void DAC_SetCH1(uint16_t value);
void DAC_SetCH2(uint16_t value);
void DAC_Set(uint16_t ch1, uint16_t ch2);

#endif

// hal/stm32/per/dac.h

#ifndef DAC_H_
#define DAC_H_

#include <stdbool.h>
#include <stdint.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif

//--------------------------------------------------------------------------------------- Constants

// 12-bit right-aligned data
#define DAC_MASK 0x0FFFu

//--------------------------------------------------------------------------------------------- API

// Run the output buffer calibration of the selected channels, done by `DAC_Init`
void DAC_Calib(bool ch1, bool ch2);

/**
 * @brief Enable the selected channels on their analog pins, `PA4` and `PA5`.
 * @param[in] ch1 Channel 1
 * @param[in] ch2 Channel 2
 */
void DAC_Init(bool ch1, bool ch2);

/**
 * @brief Set one channel.
 * @param[in] value Output, `0..4095`
 */
static inline void DAC_SetCH1(uint16_t value) { DAC1->DHR12R1 = value & DAC_MASK; }
static inline void DAC_SetCH2(uint16_t value) { DAC1->DHR12R2 = value & DAC_MASK; }

/**
 * @brief Set both channels in one write.
 * @param[in] ch1 Channel 1 output, `0..4095`
 * @param[in] ch2 Channel 2 output, `0..4095`
 */
static inline void DAC_Set(uint16_t ch1, uint16_t ch2)
{
  DAC1->DHR12RD = ((uint32_t)(ch2 & DAC_MASK) << 16) | (ch1 & DAC_MASK);
}

//-------------------------------------------------------------------------------------------------
#endif

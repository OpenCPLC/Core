// hal/stm32/per/rng.h

#ifndef RNG_H_
#define RNG_H_

#include <stdbool.h>
#include <stdint.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "main.h"

//------------------------------------------------------------------------------------------- Types

typedef enum {
  RNG_Source_Void = 0, // family reset route, `HSI48` on STM32WB
  RNG_Source_HSI16 = 1,
  RNG_Source_Sysclk = 2,
  RNG_Source_PLLQ = 3
} RNG_Source_t;

// Clock divider, STM32G0 only
typedef enum {
  RNG_Divider_1 = 0,
  RNG_Divider_2 = 1,
  RNG_Divider_4 = 2,
  RNG_Divider_8 = 3
} RNG_Divider_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Clock and start the generator.
 * @param[in] source Kernel clock route
 * @param[in] div Divider, ignored where the family has none
 */
void RNG_Init(RNG_Source_t source, RNG_Divider_t div);

// Next 32-bit random value, waits for one
uint32_t RNG_Run(void);

/**
 * @brief Random value in `[min, max)`.
 * @param[in] min Lower bound, included
 * @param[in] max Upper bound, excluded
 * @return Random value, `min` when the range is empty
 */
int32_t rng(int32_t min, int32_t max);

//-------------------------------------------------------------------------------------------------
#endif

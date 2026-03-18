// hal/stm32/per/rng.h

#ifndef RNG_H_
#define RNG_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#elif defined(STM32G4)
  #include "stm32g4xx.h"
#endif
#include "main.h"

//------------------------------------------------------------------------------------------------- Compatibility

#if defined(STM32G0)
  #define RCC_RNG_EN()  (RCC->AHBENR |= RCC_AHBENR_RNGEN)
  #define RNG_HAS_DIV   1
#elif defined(STM32WB)
  #define RCC_RNG_EN()  (RCC->AHB3ENR |= RCC_AHB3ENR_RNGEN)
  #define RNG_HAS_DIV   0
#elif defined(STM32G4)
  #define RCC_RNG_EN()  (RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN)
  #define RNG_HAS_DIV   0
#endif

//------------------------------------------------------------------------------------------------- Types

typedef enum {
  RNG_Source_Void = 0,
  RNG_Source_HSI16 = 1,
  RNG_Source_Sysclk = 2,
  RNG_Source_PLLQ = 3
} RNG_Source_t;

typedef enum {
  RNG_Divider_1 = 0,
  RNG_Divider_2 = 1,
  RNG_Divider_4 = 2,
  RNG_Divider_8 = 3
} RNG_Divider_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Get random 32-bit value.
 * @return Random number
 */
uint32_t RNG_Run(void);

/**
 * @brief Initialize RNG peripheral.
 * @param[in] source Clock source
 * @param[in] div Divider (G0 only, ignored on WB/G4)
 */
void RNG_Init(RNG_Source_t source, RNG_Divider_t div);

/**
 * @brief Get random value in range [min, max).
 * @param[in] min Minimum value (inclusive)
 * @param[in] max Maximum value (exclusive)
 * @return Random number in range
 */
int32_t rng(int32_t min, int32_t max);

//-------------------------------------------------------------------------------------------------

#endif
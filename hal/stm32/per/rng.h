#ifndef RNG_H_
#define RNG_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "main.h"

typedef enum {
  RNG_Source_Void = 0,
  RNG_Source_HSI16 = 1,
  RNG_Source_Sysclk = 2,
  RNG_Source_PLLQ = 3
} RNG_Source_e;

typedef enum {
  RNG_Divider_1 = 0,
  RNG_Divider_2 = 1,
  RNG_Divider_4 = 2,
  RNG_Divider_8 = 3
} RNG_Divider_e;

uint32_t RNG_Run(void);
void RNG_Init(RNG_Source_e source, RNG_Divider_e div);
int32_t rng(int32_t min, int32_t max);

//-------------------------------------------------------------------------------------------------
#endif

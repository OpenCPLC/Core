// hal/stm32/per/rng.c

#include "rng.h"

//-------------------------------------------------------------------------------------------------

uint32_t RNG_Run(void)
{
  while(!(RNG->SR & RNG_SR_DRDY)) __NOP();
  return RNG->DR;
}

void RNG_Init(RNG_Source_t source, RNG_Divider_t div)
{
  RCC->CCIPR = (RCC->CCIPR & (~RCC_CCIPR_RNGSEL_Msk)) | (source << RCC_CCIPR_RNGSEL_Pos);
  #if(RNG_HAS_DIV)
    RCC->CCIPR = (RCC->CCIPR & (~RCC_CCIPR_RNGDIV_Msk)) | (div << RCC_CCIPR_RNGDIV_Pos);
  #else
    unused(div);
  #endif
  RCC_RNG_EN();
  RNG->CR |= RNG_CR_RNGEN;
}

int32_t rng(int32_t min, int32_t max)
{
  return (RNG_Run() % (max - min)) + min;
}

//-------------------------------------------------------------------------------------------------
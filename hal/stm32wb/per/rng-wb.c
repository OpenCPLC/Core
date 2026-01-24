#include "rng.h"

//-------------------------------------------------------------------------------------------------

uint32_t RNG_Run(void)
{
  while(!(RNG->SR & RNG_SR_DRDY)) __NOP();
  return RNG->DR;
}

void RNG_Init(RNG_Source_e source, RNG_Divider_e div)
{
  (void)div;
  if(source == RNG_Source_PLLQ) { /* PLLQ clock should be enabled by caller if used */ }
  RCC->CCIPR = (RCC->CCIPR & (~RCC_CCIPR_RNGSEL_Msk)) | (source << RCC_CCIPR_RNGSEL_Pos);
  RCC->AHB3ENR |= RCC_AHB3ENR_RNGEN;
  RNG->CR |= RNG_CR_RNGEN;
}

int32_t rng(int32_t min, int32_t max)
{
  return (RNG_Run() % (max - min)) + min;
}

//-------------------------------------------------------------------------------------------------
// hal/stm32/per/rng.c

#include "rng.h"

#if defined(STM32WB)
  #include "hsem_wb.h"
#endif

//-------------------------------------------------------------------------------------------------

uint32_t RNG_Run(void)
{
  #if defined(STM32WB)
    HSEM_Wait(HSEM_RNG); // the CPU2 wireless stack shares the peripheral
  #endif
  while(!(RNG->SR & RNG_SR_DRDY)) __NOP();
  uint32_t value = RNG->DR;
  #if defined(STM32WB)
    HSEM_Give(HSEM_RNG);
  #endif
  return value;
}

void RNG_Init(RNG_Source_t source, RNG_Divider_t div)
{
  #if defined(STM32WB)
    // Source `0` is the CLK48 domain here: hold its semaphore and start HSI48,
    // or the CPU2 stack may stop the clock under a pending generation
    if(source == RNG_Source_Void) {
      HSEM_Wait(HSEM_CLK48);
      RCC->CRRCR |= RCC_CRRCR_HSI48ON;
      while(!(RCC->CRRCR & RCC_CRRCR_HSI48RDY));
      RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL; // `00` = `HSI48`
    }
  #endif
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
  if(max <= min) return min;
  // Unsigned subtraction: `max - min` overflows `int32_t` for a full-range span
  uint32_t span = (uint32_t)max - (uint32_t)min;
  return (int32_t)((uint32_t)min + RNG_Run() % span);
}

//-------------------------------------------------------------------------------------------------

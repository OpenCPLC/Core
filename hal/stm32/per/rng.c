// hal/stm32/per/rng.c

#include "rng.h"

#include "pwr.h"
#if defined(STM32WB)
  #include "hsem_wb.h"
#endif

//--------------------------------------------------------------------------------------------- API

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
  unused(div);
  #endif
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_RNGSEL_Msk) | (source << RCC_CCIPR_RNGSEL_Pos);
  #if defined(STM32G0)
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_RNGDIV_Msk) | (div << RCC_CCIPR_RNGDIV_Pos);
  #endif
  RCC_EnableRNG();
  RNG->CR |= RNG_CR_RNGEN;
}

uint32_t RNG_Run(void)
{
  #if defined(STM32WB)
  HSEM_Wait(HSEM_RNG); // the CPU2 wireless stack shares the peripheral
  RNG->CR |= RNG_CR_RNGEN; // the CPU2 may leave the generator off after its turn
  #endif
  uint32_t value;
  do {
    // Seed error recovery from the reference manual:
    // clear the flag, then discard the 12 words already in the pipeline.
    if(RNG->SR & RNG_SR_SEIS) {
      RNG->SR = ~RNG_SR_SEIS;
      for(uint8_t i = 0; i < 12; i++) (void)RNG->DR;
    }
    while(!(RNG->SR & (RNG_SR_DRDY | RNG_SR_SEIS))) __NOP();
    value = RNG->DR;
  } while(RNG->SR & RNG_SR_SEIS); // a word read beside a seed error is not used
  #if defined(STM32WB)
  HSEM_Give(HSEM_RNG);
  #endif
  return value;
}

int32_t rng(int32_t min, int32_t max)
{
  if(max <= min) return min;
  // Unsigned subtraction: `max - min` overflows `int32_t` for a full-range span
  uint32_t span = (uint32_t)max - (uint32_t)min;
  return (int32_t)((uint32_t)min + RNG_Run() % span);
}

//-------------------------------------------------------------------------------------------------

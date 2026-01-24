#include "stm32g0xx.h"

// Value of the External oscillator in Hz
#ifndef HSE_VALUE
  #define HSE_VALUE (8000000UL)    
#endif

// Value of the Internal oscillator in Hz
#ifndef HSI_VALUE
  #define HSI_VALUE  (16000000UL)   
#endif

// Value of LSI in Hz
#ifndef LSI_VALUE
 #define LSI_VALUE (32000UL)
#endif

// Value of LSE in Hz
#ifndef LSE_VALUE 
  #define LSE_VALUE (32768UL)
#endif

// Following vector table addresses must be defined in line with linker configuration.
// Uncomment the following line if you need to relocate the vector table
// anywhere in Flash or Sram, else the vector table is kept at the automatic
// remap of boot address selected
#if defined(USER_VECT_TAB_ADDRESS)
  #if defined(VECT_TAB_SRAM)
    #define VECT_TAB_BASE_ADDRESS  SRAM_BASE
    #define VECT_TAB_OFFSET        0x00000000U
  #else
    #define VECT_TAB_BASE_ADDRESS  FLASH_BASE
    #define VECT_TAB_OFFSET        0x00000000U
  #endif
#endif

uint32_t SystemCoreClock = 16000000UL;
const uint32_t AHBPrescTable[16UL] = {0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL, 6UL, 7UL, 8UL, 9UL};
const uint32_t APBPrescTable[8UL] =  {0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL};

void SystemInit(void)
{
  #if defined(USER_VECT_TAB_ADDRESS)
    SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET; // Vector Table Relocation
  #endif
}

void SystemCoreClockUpdate(void)
{
  uint32_t tmp;
  uint32_t pllvco;
  uint32_t pllr;
  uint32_t pllsource;
  uint32_t pllm;
  uint32_t hsidiv;
  switch (RCC->CFGR & RCC_CFGR_SWS) {
    case RCC_CFGR_SWS_0:
      SystemCoreClock = HSE_VALUE;
      break;
    case (RCC_CFGR_SWS_1 | RCC_CFGR_SWS_0):
      SystemCoreClock = LSI_VALUE;
      break;
    case RCC_CFGR_SWS_2:
      SystemCoreClock = LSE_VALUE;
      break;
    case RCC_CFGR_SWS_1:
      pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
      pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1UL;
      if(pllsource == 0x03UL) { // HSE used as PLL clock source
        pllvco = (HSE_VALUE / pllm);
      }
      else { // HSI used as PLL clock source
        pllvco = (HSI_VALUE / pllm);
      }
      pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
      pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1UL);
      SystemCoreClock = pllvco/pllr;
      break;
    case 0x00000000U: // HSI used as system clock
    default:
      hsidiv = (1UL << ((READ_BIT(RCC->CR, RCC_CR_HSIDIV))>> RCC_CR_HSIDIV_Pos));
      SystemCoreClock = (HSI_VALUE/hsidiv);
      break;
  }
  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)]; //  Get HCLK prescaler
  SystemCoreClock >>= tmp; // HCLK clock frequency
}

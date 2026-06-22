// hal/stm32g0/per/dac_g0.c

#include "dac.h"

void DAC_Calib(bool ch1_pa4, bool ch2_pa5)
{
  DAC1->CR &= ~((ch1_pa4 ? DAC_CR_EN1 : 0) | (ch2_pa5 ? DAC_CR_EN2 : 0));
  DAC1->CR |= (ch1_pa4 ? DAC_CR_CEN1: 0) | (ch2_pa5 ? DAC_CR_CEN2: 0); // Start
  for(volatile uint32_t i = 0; i < (SystemCoreClock / 1000000) * 2 / 3; ++i) let(); // Wait ~2µs 
  DAC1->CR &= ~((ch1_pa4 ? DAC_CR_CEN1: 0) | (ch2_pa5 ? DAC_CR_CEN2: 0)); // Stop
}

void DAC_Init(bool ch1_pa4, bool ch2_pa5)
{
  if(!ch1_pa4 && !ch2_pa5) return;
  RCC->IOPENR  |= RCC_IOPENR_GPIOAEN;
  RCC->APBENR1 |= RCC_APBENR1_DAC1EN;
  uint32_t moder = GPIOA->MODER;
  uint32_t pupdr = GPIOA->PUPDR;
  if(ch1_pa4) {
    moder |= (0x03 << GPIO_MODER_MODE4_Pos);
    pupdr &= ~(0x03 << GPIO_PUPDR_PUPD4_Pos);
  }
  if(ch2_pa5) {
    moder |= (0x03 << GPIO_MODER_MODE5_Pos);
    pupdr &= ~(0x03 << GPIO_PUPDR_PUPD5_Pos);
  }
  RCC->APBRSTR1 |=  RCC_APBRSTR1_DAC1RST;
  RCC->APBRSTR1 &= ~RCC_APBRSTR1_DAC1RST;
  DAC_Calib(ch1_pa4, ch2_pa5);
  DAC1->CR |= (ch1_pa4 ? DAC_CR_EN1: 0) | (ch2_pa5 ? DAC_CR_EN2: 0);
}

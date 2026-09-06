// hal/stm32g0/per/dac_g0.c

#include "dac.h"

#include "vrts.h"

//--------------------------------------------------------------------------------------------- API

void DAC_Calib(bool ch1, bool ch2)
{
  DAC1->CR &= ~((ch1 ? DAC_CR_EN1 : 0) | (ch2 ? DAC_CR_EN2 : 0));
  DAC1->CR |= (ch1 ? DAC_CR_CEN1 : 0) | (ch2 ? DAC_CR_CEN2 : 0);
  // Calibration takes about 2 us
  for(volatile uint32_t i = 0; i < (SystemCoreClock / 1000000) * 2 / 3; ++i) let();
  DAC1->CR &= ~((ch1 ? DAC_CR_CEN1 : 0) | (ch2 ? DAC_CR_CEN2 : 0));
}

// `CH1` on `PA4`, `CH2` on `PA5`
void DAC_Init(bool ch1, bool ch2)
{
  if(!ch1 && !ch2) return;
  RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
  RCC->APBENR1 |= RCC_APBENR1_DAC1EN;
  uint32_t moder = GPIOA->MODER;
  uint32_t pupdr = GPIOA->PUPDR;
  if(ch1) {
    moder |= 0x3u << GPIO_MODER_MODE4_Pos;
    pupdr &= ~(0x3u << GPIO_PUPDR_PUPD4_Pos);
  }
  if(ch2) {
    moder |= 0x3u << GPIO_MODER_MODE5_Pos;
    pupdr &= ~(0x3u << GPIO_PUPDR_PUPD5_Pos);
  }
  // Analog mode with no pull is also the reset state, so this only matters when the pin
  // was configured for something else first
  GPIOA->MODER = moder;
  GPIOA->PUPDR = pupdr;
  RCC->APBRSTR1 |= RCC_APBRSTR1_DAC1RST;
  RCC->APBRSTR1 &= ~RCC_APBRSTR1_DAC1RST;
  DAC_Calib(ch1, ch2);
  DAC1->CR |= (ch1 ? DAC_CR_EN1 : 0) | (ch2 ? DAC_CR_EN2 : 0);
}

//-------------------------------------------------------------------------------------------------

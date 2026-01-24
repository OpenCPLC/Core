#include "pwr.h"

//------------------------------------------------------------------------------------------------- SYS

void RAMP_PA11_PA12(void) { /* not applicable on WB */ }

//------------------------------------------------------------------------------------------------- Clock Enable

void RCC_EnableTIM(TIM_TypeDef *tim_typedef)
{
  switch((uint32_t)tim_typedef) {
    case (uint32_t)TIM1:   RCC->APB2ENR  |= RCC_APB2ENR_TIM1EN; break;
    case (uint32_t)TIM2:   RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN; break;
    case (uint32_t)TIM16:  RCC->APB2ENR  |= RCC_APB2ENR_TIM16EN; break;
    case (uint32_t)TIM17:  RCC->APB2ENR  |= RCC_APB2ENR_TIM17EN; break;
    case (uint32_t)LPTIM1: RCC->APB1ENR1 |= RCC_APB1ENR1_LPTIM1EN; break;
    case (uint32_t)LPTIM2: RCC->APB1ENR2 |= RCC_APB1ENR2_LPTIM2EN; break;
    default: break;
  }
}

void RCC_EnableGPIO(GPIO_TypeDef *gpio_typedef)
{
  switch((uint32_t)gpio_typedef) {
    case (uint32_t)GPIOA: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN; break;
    case (uint32_t)GPIOB: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; break;
    case (uint32_t)GPIOC: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN; break;
    case (uint32_t)GPIOD: RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN; break;
    case (uint32_t)GPIOE: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN; break;
    case (uint32_t)GPIOH: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOHEN; break;
    default: break;
  }
}

void RCC_EnableUART(USART_TypeDef *uart_typedef)
{
  switch((uint32_t)uart_typedef) {
    case (uint32_t)USART1:  RCC->APB2ENR  |= RCC_APB2ENR_USART1EN; break;
    case (uint32_t)LPUART1: RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN; break;
    default: break;
  }
}

void RCC_DisableUART(USART_TypeDef *uart_typedef)
{
  switch((uint32_t)uart_typedef) {
    case (uint32_t)USART1:  RCC->APB2ENR  &= ~RCC_APB2ENR_USART1EN; break;
    case (uint32_t)LPUART1: RCC->APB1ENR2 &= ~RCC_APB1ENR2_LPUART1EN; break;
    default: break;
  }
}

void RCC_EnableI2C(I2C_TypeDef *i2c_typedef)
{
  switch((uint32_t)i2c_typedef) {
    case (uint32_t)I2C1: RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN; break;
    case (uint32_t)I2C3: RCC->APB1ENR1 |= RCC_APB1ENR1_I2C3EN; break;
    default: break;
  }
}

void RCC_DisableI2C(I2C_TypeDef *i2c_typedef)
{
  switch((uint32_t)i2c_typedef) {
    case (uint32_t)I2C1: RCC->APB1ENR1 &= ~RCC_APB1ENR1_I2C1EN; break;
    case (uint32_t)I2C3: RCC->APB1ENR1 &= ~RCC_APB1ENR1_I2C3EN; break;
    default: break;
  }
}

void RCC_EnableSPI(SPI_TypeDef *spi_typedef)
{
  switch((uint32_t)spi_typedef) {
    case (uint32_t)SPI1: RCC->APB2ENR  |= RCC_APB2ENR_SPI1EN; break;
    case (uint32_t)SPI2: RCC->APB1ENR1 |= RCC_APB1ENR1_SPI2EN; break;
    default: break;
  }
}

void RCC_EnableDMA(DMA_TypeDef *dam_typedef)
{
  switch((uint32_t)dam_typedef) {
    case (uint32_t)DMA1: RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; break;
    case (uint32_t)DMA2: RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; break;
    default: break;
  }
  RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;
}

//------------------------------------------------------------------------------------------------- RCC

static void RCC_FLASH_SetLatency(uint32_t freq)
{
  uint32_t acr = FLASH->ACR & ~FLASH_ACR_LATENCY;
  if(freq > 48000000) acr |= FLASH_ACR_LATENCY_3WS;
  else if(freq > 32000000) acr |= FLASH_ACR_LATENCY_2WS;
  else if(freq > 16000000) acr |= FLASH_ACR_LATENCY_1WS;
  else acr |= FLASH_ACR_LATENCY_0WS;
  FLASH->ACR = acr;
}

static void RCC_SetVoltageScaling(uint32_t freq)
{
  uint32_t vos = (freq > 16000000) ? PWR_CR1_VOS_0 : PWR_CR1_VOS_1;
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | vos;
  while(PWR->SR2 & PWR_SR2_VOSF) __NOP();
}

static uint32_t RCC_MSI(uint32_t range_bits, uint32_t freq)
{
  RCC->CR |= RCC_CR_MSION;
  RCC->CR = (RCC->CR & ~RCC_CR_MSIRANGE) | range_bits;
  while(!(RCC->CR & RCC_CR_MSIRDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW);
  while((RCC->CFGR & RCC_CFGR_SWS) != 0) __NOP();
  RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON);
  SystemCoreClock = freq;
  return freq;
}

static uint32_t RCC_HSI16(void)
{
  RCC->CR |= RCC_CR_HSION;
  while(!(RCC->CR & RCC_CR_HSIRDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_0;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_0) __NOP();
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_PLLON);
  SystemCoreClock = 16000000;
  return SystemCoreClock;
}

uint32_t RCC_HSE(uint32_t xtal_value)
{
  RCC->CR |= RCC_CR_HSEON;
  while(!(RCC->CR & RCC_CR_HSERDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_1;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_1) __NOP();
  RCC->CR &= ~RCC_CR_PLLON;
  SystemCoreClock = xtal_value ? xtal_value : SystemCoreClock;
  return SystemCoreClock;
}

uint32_t RCC_PLL(uint32_t hse_xtal_value, uint8_t m, uint8_t n, uint8_t r)
{
  if(m < 1) m = 1; else if(m > 8) m = 8;
  if(n < 8) n = 8; else if(n > 86) n = 86;
  if(r < 2) r = 2; else if(r > 8) r = 8;
  uint32_t freq;
  if(hse_xtal_value) {
    freq = (hse_xtal_value / m) * n / r;
    RCC_HSE(hse_xtal_value);
  }
  else {
    freq = (16000000 / m) * n / r;
    RCC_HSI16();
  }
  RCC_SetVoltageScaling(freq);
  RCC_FLASH_SetLatency(freq);
  RCC->CR &= ~RCC_CR_PLLON;
  while(RCC->CR & RCC_CR_PLLRDY) __NOP();
  RCC->PLLCFGR = (RCC->PLLCFGR & ~(RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLR)) |
                 ((m - 1) << RCC_PLLCFGR_PLLM_Pos) |
                 (n << RCC_PLLCFGR_PLLN_Pos) |
                 ((r - 1) << RCC_PLLCFGR_PLLR_Pos) |
                 RCC_PLLCFGR_PLLREN |
                 (hse_xtal_value ? (RCC_PLLCFGR_PLLSRC_0 | RCC_PLLCFGR_PLLSRC_1) : RCC_PLLCFGR_PLLSRC_1);
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (RCC_CFGR_SW_0 | RCC_CFGR_SW_1);
  while((RCC->CFGR & RCC_CFGR_SWS) != (RCC_CFGR_SWS_0 | RCC_CFGR_SWS_1)) __NOP();
  SystemCoreClock = freq;
  return freq;
}

uint32_t RCC_2MHz(void)
{
  RCC_SetVoltageScaling(2000000);
  RCC_FLASH_SetLatency(2000000);
  return RCC_MSI(RCC_CR_MSIRANGE_5, 2000000);
}

uint32_t RCC_16MHz(void)
{
  RCC_SetVoltageScaling(16000000);
  RCC_FLASH_SetLatency(16000000);
  return RCC_HSI16();
}

uint32_t RCC_48MHz(void)
{
  return RCC_PLL(false, 2, 12, 2);
}

uint32_t RCC_64MHz(void)
{
  return RCC_PLL(false, 2, 16, 2);
}

//------------------------------------------------------------------------------------------------- PWR

inline void PWR_Reset(void)
{
  NVIC_SystemReset();
}

void PWR_Sleep(PWR_SleepMode_e mode)
{
  const uint8_t PWR_MODE_ARRAY[] = { 0b000, 0b001, 0b011, 0b011, 0b100 };
  if((PWR->SR2 & PWR_SR2_REGLPF) && (mode == PWR_SleepMode_Stop0)) return;
  PWR->CR1 |= PWR_MODE_ARRAY[mode];
  if(mode == PWR_SleepMode_StandbySRAM) PWR->CR3 |= PWR_CR3_RRS;
  else PWR->CR3 &= ~PWR_CR3_RRS;
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  PWR->SCR |= PWR_SCR_CLEAR;
  __SEV(); __WFE(); __WFE();
}

void PWR_Wakeup(PWR_WakeupPin_e wakeup_pin, PWR_WakeupDir_e dir)
{
  PWR->CR3 |= PWR_CR3_EIWUL | (1U << wakeup_pin);
  PWR->CR4 |= (dir << wakeup_pin);
}

//------------------------------------------------------------------------------------------------- BKPR

void BKPR_Write(BKPR_e reg, uint32_t value)
{
  RTC->BKP0R[reg] = value;
}

uint32_t BKPR_Read(BKPR_e reg)
{
  return RTC->BKP0R[reg];
}

//------------------------------------------------------------------------------------------------- IWDG

inline void IWDG_Init(IWDG_Time_e time, uint16_t reload_counter)
{
  if(reload_counter > 0x0FFF) reload_counter = 0x0FFF;
  RCC->CSR |= RCC_CSR_LSION;
  while(!(RCC->CSR & RCC_CSR_LSIRDY)) __NOP();
  IWDG->KR = IWDG_START;
  IWDG->KR = IWDG_WRITE_ACCESS;
  IWDG->PR = (uint32_t)time;
  IWDG->RLR = reload_counter;
  while(IWDG->SR) __NOP();
  IWDG->KR = IWDG_REFRESH;
}

inline void IWDG_Refresh(void)
{
  IWDG->KR = IWDG_REFRESH;
}

bool IWDG_Status(void)
{
  if(RCC->CSR & RCC_CSR_IWDGRSTF) {
    RCC->CSR |= RCC_CSR_RMVF;
    return true;
  }
  return false;
}

//-------------------------------------------------------------------------------------------------
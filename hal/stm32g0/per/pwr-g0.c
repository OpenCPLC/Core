#include "pwr.h"

//------------------------------------------------------------------------------------------------- RAMP

void RAMP_PA11_PA12(void)
{
  RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;
  SYSCFG->CFGR1 |= SYSCFG_CFGR1_PA12_RMP | SYSCFG_CFGR1_PA11_RMP;
}

//------------------------------------------------------------------------------------------------- Clock Enable

void RCC_EnableTIM(TIM_TypeDef *tim_typedef)
{
  switch((uint32_t)tim_typedef) {
    case (uint32_t)TIM2:  RCC->APBENR1 |= RCC_APBENR1_TIM2EN; break;
    case (uint32_t)TIM3:  RCC->APBENR1 |= RCC_APBENR1_TIM3EN; break;
    case (uint32_t)TIM6:  RCC->APBENR1 |= RCC_APBENR1_TIM6EN; break;
    case (uint32_t)TIM7:  RCC->APBENR1 |= RCC_APBENR1_TIM7EN; break;
    case (uint32_t)TIM1:  RCC->APBENR2 |= RCC_APBENR2_TIM1EN; break;
    case (uint32_t)TIM14: RCC->APBENR2 |= RCC_APBENR2_TIM14EN; break;
    case (uint32_t)TIM15: RCC->APBENR2 |= RCC_APBENR2_TIM15EN; break;
    case (uint32_t)TIM16: RCC->APBENR2 |= RCC_APBENR2_TIM16EN; break;
    case (uint32_t)TIM17: RCC->APBENR2 |= RCC_APBENR2_TIM17EN; break;
    default: break;
  }
}

void RCC_EnableGPIO(GPIO_TypeDef *gpio_typedef)
{
  switch((uint32_t)gpio_typedef) {
    case (uint32_t)GPIOA: RCC->IOPENR |= RCC_IOPSMENR_GPIOASMEN; break;
    case (uint32_t)GPIOB: RCC->IOPENR |= RCC_IOPSMENR_GPIOBSMEN; break;
    case (uint32_t)GPIOC: RCC->IOPENR |= RCC_IOPSMENR_GPIOCSMEN; break;
    case (uint32_t)GPIOD: RCC->IOPENR |= RCC_IOPSMENR_GPIODSMEN; break;
    case (uint32_t)GPIOF: RCC->IOPENR |= RCC_IOPSMENR_GPIOFSMEN; break;
    default: break;
  }
}

void RCC_EnableUART(USART_TypeDef *uart_typedef)
{
  switch((uint32_t)uart_typedef) {
    case (uint32_t)USART1:  RCC->APBENR2 |= RCC_APBENR2_USART1EN; break;
    case (uint32_t)USART2:  RCC->APBENR1 |= RCC_APBENR1_USART2EN; break;
    case (uint32_t)USART3:  RCC->APBENR1 |= RCC_APBENR1_USART3EN; break;
    case (uint32_t)USART4:  RCC->APBENR1 |= RCC_APBENR1_USART4EN; break;
    case (uint32_t)LPUART1: RCC->APBENR1 |= RCC_APBENR1_LPUART1EN; break;
    default: break;
  }
}

void RCC_DisableUART(USART_TypeDef *uart_typedef)
{
  switch((uint32_t)uart_typedef) {
    case (uint32_t)USART1:  RCC->APBENR2 &= ~RCC_APBENR2_USART1EN; break;
    case (uint32_t)USART2:  RCC->APBENR1 &= ~RCC_APBENR1_USART2EN; break;
    case (uint32_t)USART3:  RCC->APBENR1 &= ~RCC_APBENR1_USART3EN; break;
    case (uint32_t)USART4:  RCC->APBENR1 &= ~RCC_APBENR1_USART4EN; break;
    case (uint32_t)LPUART1: RCC->APBENR1 &= ~RCC_APBENR1_LPUART1EN; break;
    default: break;
  }
}

void RCC_EnableI2C(I2C_TypeDef *i2c_typedef)
{
  switch((uint32_t)i2c_typedef) {
    case (uint32_t)I2C1: RCC->APBENR1 |= RCC_APBENR1_I2C1EN; break;
    case (uint32_t)I2C2: RCC->APBENR1 |= RCC_APBENR1_I2C2EN; break;
    default: break;
  }
}

void RCC_DisableI2C(I2C_TypeDef *i2c_typedef)
{
  switch((uint32_t)i2c_typedef) {
    case (uint32_t)I2C1: RCC->APBENR1 &= ~RCC_APBENR1_I2C1EN; break;
    case (uint32_t)I2C2: RCC->APBENR1 &= ~RCC_APBENR1_I2C2EN; break;
    default: break;
  }
}

void RCC_EnableSPI(SPI_TypeDef *spi_typedef)
{
  switch((uint32_t)spi_typedef) {
    case (uint32_t)SPI1: RCC->APBENR2 |= RCC_APBENR2_SPI1EN; break;
    case (uint32_t)SPI2: RCC->APBENR1 |= RCC_APBENR1_SPI2EN; break;
    default: break;
  }
}

void RCC_EnableDMA(DMA_TypeDef *dam_typedef)
{
  switch((uint32_t)dam_typedef) {
    case (uint32_t)DMA1: RCC->AHBENR |= RCC_AHBENR_DMA1EN; break;
    #ifdef STM32G0C1xx
      case (uint32_t)DMA2: RCC->AHBENR |= RCC_AHBENR_DMA2EN; break;
    #endif
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- RCC

static uint32_t RCC_HSI(RCC_HSI_e frequency)
{
  RCC->CR = (RCC->CR & ~RCC_CR_HSIDIV_Msk) | (frequency << RCC_CR_HSIDIV_Pos) | RCC_CR_HSION;
  while(!(RCC->CR & RCC_CR_HSIRDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW);
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSISYS) __NOP();
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_PLLON);
  SystemCoreClockUpdate();
  return SystemCoreClock;
}

static void RCC_FLASH_SetLatency(uint32_t freq)
{
  if(freq >= 48000000) FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_2;
  else if(freq >= 24000000) FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_1;
  else FLASH->ACR &= ~FLASH_ACR_LATENCY;
}

uint32_t RCC_HSE(uint32_t xtal_value)
{
  RCC->CR |= RCC_CR_HSEON;
  while(!(RCC->CR & RCC_CR_HSERDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_0;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE) __NOP();
  RCC->CR &= ~RCC_CR_PLLON;
  SystemCoreClock = xtal_value;
  return xtal_value;
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
    RCC_HSI(RCC_HSI_16MHz);
  }
  RCC_FLASH_SetLatency(freq);
  while((RCC->CR & RCC_CR_PLLRDY)) __NOP();
  RCC->PLLCFGR = (RCC->PLLCFGR & ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLR | RCC_PLLCFGR_PLLSRC)) |
                 ((m - 1) << RCC_PLLCFGR_PLLM_Pos) |
                 (n << RCC_PLLCFGR_PLLN_Pos) |
                 (((r / 2) - 1) << RCC_PLLCFGR_PLLR_Pos) |
                 RCC_PLLCFGR_PLLREN |
                 (hse_xtal_value ? RCC_PLLCFGR_PLLSRC_HSE : RCC_PLLCFGR_PLLSRC_HSI);
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY)) __NOP();
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_1;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLLRCLK) __NOP();
  SystemCoreClock = freq;
  return freq;
}

uint32_t RCC_2MHz(void)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  RCC_HSI(RCC_HSI_2MHz);
  PWR->CR1 |= PWR_CR1_LPR;
  while(!(PWR->SR2 & PWR_SR2_REGLPF_Msk)) __NOP();
  return SystemCoreClock;
}

uint32_t RCC_16MHz(void)
{
  RCC_FLASH_SetLatency(16000000);
  return RCC_HSI(RCC_HSI_16MHz);
}

uint32_t RCC_48MHz(void)
{
  RCC_FLASH_SetLatency(48000000);
  return RCC_PLL(false, 2, 12, 2);
}

uint32_t RCC_64MHz(void)
{
  RCC_FLASH_SetLatency(64000000);
  return RCC_PLL(false, 2, 16, 2);
}

//------------------------------------------------------------------------------------------------- PWR

inline void PWR_Reset(void)
{
  NVIC_SystemReset();
}

void PWR_Sleep(PWR_SleepMode_e mode)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
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
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  PWR->CR3 |= PWR_CR3_EIWUL | (1U << wakeup_pin);
  PWR->CR4 |= (dir << wakeup_pin);
}

//------------------------------------------------------------------------------------------------- BKPR

void BKPR_Write(BKPR_e reg, uint32_t value)
{
  RTC->CR |= PWR_CR1_DBP;
  *((uint32_t *)(TAMP_BASE + 0x100u + (4u * reg))) = value;
  RTC->CR &= ~PWR_CR1_DBP;
}

uint32_t BKPR_Read(BKPR_e reg)
{
  return *((uint32_t *)(TAMP_BASE + 0x100u + (4u * reg)));
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


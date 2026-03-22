// hal/stm32g0/sys/pwr_g0.c

#include "pwr.h"

#define IWDG_KEY_REFRESH 0xAAAA
#define IWDG_KEY_ACCESS  0x5555
#define IWDG_KEY_START   0xCCCC

//------------------------------------------------------------------------------------------------- Platform-specific

void RAMP_PA11_PA12(void)
{
  RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;
  SYSCFG->CFGR1 |= SYSCFG_CFGR1_PA12_RMP | SYSCFG_CFGR1_PA11_RMP;
}

//------------------------------------------------------------------------------------------------- RCC: Clock Enable

void RCC_EnableTIM(void *tim)
{
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:  RCC->APBENR2 |= RCC_APBENR2_TIM1EN; break;
    case (uint32_t)TIM2:  RCC->APBENR1 |= RCC_APBENR1_TIM2EN; break;
    case (uint32_t)TIM3:  RCC->APBENR1 |= RCC_APBENR1_TIM3EN; break;
    case (uint32_t)TIM6:  RCC->APBENR1 |= RCC_APBENR1_TIM6EN; break;
    case (uint32_t)TIM7:  RCC->APBENR1 |= RCC_APBENR1_TIM7EN; break;
    case (uint32_t)TIM14: RCC->APBENR2 |= RCC_APBENR2_TIM14EN; break;
    case (uint32_t)TIM15: RCC->APBENR2 |= RCC_APBENR2_TIM15EN; break;
    case (uint32_t)TIM16: RCC->APBENR2 |= RCC_APBENR2_TIM16EN; break;
    case (uint32_t)TIM17: RCC->APBENR2 |= RCC_APBENR2_TIM17EN; break;
  }
}

void RCC_EnableGPIO(void *gpio)
{
  switch((uint32_t)gpio) {
    case (uint32_t)GPIOA: RCC->IOPENR |= RCC_IOPENR_GPIOAEN; break;
    case (uint32_t)GPIOB: RCC->IOPENR |= RCC_IOPENR_GPIOBEN; break;
    case (uint32_t)GPIOC: RCC->IOPENR |= RCC_IOPENR_GPIOCEN; break;
    case (uint32_t)GPIOD: RCC->IOPENR |= RCC_IOPENR_GPIODEN; break;
    case (uint32_t)GPIOF: RCC->IOPENR |= RCC_IOPENR_GPIOFEN; break;
  }
}

void RCC_EnableUART(void *uart)
{
  switch((uint32_t)uart) {
    case (uint32_t)USART1:  RCC->APBENR2 |= RCC_APBENR2_USART1EN; break;
    case (uint32_t)USART2:  RCC->APBENR1 |= RCC_APBENR1_USART2EN; break;
    case (uint32_t)USART3:  RCC->APBENR1 |= RCC_APBENR1_USART3EN; break;
    case (uint32_t)USART4:  RCC->APBENR1 |= RCC_APBENR1_USART4EN; break;
    case (uint32_t)LPUART1: RCC->APBENR1 |= RCC_APBENR1_LPUART1EN; break;
  }
}

void RCC_DisableUART(void *uart)
{
  switch((uint32_t)uart) {
    case (uint32_t)USART1:  RCC->APBENR2 &= ~RCC_APBENR2_USART1EN; break;
    case (uint32_t)USART2:  RCC->APBENR1 &= ~RCC_APBENR1_USART2EN; break;
    case (uint32_t)USART3:  RCC->APBENR1 &= ~RCC_APBENR1_USART3EN; break;
    case (uint32_t)USART4:  RCC->APBENR1 &= ~RCC_APBENR1_USART4EN; break;
    case (uint32_t)LPUART1: RCC->APBENR1 &= ~RCC_APBENR1_LPUART1EN; break;
  }
}

void RCC_EnableI2C(void *i2c)
{
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1: RCC->APBENR1 |= RCC_APBENR1_I2C1EN; break;
    case (uint32_t)I2C2: RCC->APBENR1 |= RCC_APBENR1_I2C2EN; break;
  }
}

void RCC_DisableI2C(void *i2c)
{
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1: RCC->APBENR1 &= ~RCC_APBENR1_I2C1EN; break;
    case (uint32_t)I2C2: RCC->APBENR1 &= ~RCC_APBENR1_I2C2EN; break;
  }
}

void RCC_EnableSPI(void *spi)
{
  switch((uint32_t)spi) {
    case (uint32_t)SPI1: RCC->APBENR2 |= RCC_APBENR2_SPI1EN; break;
    case (uint32_t)SPI2: RCC->APBENR1 |= RCC_APBENR1_SPI2EN; break;
  }
}

void RCC_EnableDMA(void *dma)
{
  switch((uint32_t)dma) {
    case (uint32_t)DMA1: RCC->AHBENR |= RCC_AHBENR_DMA1EN; break;
    #ifdef DMA2
      case (uint32_t)DMA2: RCC->AHBENR |= RCC_AHBENR_DMA2EN; break;
    #endif
  }
}

//------------------------------------------------------------------------------------------------- RCC: System Clock

uint32_t RCC_GetClock(void) { return SystemCoreClock; }

static void RCC_SetFlashLatency(uint32_t freq_Hz)
{
  uint32_t latency;
  if(freq_Hz > 48000000) latency = FLASH_ACR_LATENCY_2;
  else if(freq_Hz > 24000000) latency = FLASH_ACR_LATENCY_1;
  else latency = FLASH_ACR_LATENCY_0;
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | latency;
}

static uint32_t RCC_SetHSI(uint8_t div)
{
  if(div > 7) div = 7;
  RCC->CR = (RCC->CR & ~RCC_CR_HSIDIV) | (div << RCC_CR_HSIDIV_Pos) | RCC_CR_HSION;
  while(!(RCC->CR & RCC_CR_HSIRDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW);
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSISYS);
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_PLLON);
  SystemCoreClockUpdate();
  return SystemCoreClock;
}

uint32_t RCC_SetHSE(uint32_t xtal_Hz)
{
  RCC->CR |= RCC_CR_HSEON;
  while(!(RCC->CR & RCC_CR_HSERDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_0;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);
  RCC->CR &= ~RCC_CR_PLLON;
  SystemCoreClock = xtal_Hz;
  return xtal_Hz;
}

uint32_t RCC_SetPLL(uint32_t hse_Hz, uint8_t m, uint8_t n, uint8_t r)
{
  if(m < 1) m = 1; else if(m > 8) m = 8;
  if(n < 8) n = 8; else if(n > 86) n = 86;
  if(r < 2) r = 2; else if(r > 8) r = 8;
  uint32_t freq_Hz;
  if(hse_Hz) {
    freq_Hz = (hse_Hz / m) * n / r;
    RCC_SetHSE(hse_Hz);
  }
  else {
    freq_Hz = (16000000 / m) * n / r;
    RCC_SetHSI(0);
  }
  RCC_SetFlashLatency(freq_Hz);
  RCC->CR &= ~RCC_CR_PLLON;
  while(RCC->CR & RCC_CR_PLLRDY);
  RCC->PLLCFGR = ((m - 1) << RCC_PLLCFGR_PLLM_Pos) |
                 (n << RCC_PLLCFGR_PLLN_Pos) |
                 (((r / 2) - 1) << RCC_PLLCFGR_PLLR_Pos) |
                 RCC_PLLCFGR_PLLREN |
                 (hse_Hz ? RCC_PLLCFGR_PLLSRC_HSE : RCC_PLLCFGR_PLLSRC_HSI);
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_1;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLLRCLK);
  SystemCoreClock = freq_Hz;
  return freq_Hz;
}

uint32_t RCC_2MHz(void)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  RCC_SetFlashLatency(2000000);
  RCC_SetHSI(3);
  PWR->CR1 |= PWR_CR1_LPR;
  while(!(PWR->SR2 & PWR_SR2_REGLPF));
  return SystemCoreClock;
}

uint32_t RCC_16MHz(void)
{
  RCC_SetFlashLatency(16000000);
  return RCC_SetHSI(0);
}

uint32_t RCC_48MHz(void) { return RCC_SetPLL(0, 2, 12, 2); }
uint32_t RCC_64MHz(void) { return RCC_SetPLL(0, 2, 16, 2); }

//------------------------------------------------------------------------------------------------- PWR

void PWR_Reset(void) { NVIC_SystemReset(); }

void PWR_Sleep(PWR_SleepMode_t mode)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  // G0: Stop0=000, Stop1=001, Standby=011, Shutdown=100
  // Stop2 not available on G0, map to Stop1
  static const uint8_t mode_bits[] = { 0b000, 0b001, 0b001, 0b011, 0b011, 0b100 };
  if((PWR->SR2 & PWR_SR2_REGLPF) && (mode == PWR_SleepMode_Stop0)) return;
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | mode_bits[mode];
  if(mode == PWR_SleepMode_StandbySRAM) PWR->CR3 |= PWR_CR3_RRS;
  else PWR->CR3 &= ~PWR_CR3_RRS;
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  PWR->SCR = 0x013F; // Clear all wakeup flags
  __SEV(); __WFE(); __WFE();
}

void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  PWR->CR3 |= PWR_CR3_EIWUL | (1u << pin);
  if(edge == PWR_Edge_Falling) PWR->CR4 |= (1u << pin);
  else PWR->CR4 &= ~(1u << pin);
}

//------------------------------------------------------------------------------------------------- BKPR

void BKPR_Write(BKPR_t reg, uint32_t value)
{
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  PWR->CR1 |= PWR_CR1_DBP;
  // G0: backup registers in TAMP peripheral
  *((volatile uint32_t *)(TAMP_BASE + 0x100u + (4u * reg))) = value;
  PWR->CR1 &= ~PWR_CR1_DBP;
}

uint32_t BKPR_Read(BKPR_t reg)
{
  return *((volatile uint32_t *)(TAMP_BASE + 0x100u + (4u * reg)));
}

//------------------------------------------------------------------------------------------------- IWDG

void IWDG_Init(IWDG_Time_t prescaler, uint16_t reload)
{
  if(reload > 0x0FFF) reload = 0x0FFF;
  RCC->CSR |= RCC_CSR_LSION;
  while(!(RCC->CSR & RCC_CSR_LSIRDY));
  IWDG->KR = IWDG_KEY_START;
  IWDG->KR = IWDG_KEY_ACCESS;
  IWDG->PR = prescaler;
  IWDG->RLR = reload;
  while(IWDG->SR);
  IWDG->KR = IWDG_KEY_REFRESH;
}

void IWDG_Refresh(void) { IWDG->KR = IWDG_KEY_REFRESH; }

bool IWDG_WasReset(void)
{
  if(RCC->CSR & RCC_CSR_IWDGRSTF) {
    RCC->CSR |= RCC_CSR_RMVF;
    return true;
  }
  return false;
}

//-------------------------------------------------------------------------------------------------
// hal/stm32wb/sys/pwr_wb.c

#include "pwr.h"

#define IWDG_KEY_REFRESH 0xAAAA
#define IWDG_KEY_ACCESS  0x5555
#define IWDG_KEY_START   0xCCCC

// WB clock source selection (SW/SWS bits)
#define RCC_SW_MSI  0u
#define RCC_SW_HSI  1u
#define RCC_SW_HSE  2u
#define RCC_SW_PLL  3u

// WB PLL source selection
#define RCC_PLLSRC_NONE 0u
#define RCC_PLLSRC_MSI  1u
#define RCC_PLLSRC_HSI  2u
#define RCC_PLLSRC_HSE  3u

//------------------------------------------------------------------------------------------------- RCC: Clock Enable

void RCC_EnableTIM(void *tim)
{
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:   RCC->APB2ENR  |= RCC_APB2ENR_TIM1EN; break;
    case (uint32_t)TIM2:   RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN; break;
    case (uint32_t)TIM16:  RCC->APB2ENR  |= RCC_APB2ENR_TIM16EN; break;
    case (uint32_t)TIM17:  RCC->APB2ENR  |= RCC_APB2ENR_TIM17EN; break;
    case (uint32_t)LPTIM1: RCC->APB1ENR1 |= RCC_APB1ENR1_LPTIM1EN; break;
    case (uint32_t)LPTIM2: RCC->APB1ENR2 |= RCC_APB1ENR2_LPTIM2EN; break;
  }
}

void RCC_EnableGPIO(void *gpio)
{
  switch((uint32_t)gpio) {
    case (uint32_t)GPIOA: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN; break;
    case (uint32_t)GPIOB: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; break;
    case (uint32_t)GPIOC: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN; break;
    case (uint32_t)GPIOD: RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN; break;
    case (uint32_t)GPIOE: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN; break;
    case (uint32_t)GPIOH: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOHEN; break;
  }
}

void RCC_EnableUART(void *uart)
{
  switch((uint32_t)uart) {
    case (uint32_t)USART1:  RCC->APB2ENR  |= RCC_APB2ENR_USART1EN; break;
    case (uint32_t)LPUART1: RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN; break;
  }
}

void RCC_DisableUART(void *uart)
{
  switch((uint32_t)uart) {
    case (uint32_t)USART1:  RCC->APB2ENR  &= ~RCC_APB2ENR_USART1EN; break;
    case (uint32_t)LPUART1: RCC->APB1ENR2 &= ~RCC_APB1ENR2_LPUART1EN; break;
  }
}

void RCC_EnableI2C(void *i2c)
{
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1: RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN; break;
    case (uint32_t)I2C3: RCC->APB1ENR1 |= RCC_APB1ENR1_I2C3EN; break;
  }
}

void RCC_DisableI2C(void *i2c)
{
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1: RCC->APB1ENR1 &= ~RCC_APB1ENR1_I2C1EN; break;
    case (uint32_t)I2C3: RCC->APB1ENR1 &= ~RCC_APB1ENR1_I2C3EN; break;
  }
}

void RCC_EnableSPI(void *spi)
{
  switch((uint32_t)spi) {
    case (uint32_t)SPI1: RCC->APB2ENR  |= RCC_APB2ENR_SPI1EN; break;
    case (uint32_t)SPI2: RCC->APB1ENR1 |= RCC_APB1ENR1_SPI2EN; break;
  }
}

void RCC_EnableDMA(void *dma)
{
  switch((uint32_t)dma) {
    case (uint32_t)DMA1: RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; break;
    case (uint32_t)DMA2: RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; break;
  }
  RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;
}

//------------------------------------------------------------------------------------------------- RCC: System Clock

uint32_t RCC_GetClock(void) { return SystemCoreClock; }

static void RCC_SetFlashLatency(uint32_t freq_Hz)
{
  uint32_t latency;
  if(freq_Hz > 48000000) latency = FLASH_ACR_LATENCY_3WS;
  else if(freq_Hz > 32000000) latency = FLASH_ACR_LATENCY_2WS;
  else if(freq_Hz > 16000000) latency = FLASH_ACR_LATENCY_1WS;
  else latency = FLASH_ACR_LATENCY_0WS;
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | latency;
}

static void RCC_SetVoltageScale(uint32_t freq_Hz)
{
  // WB: PWR is always accessible (no enable bit)
  uint32_t vos = (freq_Hz > 16000000) ? PWR_CR1_VOS_0 : PWR_CR1_VOS_1;
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | vos;
  while(PWR->SR2 & PWR_SR2_VOSF);
}

static uint32_t RCC_SetHSI16(void)
{
  RCC->CR |= RCC_CR_HSION;
  while(!(RCC->CR & RCC_CR_HSIRDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (RCC_SW_HSI << RCC_CFGR_SW_Pos);
  while(((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos) != RCC_SW_HSI);
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_PLLON);
  SystemCoreClock = 16000000;
  return SystemCoreClock;
}

static uint32_t RCC_SetMSI(uint32_t range, uint32_t freq_Hz)
{
  RCC->CR |= RCC_CR_MSION;
  RCC->CR = (RCC->CR & ~RCC_CR_MSIRANGE) | range;
  while(!(RCC->CR & RCC_CR_MSIRDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (RCC_SW_MSI << RCC_CFGR_SW_Pos);
  while(((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos) != RCC_SW_MSI);
  RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON);
  SystemCoreClock = freq_Hz;
  return freq_Hz;
}

uint32_t RCC_SetHSE(uint32_t xtal_Hz)
{
  RCC->CR |= RCC_CR_HSEON;
  while(!(RCC->CR & RCC_CR_HSERDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (RCC_SW_HSE << RCC_CFGR_SW_Pos);
  while(((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos) != RCC_SW_HSE);
  RCC->CR &= ~RCC_CR_PLLON;
  SystemCoreClock = xtal_Hz ? xtal_Hz : SystemCoreClock;
  return SystemCoreClock;
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
    RCC_SetHSI16();
  }
  RCC_SetVoltageScale(freq_Hz);
  RCC_SetFlashLatency(freq_Hz);
  RCC->CR &= ~RCC_CR_PLLON;
  while(RCC->CR & RCC_CR_PLLRDY);
  RCC->PLLCFGR = ((m - 1) << RCC_PLLCFGR_PLLM_Pos) |
                 (n << RCC_PLLCFGR_PLLN_Pos) |
                 ((r - 1) << RCC_PLLCFGR_PLLR_Pos) |
                 RCC_PLLCFGR_PLLREN |
                 ((hse_Hz ? RCC_PLLSRC_HSE : RCC_PLLSRC_HSI) << RCC_PLLCFGR_PLLSRC_Pos);
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY));
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (RCC_SW_PLL << RCC_CFGR_SW_Pos);
  while(((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos) != RCC_SW_PLL);
  SystemCoreClock = freq_Hz;
  return freq_Hz;
}

uint32_t RCC_2MHz(void)
{
  RCC_SetVoltageScale(2000000);
  RCC_SetFlashLatency(2000000);
  return RCC_SetMSI(RCC_CR_MSIRANGE_5, 2000000);
}

uint32_t RCC_16MHz(void)
{
  RCC_SetVoltageScale(16000000);
  RCC_SetFlashLatency(16000000);
  return RCC_SetHSI16();
}

uint32_t RCC_48MHz(void) { return RCC_SetPLL(0, 2, 12, 2); }
uint32_t RCC_64MHz(void) { return RCC_SetPLL(0, 2, 16, 2); }

//------------------------------------------------------------------------------------------------- PWR

void PWR_Reset(void) { NVIC_SystemReset(); }

void PWR_Sleep(PWR_SleepMode_t mode)
{
  // WB: PWR is always accessible (no enable bit)
  // WB: Stop0=000, Stop1=001, Stop2=010, Standby=011, Shutdown=100
  static const uint8_t mode_bits[] = { 0b000, 0b001, 0b010, 0b011, 0b011, 0b100 };
  if((PWR->SR2 & PWR_SR2_REGLPF) && (mode == PWR_SleepMode_Stop0)) return;
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | mode_bits[mode];
  if(mode == PWR_SleepMode_StandbySRAM) PWR->CR3 |= PWR_CR3_RRS;
  else PWR->CR3 &= ~PWR_CR3_RRS;
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  PWR->SCR = 0x001F; // Clear wakeup flags (CWUF1-5)
  __SEV(); __WFE(); __WFE();
}

void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge)
{
  // WB: PWR is always accessible (no enable bit)
  PWR->CR3 |= PWR_CR3_EIWUL | (1u << pin);
  if(edge == PWR_Edge_Falling) PWR->CR4 |= (1u << pin);
  else PWR->CR4 &= ~(1u << pin);
}

//------------------------------------------------------------------------------------------------- BKPR

void BKPR_Write(BKPR_t reg, uint32_t value)
{
  // WB: PWR is always accessible (no enable bit)
  RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;
  RCC->BDCR |= RCC_BDCR_RTCEN;
  PWR->CR1 |= PWR_CR1_DBP;
  while(!(PWR->CR1 & PWR_CR1_DBP));
  // WB: backup registers in RTC peripheral
  (&RTC->BKP0R)[reg] = value;
  PWR->CR1 &= ~PWR_CR1_DBP;
}

uint32_t BKPR_Read(BKPR_t reg)
{
  RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;
  return (&RTC->BKP0R)[reg];
}

//------------------------------------------------------------------------------------------------- IWDG

void IWDG_Init(IWDG_Prescaler_t prescaler, uint16_t reload)
{
  if(reload > 0x0FFF) reload = 0x0FFF;
  // WB uses LSI1 (not LSI like G0)
  RCC->CSR |= RCC_CSR_LSI1ON;
  while(!(RCC->CSR & RCC_CSR_LSI1RDY));
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
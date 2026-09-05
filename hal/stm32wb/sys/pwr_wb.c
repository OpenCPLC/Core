// hal/stm32wb/sys/pwr_wb.c

#include "pwr.h"

#include "hsem_wb.h"

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

//------------------------------------------------------------------------------- RCC: Clock Enable

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

void RCC_EnableUSB(void)
{
  // The CPU2 stack stops HSI48 once its RNG runs dry; holding the CLK48 semaphore
  // for the whole USB lifetime keeps the clock in CPU1 hands.
  HSEM_Wait(HSEM_CLK48);
  // `CRS_CFGR` reset selects USB SOF with `48 MHz` reload.
  RCC->CRRCR |= RCC_CRRCR_HSI48ON;
  while(!(RCC->CRRCR & RCC_CRRCR_HSI48RDY));
  RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL; // `00` = `HSI48`
  RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN | RCC_APB1ENR1_USBEN;
  CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
  PWR->CR2 |= PWR_CR2_USV; // `VDDUSB` valid, isolation off
}

//------------------------------------------------------------------------------- RCC: System Clock

uint32_t RCC_GetClock(void) { return SystemCoreClock; }

// Wait states are raised before the clock and lowered after it
static void RCC_SetFlashLatency(uint32_t freq_Hz)
{
  uint32_t latency;
  if(freq_Hz > 48000000) latency = FLASH_ACR_LATENCY_3WS;
  else if(freq_Hz > 32000000) latency = FLASH_ACR_LATENCY_2WS;
  else if(freq_Hz > 16000000) latency = FLASH_ACR_LATENCY_1WS;
  else latency = FLASH_ACR_LATENCY_0WS;
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | latency;
  // Field must read back before the clock rises
  while((FLASH->ACR & FLASH_ACR_LATENCY) != latency);
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
  uint32_t freq_Hz = RCC_SetMSI(RCC_CR_MSIRANGE_5, 2000000);
  RCC_SetFlashLatency(2000000);
  // Voltage scale last, a lower range caps the frequency
  RCC_SetVoltageScale(2000000);
  return freq_Hz;
}

uint32_t RCC_16MHz(void)
{
  uint32_t freq_Hz = RCC_SetHSI16();
  RCC_SetFlashLatency(16000000);
  RCC_SetVoltageScale(16000000);
  return freq_Hz;
}

uint32_t RCC_48MHz(void) { return RCC_SetPLL(0, 2, 12, 2); }
uint32_t RCC_64MHz(void) { return RCC_SetPLL(0, 2, 16, 2); }

//--------------------------------------------------------------------------------------------- PWR

void PWR_Reset(void) { NVIC_SystemReset(); }

void PWR_Sleep(PWR_SleepMode_t mode)
{
  // WB: PWR is always accessible (no enable bit)
  // WB: Stop0=000, Stop1=001, Stop2=010, Standby=011, Shutdown=100
  static const uint8_t mode_bits[] = { 0b000, 0b001, 0b010, 0b011, 0b011, 0b100 };
  // `PWR_SleepMode_Error` names a wakeup cause, not a mode, and sits past the table.
  if(mode >= PWR_SleepMode_Error) return;
  if((PWR->SR2 & PWR_SR2_REGLPF) && (mode == PWR_SleepMode_Stop0)) return;
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | mode_bits[mode];
  if(mode == PWR_SleepMode_StandbySRAM) PWR->CR3 |= PWR_CR3_RRS;
  else PWR->CR3 &= ~PWR_CR3_RRS;
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  PWR->SCR = 0x001F; // Clear wakeup flags (CWUF1-5)
  __SEV(); __WFE(); __WFE();
  // `SLEEPDEEP` must not outlive the call: a later `__WFI` would enter Stop
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge)
{
  // WB: PWR is always accessible (no enable bit)
  PWR->CR3 |= PWR_CR3_EIWUL | (1u << pin);
  if(edge == PWR_Edge_Falling) PWR->CR4 |= (1u << pin);
  else PWR->CR4 &= ~(1u << pin);
}

//-------------------------------------------------------------------------------------------- BKPR

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

void BKP_DomainReset(void)
{
  if(!BOR_WasReset()) return; // domain can only be corrupted by a power-on.
  // WB: PWR is always accessible (no enable bit)
  PWR->CR1 |= PWR_CR1_DBP;
  while(!(PWR->CR1 & PWR_CR1_DBP));
  RCC->BDCR = RCC_BDCR_BDRST; // set `BDRST`, clear `LSCO`/`LSE`/`RTCSEL`
  (void)RCC->BDCR; // read back lengthens reset pulse
  RCC->BDCR = 0; // release reset
}

//-------------------------------------------------------------------------------------------- IWDG

void IWDG_Init(IWDG_Time_t prescaler, uint16_t reload)
{
  if(reload > 0x0FFF) reload = 0x0FFF;
  // A halted core would starve the dog: pause it whenever the debugger holds the CPU
  DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP;
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

// The smallest tick that covers the timeout, so the resolution stays the finest possible
void IWDG_Init_ms(uint32_t timeout_ms)
{
  uint8_t time = IWDG_Time_125us;
  uint32_t reload;
  while((reload = (timeout_ms * 8) >> time) > 0x0FFF && time < IWDG_Time_8ms) time++;
  if(!reload) reload = 1;
  IWDG_Init((IWDG_Time_t)time, (uint16_t)reload);
}


// `RMVF` clears all reset flags at once: latch on first read so `IWDG_WasReset`
// and `BOR_WasReset` do not clobber each other.
static uint32_t rst_csr;
static bool rst_latched;

static uint32_t rst_flags(void)
{
  if(!rst_latched) {
    rst_csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    rst_latched = true;
  }
  return rst_csr;
}

bool IWDG_WasReset(void) { return (rst_flags() & RCC_CSR_IWDGRSTF) != 0; }

void PWR_SetBOR(PWR_BOR_t level)
{
  uint32_t want = (uint32_t)level << FLASH_OPTR_BOR_LEV_Pos;
  if((FLASH->OPTR & FLASH_OPTR_BOR_LEV) == want) return;
  while(FLASH->SR & FLASH_SR_BSY);
  if(FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
  }
  if(FLASH->CR & FLASH_CR_OPTLOCK) {
    FLASH->OPTKEYR = 0x08192A3Bu;
    FLASH->OPTKEYR = 0x4C5D6E7Fu;
  }
  FLASH->OPTR = (FLASH->OPTR & ~FLASH_OPTR_BOR_LEV) | want;
  FLASH->CR |= FLASH_CR_OPTSTRT;
  while(FLASH->SR & FLASH_SR_BSY);
  FLASH->CR |= FLASH_CR_OBL_LAUNCH; // reload the option bytes, a system reset
  while(1);
}

status_t PWR_Shutdown(uint8_t wakeup_mask, uint8_t falling_mask)
{
  // AN5289 gives ownership of C2CR1 to CPU2 once C2BOOT is set. HCI reset only stops
  // radio activity; it does not stop CPU2, whose Stop mode would keep LSI/IWDG alive.
  // The caller must reset first and arrive here while CPU2 is still held.
  if(PWR->CR4 & PWR_CR4_C2BOOT) return ERR;

  // nRST_SHDW=0 deliberately converts Shutdown entry into a low-power security
  // reset. Refuse it here so the boot hand-off cannot become a reset loop.
  if(!(FLASH->OPTR & FLASH_OPTR_nRST_SHDW)) return ERR;

  __disable_irq();
  SysTick->CTRL = 0;
  for(uint8_t i = 0; i < 2; i++) {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }
  // The thread switch and the tick are system handlers, out of reach of the controller:
  // either one left pending turns `WFI` into a plain instruction
  SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
  // Configure CPU2's reset-state selection before it is ever booted. Otherwise its
  // Stop0 reset value caps the system at Stop0 even though CPU1 asks for Shutdown.
  PWR->C2CR1 = (PWR->C2CR1 & ~PWR_C2CR1_LPMS) | PWR_C2CR1_LPMS_2;

  // Disable first and clear on both sides of the polarity/enable update. This avoids
  // carrying a wake flag raised while the pin configuration was changing.
  PWR->CR3 &= ~PWR_CR3_EWUP;
  PWR->SCR = PWR_SCR_CWUF;
  PWR->CR4 = (PWR->CR4 & ~0x1Fu) | (falling_mask & 0x1Fu);
  PWR->CR3 = (PWR->CR3 & ~PWR_CR3_EWUP) | (wakeup_mask & 0x1Fu);
  PWR->SCR = PWR_SCR_CWUF;

  // A probe may have left low-power debug enabled. On STM32WB that keeps enough of
  // the domain alive for IWDG to run, producing the misleading "Shutdown then reset".
  DBGMCU->CR &= ~(DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);

  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | PWR_CR1_LPMS_2;
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  // WFI is allowed to act as a NOP on a simultaneous debug/wakeup request. Shutdown
  // is terminal, so never fall through into half-torn-down application code.
  while(1) {
    __DSB();
    __WFI();
  }
}


//--------------------------------------------------------------------------------------------- BOR

#define FLASH_KEY1    0x45670123u
#define FLASH_KEY2    0xCDEF89ABu
#define FLASH_OPTKEY1 0x08192A3Bu
#define FLASH_OPTKEY2 0x4C5D6E7Fu

BOR_Level_t BOR_GetLevel(void)
{
  // WB: `BOR_LEV` field maps 1:1 to `BOR_Level_t`.
  return (BOR_Level_t)((FLASH->OPTR & FLASH_OPTR_BOR_LEV) >> FLASH_OPTR_BOR_LEV_Pos);
}

status_t BOR_SetLevel(BOR_Level_t level)
{
  if(level > BOR_Level_2V8) level = BOR_Level_2V8;
  if(BOR_GetLevel() == level) return OK; // already set: no flash wear, no reset
  while(FLASH->SR & (FLASH_SR_BSY | FLASH_SR_CFGBSY)) __DSB();
  if(FLASH->SR & FLASH_SR_PESD) return ERR; // `CPU2` (M0+) holds flash for prog/erase.
  // Unlock flash control register
  if(FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    if(FLASH->CR & FLASH_CR_LOCK) return ERR;
  }
  // Unlock option bytes
  if(FLASH->CR & FLASH_CR_OPTLOCK) {
    FLASH->OPTKEYR = FLASH_OPTKEY1;
    FLASH->OPTKEYR = FLASH_OPTKEY2;
    if(FLASH->CR & FLASH_CR_OPTLOCK) return ERR;
  }
  uint32_t optr = FLASH->OPTR & ~FLASH_OPTR_BOR_LEV;
  optr |= (uint32_t)level << FLASH_OPTR_BOR_LEV_Pos;
  FLASH->OPTR = optr;
  FLASH->CR |= FLASH_CR_OPTSTRT;
  while(FLASH->SR & FLASH_SR_BSY) __DSB();
  FLASH->CR |= FLASH_CR_OBL_LAUNCH; // reloads option bytes, resets MCU (no return).
  while(1) __DSB();
}

bool BOR_WasReset(void) { return (rst_flags() & RCC_CSR_BORRSTF) != 0; }

//-------------------------------------------------------------------------------------------------

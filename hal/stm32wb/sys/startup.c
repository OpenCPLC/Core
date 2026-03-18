// hal/stm32wb/sys/startup.c

#include "startup.h"
#define __weak __attribute__((weak))

//------------------------------------------------------------------------------------------------- Linker symbols

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;
extern uint32_t _siMB_MEM2;
extern uint32_t _sMB_MEM2;
extern uint32_t _eMB_MEM2;

extern void __libc_init_array(void);
extern int main(void);

//------------------------------------------------------------------------------------------------- Default handlers

void Reset_Handler(void)
{
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while(dst < &_edata) *dst++ = *src++;
  // BLE stack data
  src = &_siMB_MEM2;
  dst = &_sMB_MEM2;
  while(dst < &_eMB_MEM2) *dst++ = *src++;
  dst = &_sbss;
  while(dst < &_ebss) *dst++ = 0;
  SystemInit();
  __libc_init_array();
  main();
  while(1);
}

void Default_Handler(void) { while(1); }

static void Void_Handler(void *arg) { (void)arg; }

//------------------------------------------------------------------------------------------------- Callbacks: ADC

void (* volatile ADC_Cb)(void *) = Void_Handler;
void * volatile ADC_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: EXTI

void (* volatile EXTI0_Cb)(void *) = Void_Handler;  void * volatile EXTI0_CbArg;
void (* volatile EXTI1_Cb)(void *) = Void_Handler;  void * volatile EXTI1_CbArg;
void (* volatile EXTI2_Cb)(void *) = Void_Handler;  void * volatile EXTI2_CbArg;
void (* volatile EXTI3_Cb)(void *) = Void_Handler;  void * volatile EXTI3_CbArg;
void (* volatile EXTI4_Cb)(void *) = Void_Handler;  void * volatile EXTI4_CbArg;
void (* volatile EXTI5_Cb)(void *) = Void_Handler;  void * volatile EXTI5_CbArg;
void (* volatile EXTI6_Cb)(void *) = Void_Handler;  void * volatile EXTI6_CbArg;
void (* volatile EXTI7_Cb)(void *) = Void_Handler;  void * volatile EXTI7_CbArg;
void (* volatile EXTI8_Cb)(void *) = Void_Handler;  void * volatile EXTI8_CbArg;
void (* volatile EXTI9_Cb)(void *) = Void_Handler;  void * volatile EXTI9_CbArg;
void (* volatile EXTI10_Cb)(void *) = Void_Handler; void * volatile EXTI10_CbArg;
void (* volatile EXTI11_Cb)(void *) = Void_Handler; void * volatile EXTI11_CbArg;
void (* volatile EXTI12_Cb)(void *) = Void_Handler; void * volatile EXTI12_CbArg;
void (* volatile EXTI13_Cb)(void *) = Void_Handler; void * volatile EXTI13_CbArg;
void (* volatile EXTI14_Cb)(void *) = Void_Handler; void * volatile EXTI14_CbArg;
void (* volatile EXTI15_Cb)(void *) = Void_Handler; void * volatile EXTI15_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: DMA1

void (* volatile DMA_CH1_Cb)(void *) = Void_Handler;  void * volatile DMA_CH1_CbArg;
void (* volatile DMA_CH2_Cb)(void *) = Void_Handler;  void * volatile DMA_CH2_CbArg;
void (* volatile DMA_CH3_Cb)(void *) = Void_Handler;  void * volatile DMA_CH3_CbArg;
void (* volatile DMA_CH4_Cb)(void *) = Void_Handler;  void * volatile DMA_CH4_CbArg;
void (* volatile DMA_CH5_Cb)(void *) = Void_Handler;  void * volatile DMA_CH5_CbArg;
void (* volatile DMA_CH6_Cb)(void *) = Void_Handler;  void * volatile DMA_CH6_CbArg;
void (* volatile DMA_CH7_Cb)(void *) = Void_Handler;  void * volatile DMA_CH7_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: DMA2

void (* volatile DMA_CH8_Cb)(void *) = Void_Handler;  void * volatile DMA_CH8_CbArg;
void (* volatile DMA_CH9_Cb)(void *) = Void_Handler;  void * volatile DMA_CH9_CbArg;
void (* volatile DMA_CH10_Cb)(void *) = Void_Handler; void * volatile DMA_CH10_CbArg;
void (* volatile DMA_CH11_Cb)(void *) = Void_Handler; void * volatile DMA_CH11_CbArg;
void (* volatile DMA_CH12_Cb)(void *) = Void_Handler; void * volatile DMA_CH12_CbArg;
void (* volatile DMA_CH13_Cb)(void *) = Void_Handler; void * volatile DMA_CH13_CbArg;
void (* volatile DMA_CH14_Cb)(void *) = Void_Handler; void * volatile DMA_CH14_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: TIM

void (* volatile TIM1_Cb)(void *) = Void_Handler;  void * volatile TIM1_CbArg;
void (* volatile TIM2_Cb)(void *) = Void_Handler;  void * volatile TIM2_CbArg;
void (* volatile TIM16_Cb)(void *) = Void_Handler; void * volatile TIM16_CbArg;
void (* volatile TIM17_Cb)(void *) = Void_Handler; void * volatile TIM17_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: LPTIM

void (* volatile LPTIM1_Cb)(void *) = Void_Handler; void * volatile LPTIM1_CbArg;
void (* volatile LPTIM2_Cb)(void *) = Void_Handler; void * volatile LPTIM2_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: I2C

void (* volatile I2C1_EventCallback)(void *) = Void_Handler; void * volatile I2C1_CbArg;
void (* volatile I2C1_ErrorCallback)(void *) = Void_Handler;
void (* volatile I2C3_EventCallback)(void *) = Void_Handler; void * volatile I2C3_CbArg;
void (* volatile I2C3_ErrorCallback)(void *) = Void_Handler;

//------------------------------------------------------------------------------------------------- Callbacks: SPI

void (* volatile SPI1_Cb)(void *) = Void_Handler; void * volatile SPI1_CbArg;
void (* volatile SPI2_Cb)(void *) = Void_Handler; void * volatile SPI2_CbArg;

//------------------------------------------------------------------------------------------------- Callbacks: UART

void (* volatile USART1_Cb)(void *) = Void_Handler;  void * volatile USART1_CbArg;
void (* volatile LPUART1_Cb)(void *) = Void_Handler; void * volatile LPUART1_CbArg;

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: System

__weak void NMI_Handler(void) { Default_Handler(); }
__weak void HardFault_Handler(void) { while(1); }
__weak void MemManage_Handler(void) { Default_Handler(); }
__weak void BusFault_Handler(void) { Default_Handler(); }
__weak void UsageFault_Handler(void) { Default_Handler(); }
__weak void SVC_Handler(void) { Default_Handler(); }
__weak void DebugMon_Handler(void) { Default_Handler(); }
__weak void PendSV_Handler(void) { Default_Handler(); }
__weak void SysTick_Handler(void) { Default_Handler(); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: Simple

__weak void WWDG_IRQHandler(void) { Default_Handler(); }
__weak void PVD_PVM_IRQHandler(void) { Default_Handler(); }
__weak void TAMP_STAMP_LSECSS_IRQHandler(void) { Default_Handler(); }
__weak void RTC_WKUP_IRQHandler(void) { Default_Handler(); }
__weak void FLASH_IRQHandler(void) { Default_Handler(); }
__weak void RCC_IRQHandler(void) { Default_Handler(); }
__weak void USB_HP_IRQHandler(void) { Default_Handler(); }
__weak void USB_LP_IRQHandler(void) { Default_Handler(); }
__weak void C2SEV_PWR_C2H_IRQHandler(void) { Default_Handler(); }
__weak void COMP_IRQHandler(void) { Default_Handler(); }
__weak void TIM1_BRK_IRQHandler(void) { Default_Handler(); }
__weak void TIM1_CC_IRQHandler(void) { Default_Handler(); }
__weak void PKA_IRQHandler(void) { Default_Handler(); }
__weak void SAI1_IRQHandler(void) { Default_Handler(); }
__weak void TSC_IRQHandler(void) { Default_Handler(); }
__weak void RTC_Alarm_IRQHandler(void) { Default_Handler(); }
__weak void CRS_IRQHandler(void) { Default_Handler(); }
__weak void PWR_SOTF_BLEACT_802ACT_RFPHASE_IRQHandler(void) { Default_Handler(); }
__weak void IPCC_C1_RX_IRQHandler(void) { Default_Handler(); }
__weak void IPCC_C1_TX_IRQHandler(void) { Default_Handler(); }
__weak void HSEM_IRQHandler(void) { Default_Handler(); }
__weak void LCD_IRQHandler(void) { Default_Handler(); }
__weak void QUADSPI_IRQHandler(void) { Default_Handler(); }
__weak void AES1_IRQHandler(void) { Default_Handler(); }
__weak void AES2_IRQHandler(void) { Default_Handler(); }
__weak void RNG_IRQHandler(void) { Default_Handler(); }
__weak void FPU_IRQHandler(void) { Default_Handler(); }
__weak void DMAMUX1_OVR_IRQHandler(void) { Default_Handler(); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: EXTI (individual on WB)

__weak void EXTI0_IRQHandler(void)
{
  EXTI->PR1 = EXTI_PR1_PIF0;
  EXTI0_Cb(EXTI0_CbArg);
}

__weak void EXTI1_IRQHandler(void)
{
  EXTI->PR1 = EXTI_PR1_PIF1;
  EXTI1_Cb(EXTI1_CbArg);
}

__weak void EXTI2_IRQHandler(void)
{
  EXTI->PR1 = EXTI_PR1_PIF2;
  EXTI2_Cb(EXTI2_CbArg);
}

__weak void EXTI3_IRQHandler(void)
{
  EXTI->PR1 = EXTI_PR1_PIF3;
  EXTI3_Cb(EXTI3_CbArg);
}

__weak void EXTI4_IRQHandler(void)
{
  EXTI->PR1 = EXTI_PR1_PIF4;
  EXTI4_Cb(EXTI4_CbArg);
}

__weak void EXTI9_5_IRQHandler(void)
{
  if(EXTI->PR1 & EXTI_PR1_PIF5) { EXTI->PR1 = EXTI_PR1_PIF5; EXTI5_Cb(EXTI5_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF6) { EXTI->PR1 = EXTI_PR1_PIF6; EXTI6_Cb(EXTI6_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF7) { EXTI->PR1 = EXTI_PR1_PIF7; EXTI7_Cb(EXTI7_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF8) { EXTI->PR1 = EXTI_PR1_PIF8; EXTI8_Cb(EXTI8_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF9) { EXTI->PR1 = EXTI_PR1_PIF9; EXTI9_Cb(EXTI9_CbArg); }
}

__weak void EXTI15_10_IRQHandler(void)
{
  if(EXTI->PR1 & EXTI_PR1_PIF10) { EXTI->PR1 = EXTI_PR1_PIF10; EXTI10_Cb(EXTI10_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF11) { EXTI->PR1 = EXTI_PR1_PIF11; EXTI11_Cb(EXTI11_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF12) { EXTI->PR1 = EXTI_PR1_PIF12; EXTI12_Cb(EXTI12_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF13) { EXTI->PR1 = EXTI_PR1_PIF13; EXTI13_Cb(EXTI13_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF14) { EXTI->PR1 = EXTI_PR1_PIF14; EXTI14_Cb(EXTI14_CbArg); }
  if(EXTI->PR1 & EXTI_PR1_PIF15) { EXTI->PR1 = EXTI_PR1_PIF15; EXTI15_Cb(EXTI15_CbArg); }
}

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: DMA1 (individual on WB)

__weak void DMA1_Channel1_IRQHandler(void) { DMA_CH1_Cb(DMA_CH1_CbArg); }
__weak void DMA1_Channel2_IRQHandler(void) { DMA_CH2_Cb(DMA_CH2_CbArg); }
__weak void DMA1_Channel3_IRQHandler(void) { DMA_CH3_Cb(DMA_CH3_CbArg); }
__weak void DMA1_Channel4_IRQHandler(void) { DMA_CH4_Cb(DMA_CH4_CbArg); }
__weak void DMA1_Channel5_IRQHandler(void) { DMA_CH5_Cb(DMA_CH5_CbArg); }
__weak void DMA1_Channel6_IRQHandler(void) { DMA_CH6_Cb(DMA_CH6_CbArg); }
__weak void DMA1_Channel7_IRQHandler(void) { DMA_CH7_Cb(DMA_CH7_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: DMA2 (individual on WB)

__weak void DMA2_Channel1_IRQHandler(void) { DMA_CH8_Cb(DMA_CH8_CbArg); }
__weak void DMA2_Channel2_IRQHandler(void) { DMA_CH9_Cb(DMA_CH9_CbArg); }
__weak void DMA2_Channel3_IRQHandler(void) { DMA_CH10_Cb(DMA_CH10_CbArg); }
__weak void DMA2_Channel4_IRQHandler(void) { DMA_CH11_Cb(DMA_CH11_CbArg); }
__weak void DMA2_Channel5_IRQHandler(void) { DMA_CH12_Cb(DMA_CH12_CbArg); }
__weak void DMA2_Channel6_IRQHandler(void) { DMA_CH13_Cb(DMA_CH13_CbArg); }
__weak void DMA2_Channel7_IRQHandler(void) { DMA_CH14_Cb(DMA_CH14_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: TIM

__weak void TIM1_UP_TIM16_IRQHandler(void)
{
  TIM1_Cb(TIM1_CbArg);
  TIM16_Cb(TIM16_CbArg);
}

__weak void TIM1_TRG_COM_TIM17_IRQHandler(void)
{
  TIM17_Cb(TIM17_CbArg);
}

__weak void TIM2_IRQHandler(void) { TIM2_Cb(TIM2_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: LPTIM

__weak void LPTIM1_IRQHandler(void) { LPTIM1_Cb(LPTIM1_CbArg); }
__weak void LPTIM2_IRQHandler(void) { LPTIM2_Cb(LPTIM2_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: I2C (separate event/error on WB)

__weak void I2C1_EV_IRQHandler(void) { I2C1_EventCallback(I2C1_CbArg); }
__weak void I2C1_ER_IRQHandler(void) { I2C1_ErrorCallback(I2C1_CbArg); }
__weak void I2C3_EV_IRQHandler(void) { I2C3_EventCallback(I2C3_CbArg); }
__weak void I2C3_ER_IRQHandler(void) { I2C3_ErrorCallback(I2C3_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: SPI

__weak void SPI1_IRQHandler(void) { SPI1_Cb(SPI1_CbArg); }
__weak void SPI2_IRQHandler(void) { SPI2_Cb(SPI2_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: UART

__weak void USART1_IRQHandler(void) { USART1_Cb(USART1_CbArg); }
__weak void LPUART1_IRQHandler(void) { LPUART1_Cb(LPUART1_CbArg); }

//------------------------------------------------------------------------------------------------- Weak IRQ handlers: ADC

__weak void ADC1_IRQHandler(void) { ADC_Cb(ADC_CbArg); }

//------------------------------------------------------------------------------------------------- Vector table

__attribute__((section(".isr_vector")))
const void (*const g_pfnVectors[])(void) = {
  (void (*)(void))(&_estack),
  Reset_Handler,
  NMI_Handler,
  HardFault_Handler,
  MemManage_Handler,
  BusFault_Handler,
  UsageFault_Handler,
  0, 0, 0, 0,
  SVC_Handler,
  DebugMon_Handler,
  0,
  PendSV_Handler,
  SysTick_Handler,
  // External interrupts
  WWDG_IRQHandler,                              // 0
  PVD_PVM_IRQHandler,                           // 1
  TAMP_STAMP_LSECSS_IRQHandler,                 // 2
  RTC_WKUP_IRQHandler,                          // 3
  FLASH_IRQHandler,                             // 4
  RCC_IRQHandler,                               // 5
  EXTI0_IRQHandler,                             // 6
  EXTI1_IRQHandler,                             // 7
  EXTI2_IRQHandler,                             // 8
  EXTI3_IRQHandler,                             // 9
  EXTI4_IRQHandler,                             // 10
  DMA1_Channel1_IRQHandler,                     // 11
  DMA1_Channel2_IRQHandler,                     // 12
  DMA1_Channel3_IRQHandler,                     // 13
  DMA1_Channel4_IRQHandler,                     // 14
  DMA1_Channel5_IRQHandler,                     // 15
  DMA1_Channel6_IRQHandler,                     // 16
  DMA1_Channel7_IRQHandler,                     // 17
  ADC1_IRQHandler,                              // 18
  USB_HP_IRQHandler,                            // 19
  USB_LP_IRQHandler,                            // 20
  C2SEV_PWR_C2H_IRQHandler,                     // 21
  COMP_IRQHandler,                              // 22
  EXTI9_5_IRQHandler,                           // 23
  TIM1_BRK_IRQHandler,                          // 24
  TIM1_UP_TIM16_IRQHandler,                     // 25
  TIM1_TRG_COM_TIM17_IRQHandler,                // 26
  TIM1_CC_IRQHandler,                           // 27
  TIM2_IRQHandler,                              // 28
  PKA_IRQHandler,                               // 29
  I2C1_EV_IRQHandler,                           // 30
  I2C1_ER_IRQHandler,                           // 31
  I2C3_EV_IRQHandler,                           // 32
  I2C3_ER_IRQHandler,                           // 33
  SPI1_IRQHandler,                              // 34
  SPI2_IRQHandler,                              // 35
  USART1_IRQHandler,                            // 36
  LPUART1_IRQHandler,                           // 37
  SAI1_IRQHandler,                              // 38
  TSC_IRQHandler,                               // 39
  EXTI15_10_IRQHandler,                         // 40
  RTC_Alarm_IRQHandler,                         // 41
  CRS_IRQHandler,                               // 42
  PWR_SOTF_BLEACT_802ACT_RFPHASE_IRQHandler,    // 43
  IPCC_C1_RX_IRQHandler,                        // 44
  IPCC_C1_TX_IRQHandler,                        // 45
  HSEM_IRQHandler,                              // 46
  LPTIM1_IRQHandler,                            // 47
  LPTIM2_IRQHandler,                            // 48
  LCD_IRQHandler,                               // 49
  QUADSPI_IRQHandler,                           // 50
  AES1_IRQHandler,                              // 51
  AES2_IRQHandler,                              // 52
  RNG_IRQHandler,                               // 53
  FPU_IRQHandler,                               // 54
  DMA2_Channel1_IRQHandler,                     // 55
  DMA2_Channel2_IRQHandler,                     // 56
  DMA2_Channel3_IRQHandler,                     // 57
  DMA2_Channel4_IRQHandler,                     // 58
  DMA2_Channel5_IRQHandler,                     // 59
  DMA2_Channel6_IRQHandler,                     // 60
  DMA2_Channel7_IRQHandler,                     // 61
  DMAMUX1_OVR_IRQHandler,                       // 62
};

//-------------------------------------------------------------------------------------------------

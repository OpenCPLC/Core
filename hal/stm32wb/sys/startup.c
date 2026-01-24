/**
 * @file  startup.c
 * @brief Startup code and interrupt vector table for STM32WBxx
 */

#include <stdint.h>
#include "stm32wbxx.h"

#define __weak __attribute__((weak))

//------------------------------------------------------------------------------------------------- Linker symbols

extern uint32_t _sidata;    // Start address of init data in flash
extern uint32_t _sdata;     // Start address of data in RAM
extern uint32_t _edata;     // End address of data in RAM
extern uint32_t _sbss;      // Start address of bss
extern uint32_t _ebss;      // End address of bss
extern uint32_t _estack;    // Top of stack
extern uint32_t _siMB_MEM2; // Start address of init .MB_MEM2 section (BLE)
extern uint32_t _sMB_MEM2;  // Start address of .MB_MEM2 section
extern uint32_t _eMB_MEM2;  // End address of .MB_MEM2 section

extern void __libc_init_array(void);
extern int main(void);

//------------------------------------------------------------------------------------------------- Default handlers

void Reset_Handler(void)
{
  // Copy .data from flash to RAM
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while(dst < &_edata) *dst++ = *src++;
  // Copy .MB_MEM2 from flash to RAM (BLE stack data)
  src = &_siMB_MEM2;
  dst = &_sMB_MEM2;
  while(dst < &_eMB_MEM2) *dst++ = *src++;
  // Zero .bss
  dst = &_sbss;
  while(dst < &_ebss) *dst++ = 0;
  // Init system and run
  SystemInit();
  __libc_init_array();
  main();
  while(1);
}

void Default_Handler(void)
{
  while(1);
}

static void Void_Handler(void *src)
{
  (void)src;
}

//------------------------------------------------------------------------------------------------- IRQ callbacks: ADC

void (* volatile ADC_IRQFnc)(void *) = Void_Handler;
void * volatile ADC_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: EXTI

void (* volatile EXTI0_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI0_IRQSrc;
void (* volatile EXTI1_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI1_IRQSrc;
void (* volatile EXTI2_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI2_IRQSrc;
void (* volatile EXTI3_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI3_IRQSrc;
void (* volatile EXTI4_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI4_IRQSrc;
void (* volatile EXTI5_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI5_IRQSrc;
void (* volatile EXTI6_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI6_IRQSrc;
void (* volatile EXTI7_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI7_IRQSrc;
void (* volatile EXTI8_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI8_IRQSrc;
void (* volatile EXTI9_IRQFnc)(void *) = Void_Handler;  void * volatile EXTI9_IRQSrc;
void (* volatile EXTI10_IRQFnc)(void *) = Void_Handler; void * volatile EXTI10_IRQSrc;
void (* volatile EXTI11_IRQFnc)(void *) = Void_Handler; void * volatile EXTI11_IRQSrc;
void (* volatile EXTI12_IRQFnc)(void *) = Void_Handler; void * volatile EXTI12_IRQSrc;
void (* volatile EXTI13_IRQFnc)(void *) = Void_Handler; void * volatile EXTI13_IRQSrc;
void (* volatile EXTI14_IRQFnc)(void *) = Void_Handler; void * volatile EXTI14_IRQSrc;
void (* volatile EXTI15_IRQFnc)(void *) = Void_Handler; void * volatile EXTI15_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: DMA1

void (* volatile DMA1_CH1_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH1_IRQSrc;
void (* volatile DMA1_CH2_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH2_IRQSrc;
void (* volatile DMA1_CH3_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH3_IRQSrc;
void (* volatile DMA1_CH4_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH4_IRQSrc;
void (* volatile DMA1_CH5_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH5_IRQSrc;
void (* volatile DMA1_CH6_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH6_IRQSrc;
void (* volatile DMA1_CH7_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_CH7_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: DMA2

void (* volatile DMA2_CH1_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH1_IRQSrc;
void (* volatile DMA2_CH2_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH2_IRQSrc;
void (* volatile DMA2_CH3_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH3_IRQSrc;
void (* volatile DMA2_CH4_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH4_IRQSrc;
void (* volatile DMA2_CH5_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH5_IRQSrc;
void (* volatile DMA2_CH6_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH6_IRQSrc;
void (* volatile DMA2_CH7_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_CH7_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: TIM

void (* volatile TIM1_IRQFnc)(void *) = Void_Handler;  void * volatile TIM1_IRQSrc;
void (* volatile TIM2_IRQFnc)(void *) = Void_Handler;  void * volatile TIM2_IRQSrc;
void (* volatile TIM16_IRQFnc)(void *) = Void_Handler; void * volatile TIM16_IRQSrc;
void (* volatile TIM17_IRQFnc)(void *) = Void_Handler; void * volatile TIM17_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: LPTIM

void (* volatile LPTIM1_IRQFnc)(void *) = Void_Handler; void * volatile LPTIM1_IRQSrc;
void (* volatile LPTIM2_IRQFnc)(void *) = Void_Handler; void * volatile LPTIM2_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: I2C

void (* volatile I2C1_Event_IRQFnc)(void *) = Void_Handler; void * volatile I2C1_IRQSrc;
void (* volatile I2C1_Error_IRQFnc)(void *) = Void_Handler;
void (* volatile I2C3_Event_IRQFnc)(void *) = Void_Handler; void * volatile I2C3_IRQSrc;
void (* volatile I2C3_Error_IRQFnc)(void *) = Void_Handler;

//------------------------------------------------------------------------------------------------- IRQ callbacks: UART

void (* volatile USART1_IRQFnc)(void *) = Void_Handler;  void * volatile USART1_IRQSrc;
void (* volatile LPUART1_IRQFnc)(void *) = Void_Handler; void * volatile LPUART1_IRQSrc;

//------------------------------------------------------------------------------------------------- Weak IRQ handlers

__weak void HardFault_Handler(void) { while(1); }
__weak void NMI_Handler(void) { Default_Handler(); }
__weak void MemManage_Handler(void) { Default_Handler(); }
__weak void BusFault_Handler(void) { Default_Handler(); }
__weak void UsageFault_Handler(void) { Default_Handler(); }
__weak void SVC_Handler(void) { Default_Handler(); }
__weak void DebugMon_Handler(void) { Default_Handler(); }
__weak void PendSV_Handler(void) { Default_Handler(); }
__weak void SysTick_Handler(void) { Default_Handler(); }
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
__weak void SPI1_IRQHandler(void) { Default_Handler(); }
__weak void SPI2_IRQHandler(void) { Default_Handler(); }
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

// EXTI handlers
__weak void EXTI0_IRQHandler(void) { EXTI0_IRQFnc(EXTI0_IRQSrc); }
__weak void EXTI1_IRQHandler(void) { EXTI1_IRQFnc(EXTI1_IRQSrc); }
__weak void EXTI2_IRQHandler(void) { EXTI2_IRQFnc(EXTI2_IRQSrc); }
__weak void EXTI3_IRQHandler(void) { EXTI3_IRQFnc(EXTI3_IRQSrc); }
__weak void EXTI4_IRQHandler(void) { EXTI4_IRQFnc(EXTI4_IRQSrc); }

__weak void EXTI9_5_IRQHandler(void)
{
  EXTI5_IRQFnc(EXTI5_IRQSrc);
  EXTI6_IRQFnc(EXTI6_IRQSrc);
  EXTI7_IRQFnc(EXTI7_IRQSrc);
  EXTI8_IRQFnc(EXTI8_IRQSrc);
  EXTI9_IRQFnc(EXTI9_IRQSrc);
}

__weak void EXTI15_10_IRQHandler(void)
{
  EXTI10_IRQFnc(EXTI10_IRQSrc);
  EXTI11_IRQFnc(EXTI11_IRQSrc);
  EXTI12_IRQFnc(EXTI12_IRQSrc);
  EXTI13_IRQFnc(EXTI13_IRQSrc);
  EXTI14_IRQFnc(EXTI14_IRQSrc); // Fixed: was missing
  EXTI15_IRQFnc(EXTI15_IRQSrc);
}

// DMA1 handlers
__weak void DMA1_Channel1_IRQHandler(void) { DMA1_CH1_IRQFnc(DMA1_CH1_IRQSrc); }
__weak void DMA1_Channel2_IRQHandler(void) { DMA1_CH2_IRQFnc(DMA1_CH2_IRQSrc); }
__weak void DMA1_Channel3_IRQHandler(void) { DMA1_CH3_IRQFnc(DMA1_CH3_IRQSrc); }
__weak void DMA1_Channel4_IRQHandler(void) { DMA1_CH4_IRQFnc(DMA1_CH4_IRQSrc); }
__weak void DMA1_Channel5_IRQHandler(void) { DMA1_CH5_IRQFnc(DMA1_CH5_IRQSrc); }
__weak void DMA1_Channel6_IRQHandler(void) { DMA1_CH6_IRQFnc(DMA1_CH6_IRQSrc); }
__weak void DMA1_Channel7_IRQHandler(void) { DMA1_CH7_IRQFnc(DMA1_CH7_IRQSrc); }

// DMA2 handlers
__weak void DMA2_Channel1_IRQHandler(void) { DMA2_CH1_IRQFnc(DMA2_CH1_IRQSrc); }
__weak void DMA2_Channel2_IRQHandler(void) { DMA2_CH2_IRQFnc(DMA2_CH2_IRQSrc); }
__weak void DMA2_Channel3_IRQHandler(void) { DMA2_CH3_IRQFnc(DMA2_CH3_IRQSrc); }
__weak void DMA2_Channel4_IRQHandler(void) { DMA2_CH4_IRQFnc(DMA2_CH4_IRQSrc); }
__weak void DMA2_Channel5_IRQHandler(void) { DMA2_CH5_IRQFnc(DMA2_CH5_IRQSrc); }
__weak void DMA2_Channel6_IRQHandler(void) { DMA2_CH6_IRQFnc(DMA2_CH6_IRQSrc); }
__weak void DMA2_Channel7_IRQHandler(void) { DMA2_CH7_IRQFnc(DMA2_CH7_IRQSrc); }

// TIM handlers
__weak void TIM1_UP_TIM16_IRQHandler(void)
{
  TIM1_IRQFnc(TIM1_IRQSrc);
  TIM16_IRQFnc(TIM16_IRQSrc);
}
__weak void TIM1_TRG_COM_TIM17_IRQHandler(void) { TIM17_IRQFnc(TIM17_IRQSrc); }
__weak void TIM2_IRQHandler(void) { TIM2_IRQFnc(TIM2_IRQSrc); }

// LPTIM handlers
__weak void LPTIM1_IRQHandler(void) { LPTIM1_IRQFnc(LPTIM1_IRQSrc); }
__weak void LPTIM2_IRQHandler(void) { LPTIM2_IRQFnc(LPTIM2_IRQSrc); }

// I2C handlers
__weak void I2C1_EV_IRQHandler(void) { I2C1_Event_IRQFnc(I2C1_IRQSrc); }
__weak void I2C1_ER_IRQHandler(void) { I2C1_Error_IRQFnc(I2C1_IRQSrc); }
__weak void I2C3_EV_IRQHandler(void) { I2C3_Event_IRQFnc(I2C3_IRQSrc); }
__weak void I2C3_ER_IRQHandler(void) { I2C3_Error_IRQFnc(I2C3_IRQSrc); } // Fixed: was I2C1_IRQSrc

// ADC handler
__weak void ADC1_IRQHandler(void) { ADC_IRQFnc(ADC_IRQSrc); }

// UART handlers
__weak void USART1_IRQHandler(void) { USART1_IRQFnc(USART1_IRQSrc); }
__weak void LPUART1_IRQHandler(void) { LPUART1_IRQFnc(LPUART1_IRQSrc); }

//------------------------------------------------------------------------------------------------- Vector table

__attribute__((section(".isr_vector")))
const void (* const IRQ[])(void) = {
  (const void (*)(void))&_estack,
  Reset_Handler,
  NMI_Handler,
  HardFault_Handler,
  MemManage_Handler,
  BusFault_Handler,
  UsageFault_Handler,
  0, 0, 0, 0,                  // Reserved
  SVC_Handler,
  DebugMon_Handler,
  0,                           // Reserved
  PendSV_Handler,
  SysTick_Handler,
  // External interrupts
  WWDG_IRQHandler, // 0
  PVD_PVM_IRQHandler, // 1
  TAMP_STAMP_LSECSS_IRQHandler, // 2
  RTC_WKUP_IRQHandler, // 3
  FLASH_IRQHandler, // 4
  RCC_IRQHandler, // 5
  EXTI0_IRQHandler, // 6
  EXTI1_IRQHandler, // 7
  EXTI2_IRQHandler, // 8
  EXTI3_IRQHandler, // 9
  EXTI4_IRQHandler, // 10
  DMA1_Channel1_IRQHandler, // 11
  DMA1_Channel2_IRQHandler, // 12
  DMA1_Channel3_IRQHandler, // 13
  DMA1_Channel4_IRQHandler, // 14
  DMA1_Channel5_IRQHandler, // 15
  DMA1_Channel6_IRQHandler, // 16
  DMA1_Channel7_IRQHandler, // 17
  ADC1_IRQHandler, // 18
  USB_HP_IRQHandler, // 19
  USB_LP_IRQHandler, // 20
  C2SEV_PWR_C2H_IRQHandler, // 21
  COMP_IRQHandler, // 22
  EXTI9_5_IRQHandler, // 23
  TIM1_BRK_IRQHandler, // 24
  TIM1_UP_TIM16_IRQHandler, // 25
  TIM1_TRG_COM_TIM17_IRQHandler, // 26
  TIM1_CC_IRQHandler, // 27
  TIM2_IRQHandler, // 28
  PKA_IRQHandler, // 29
  I2C1_EV_IRQHandler, // 30
  I2C1_ER_IRQHandler, // 31
  I2C3_EV_IRQHandler, // 32
  I2C3_ER_IRQHandler, // 33
  SPI1_IRQHandler, // 34
  SPI2_IRQHandler, // 35
  USART1_IRQHandler, // 36
  LPUART1_IRQHandler, // 37
  SAI1_IRQHandler, // 38
  TSC_IRQHandler, // 39
  EXTI15_10_IRQHandler, // 40
  RTC_Alarm_IRQHandler, // 41
  CRS_IRQHandler, // 42
  PWR_SOTF_BLEACT_802ACT_RFPHASE_IRQHandler, // 43
  IPCC_C1_RX_IRQHandler, // 44
  IPCC_C1_TX_IRQHandler, // 45
  HSEM_IRQHandler, // 46
  LPTIM1_IRQHandler, // 47
  LPTIM2_IRQHandler, // 48
  LCD_IRQHandler, // 49
  QUADSPI_IRQHandler, // 50
  AES1_IRQHandler, // 51
  AES2_IRQHandler, // 52
  RNG_IRQHandler, // 53
  FPU_IRQHandler, // 54
  DMA2_Channel1_IRQHandler, // 55
  DMA2_Channel2_IRQHandler, // 56
  DMA2_Channel3_IRQHandler, // 57
  DMA2_Channel4_IRQHandler, // 58
  DMA2_Channel5_IRQHandler, // 59
  DMA2_Channel6_IRQHandler, // 60
  DMA2_Channel7_IRQHandler, // 61
  DMAMUX1_OVR_IRQHandler // 62
};
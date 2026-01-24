#include <stdint.h>
#include "system_stm32g0xx.h"

#define __weak __attribute__((weak))

//------------------------------------------------------------------------------------------------- Linker symbols

extern uint32_t _sidata; // Start address of init data in flash
extern uint32_t _sdata;  // Start address of data in RAM
extern uint32_t _edata;  // End address of data in RAM
extern uint32_t _sbss;   // Start address of bss
extern uint32_t _ebss;   // End address of bss
extern uint32_t _estack; // Top of stack

extern void __libc_init_array(void);
extern int main(void);

//------------------------------------------------------------------------------------------------- Default handlers

void Reset_Handler(void)
{
  // Copy .data from flash to RAM
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while(dst < &_edata) *dst++ = *src++;
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

static void Void_Handler(void *object)
{
  (void)object;
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

//------------------------------------------------------------------------------------------------- IRQ callbacks: DMA

void (* volatile DMA1_IRQFnc)(void *) = Void_Handler; void * volatile DMA1_IRQSrc;
void (* volatile DMA2_IRQFnc)(void *) = Void_Handler; void * volatile DMA2_IRQSrc;
void (* volatile DMA3_IRQFnc)(void *) = Void_Handler; void * volatile DMA3_IRQSrc;
void (* volatile DMA4_IRQFnc)(void *) = Void_Handler; void * volatile DMA4_IRQSrc;
void (* volatile DMA5_IRQFnc)(void *) = Void_Handler; void * volatile DMA5_IRQSrc;
void (* volatile DMA6_IRQFnc)(void *) = Void_Handler; void * volatile DMA6_IRQSrc;
void (* volatile DMA7_IRQFnc)(void *) = Void_Handler; void * volatile DMA7_IRQSrc;
#ifdef STM32G0C1xx
  void (* volatile DMA8_IRQFnc)(void *) = Void_Handler;  void * volatile DMA8_IRQSrc;
  void (* volatile DMA9_IRQFnc)(void *) = Void_Handler;  void * volatile DMA9_IRQSrc;
  void (* volatile DMA10_IRQFnc)(void *) = Void_Handler; void * volatile DMA10_IRQSrc;
  void (* volatile DMA11_IRQFnc)(void *) = Void_Handler; void * volatile DMA11_IRQSrc;
  void (* volatile DMA12_IRQFnc)(void *) = Void_Handler; void * volatile DMA12_IRQSrc;
#endif

//------------------------------------------------------------------------------------------------- IRQ callbacks: TIM

void (* volatile TIM1_IRQFnc)(void *) = Void_Handler;  void * volatile TIM1_IRQSrc;
void (* volatile TIM2_IRQFnc)(void *) = Void_Handler;  void * volatile TIM2_IRQSrc;
void (* volatile TIM3_IRQFnc)(void *) = Void_Handler;  void * volatile TIM3_IRQSrc;
void (* volatile TIM6_IRQFnc)(void *) = Void_Handler;  void * volatile TIM6_IRQSrc;
void (* volatile TIM7_IRQFnc)(void *) = Void_Handler;  void * volatile TIM7_IRQSrc;
void (* volatile TIM14_IRQFnc)(void *) = Void_Handler; void * volatile TIM14_IRQSrc;
void (* volatile TIM15_IRQFnc)(void *) = Void_Handler; void * volatile TIM15_IRQSrc;
void (* volatile TIM16_IRQFnc)(void *) = Void_Handler; void * volatile TIM16_IRQSrc;
void (* volatile TIM17_IRQFnc)(void *) = Void_Handler; void * volatile TIM17_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: I2C

void (* volatile I2C1_IRQFnc)(void *) = Void_Handler; void * volatile I2C1_IRQSrc;
void (* volatile I2C2_IRQFnc)(void *) = Void_Handler; void * volatile I2C2_IRQSrc;

//------------------------------------------------------------------------------------------------- IRQ callbacks: UART

void (* volatile USART1_IRQFnc)(void *) = Void_Handler;  void * volatile USART1_IRQSrc;
void (* volatile USART2_IRQFnc)(void *) = Void_Handler;  void * volatile USART2_IRQSrc;
void (* volatile USART3_IRQFnc)(void *) = Void_Handler;  void * volatile USART3_IRQSrc;
void (* volatile USART4_IRQFnc)(void *) = Void_Handler;  void * volatile USART4_IRQSrc;
void (* volatile LPUART1_IRQFnc)(void *) = Void_Handler; void * volatile LPUART1_IRQSrc;

//------------------------------------------------------------------------------------------------- Weak IRQ handlers

__weak void HardFault_Handler(void) { while(1); }
__weak void NMI_Handler(void) { Default_Handler(); }
__weak void SVC_Handler(void) { Default_Handler(); }
__weak void PendSV_Handler(void) { Default_Handler(); }
__weak void SysTick_Handler(void) { Default_Handler(); }
__weak void WWDG_IRQHandler(void) { Default_Handler(); }
__weak void PVD_IRQHandler(void) { Default_Handler(); }
__weak void RTC_STAMP_IRQHandler(void) { Default_Handler(); }
__weak void FLASH_IRQHandler(void) { Default_Handler(); }
__weak void RCC_IRQHandler(void) { Default_Handler(); }
__weak void UCPD1_UCPD2_IRQHandler(void) { Default_Handler(); }
__weak void CEC_IRQHandler(void) { Default_Handler(); }
__weak void TIM1_CC_IRQHandler(void) { Default_Handler(); }
__weak void SPI1_IRQHandler(void) { Default_Handler(); }
__weak void SPI2_IRQHandler(void) { Default_Handler(); }

__weak void ADC_COMP_IRQHandler(void)
{
  ADC_IRQFnc(ADC_IRQSrc);
}

__weak void EXTI0_1_IRQHandler(void)
{
  EXTI0_IRQFnc(EXTI0_IRQSrc);
  EXTI1_IRQFnc(EXTI1_IRQSrc);
}

__weak void EXTI2_3_IRQHandler(void)
{
  EXTI2_IRQFnc(EXTI2_IRQSrc);
  EXTI3_IRQFnc(EXTI3_IRQSrc);
}

__weak void EXTI4_15_IRQHandler(void)
{
  EXTI4_IRQFnc(EXTI4_IRQSrc);
  EXTI5_IRQFnc(EXTI5_IRQSrc);
  EXTI6_IRQFnc(EXTI6_IRQSrc);
  EXTI7_IRQFnc(EXTI7_IRQSrc);
  EXTI8_IRQFnc(EXTI8_IRQSrc);
  EXTI9_IRQFnc(EXTI9_IRQSrc);
  EXTI10_IRQFnc(EXTI10_IRQSrc);
  EXTI11_IRQFnc(EXTI11_IRQSrc);
  EXTI12_IRQFnc(EXTI12_IRQSrc);
  EXTI13_IRQFnc(EXTI13_IRQSrc);
  EXTI14_IRQFnc(EXTI14_IRQSrc);
  EXTI15_IRQFnc(EXTI15_IRQSrc);
}

__weak void DMA_Channel1_IRQHandler(void)
{
  DMA1_IRQFnc(DMA1_IRQSrc);
}

__weak void DMA_Channel2_3_IRQHandler(void)
{
  DMA2_IRQFnc(DMA2_IRQSrc);
  DMA3_IRQFnc(DMA3_IRQSrc);
}

__weak void DMA_Channel4_5_6_7_IRQHandler(void)
{
  DMA4_IRQFnc(DMA4_IRQSrc);
  DMA5_IRQFnc(DMA5_IRQSrc);
  DMA6_IRQFnc(DMA6_IRQSrc);
  DMA7_IRQFnc(DMA7_IRQSrc);
  #ifdef STM32G0C1xx
    DMA8_IRQFnc(DMA8_IRQSrc);
    DMA9_IRQFnc(DMA9_IRQSrc);
    DMA10_IRQFnc(DMA10_IRQSrc);
    DMA11_IRQFnc(DMA11_IRQSrc);
    DMA12_IRQFnc(DMA12_IRQSrc);
  #endif
}

__weak void TIM1_BRK_UP_TRG_COMP_IRQHandler(void) { TIM1_IRQFnc(TIM1_IRQSrc); }
__weak void TIM2_IRQHandler(void) { TIM2_IRQFnc(TIM2_IRQSrc); }
__weak void TIM3_IRQHandler(void) { TIM3_IRQFnc(TIM3_IRQSrc); }
__weak void TIM6_DAC_LPTIM1_IRQHandler(void) { TIM6_IRQFnc(TIM6_IRQSrc); }
__weak void TIM7_LPTIM2_IRQHandler(void) { TIM7_IRQFnc(TIM7_IRQSrc); }
__weak void TIM14_IRQHandler(void) { TIM14_IRQFnc(TIM14_IRQSrc); }
__weak void TIM15_IRQHandler(void) { TIM15_IRQFnc(TIM15_IRQSrc); }
__weak void TIM16_IRQHandler(void) { TIM16_IRQFnc(TIM16_IRQSrc); }
__weak void TIM17_IRQHandler(void) { TIM17_IRQFnc(TIM17_IRQSrc); }
__weak void I2C1_IRQHandler(void) { I2C1_IRQFnc(I2C1_IRQSrc); }
__weak void I2C2_IRQHandler(void) { I2C2_IRQFnc(I2C2_IRQSrc); }
__weak void USART1_IRQHandler(void) { USART1_IRQFnc(USART1_IRQSrc); }
__weak void USART2_IRQHandler(void) { USART2_IRQFnc(USART2_IRQSrc); }

__weak void USART3_USART4_LPUART1_IRQHandler(void)
{
  USART3_IRQFnc(USART3_IRQSrc);
  USART4_IRQFnc(USART4_IRQSrc);
  LPUART1_IRQFnc(LPUART1_IRQSrc);
}

//------------------------------------------------------------------------------------------------- Vector table

__attribute__((section(".isr_vector")))
const void (* const IRQ[])(void) = {
  (const void (*)(void))&_estack,
  Reset_Handler,
  NMI_Handler,
  HardFault_Handler,
  0, 0, 0, 0, 0, 0, 0, // Reserved
  SVC_Handler,
  0, 0, // Reserved
  PendSV_Handler,
  SysTick_Handler,
  // External interrupts
  WWDG_IRQHandler, // 0
  PVD_IRQHandler, // 1
  RTC_STAMP_IRQHandler, // 2
  FLASH_IRQHandler, // 3
  RCC_IRQHandler, // 4
  EXTI0_1_IRQHandler, // 5
  EXTI2_3_IRQHandler, // 6
  EXTI4_15_IRQHandler, // 7
  UCPD1_UCPD2_IRQHandler, // 8
  DMA_Channel1_IRQHandler, // 9
  DMA_Channel2_3_IRQHandler, // 10
  DMA_Channel4_5_6_7_IRQHandler, // 11
  ADC_COMP_IRQHandler, // 12
  TIM1_BRK_UP_TRG_COMP_IRQHandler, // 13
  TIM1_CC_IRQHandler, // 14
  TIM2_IRQHandler, // 15
  TIM3_IRQHandler, // 16
  TIM6_DAC_LPTIM1_IRQHandler, // 17
  TIM7_LPTIM2_IRQHandler, // 18
  TIM14_IRQHandler, // 19
  TIM15_IRQHandler, // 20
  TIM16_IRQHandler, // 21
  TIM17_IRQHandler, // 22
  I2C1_IRQHandler, // 23
  I2C2_IRQHandler, // 24
  SPI1_IRQHandler, // 25
  SPI2_IRQHandler, // 26
  USART1_IRQHandler, // 27
  USART2_IRQHandler, // 28
  USART3_USART4_LPUART1_IRQHandler, // 29
  CEC_IRQHandler // 30
};
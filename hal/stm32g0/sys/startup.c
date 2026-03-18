// hal/stm32g0/sys/startup.c

#include "startup.h"
#define __weak __attribute__((weak))

//------------------------------------------------------------------------------------------------- Linker symbols

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;

extern void __libc_init_array(void);
extern int main(void);

//------------------------------------------------------------------------------------------------- Default handlers

void Reset_Handler(void)
{
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while(dst < &_edata) *dst++ = *src++;
  dst = &_sbss;
  while(dst < &_ebss) *dst++ = 0;
  SystemInit();
  __libc_init_array();
  main();
  while(1);
}

void Default_Handler(void) { while(1); }

static void Void_Handler(void *object) { (void)object; }

//------------------------------------------------------------------------------------------------- IRQ callbacks: ADC

void (* volatile ADC_Cb)(void *) = Void_Handler;
void * volatile ADC_CbArg;

//------------------------------------------------------------------------------------------------- IRQ callbacks: EXTI

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

//------------------------------------------------------------------------------------------------- IRQ callbacks: DMA

void (* volatile DMA_CH1_Cb)(void *) = Void_Handler;  void * volatile DMA_CH1_CbArg;
void (* volatile DMA_CH2_Cb)(void *) = Void_Handler;  void * volatile DMA_CH2_CbArg;
void (* volatile DMA_CH3_Cb)(void *) = Void_Handler;  void * volatile DMA_CH3_CbArg;
void (* volatile DMA_CH4_Cb)(void *) = Void_Handler;  void * volatile DMA_CH4_CbArg;
void (* volatile DMA_CH5_Cb)(void *) = Void_Handler;  void * volatile DMA_CH5_CbArg;
void (* volatile DMA_CH6_Cb)(void *) = Void_Handler;  void * volatile DMA_CH6_CbArg;
void (* volatile DMA_CH7_Cb)(void *) = Void_Handler;  void * volatile DMA_CH7_CbArg;
#ifdef DMA2
void (* volatile DMA_CH8_Cb)(void *) = Void_Handler;  void * volatile DMA_CH8_CbArg;
void (* volatile DMA_CH9_Cb)(void *) = Void_Handler;  void * volatile DMA_CH9_CbArg;
void (* volatile DMA_CH10_Cb)(void *) = Void_Handler; void * volatile DMA_CH10_CbArg;
void (* volatile DMA_CH11_Cb)(void *) = Void_Handler; void * volatile DMA_CH11_CbArg;
void (* volatile DMA_CH12_Cb)(void *) = Void_Handler; void * volatile DMA_CH12_CbArg;
#endif

//------------------------------------------------------------------------------------------------- IRQ callbacks: TIM

void (* volatile TIM1_Cb)(void *) = Void_Handler;  void * volatile TIM1_CbArg;
void (* volatile TIM2_Cb)(void *) = Void_Handler;  void * volatile TIM2_CbArg;
void (* volatile TIM3_Cb)(void *) = Void_Handler;  void * volatile TIM3_CbArg;
#ifdef TIM4
void (* volatile TIM4_Cb)(void *) = Void_Handler;  void * volatile TIM4_CbArg;
#endif
void (* volatile TIM6_Cb)(void *) = Void_Handler;  void * volatile TIM6_CbArg;
void (* volatile TIM7_Cb)(void *) = Void_Handler;  void * volatile TIM7_CbArg;
void (* volatile TIM14_Cb)(void *) = Void_Handler; void * volatile TIM14_CbArg;
void (* volatile TIM15_Cb)(void *) = Void_Handler; void * volatile TIM15_CbArg;
void (* volatile TIM16_Cb)(void *) = Void_Handler; void * volatile TIM16_CbArg;
void (* volatile TIM17_Cb)(void *) = Void_Handler; void * volatile TIM17_CbArg;

//------------------------------------------------------------------------------------------------- IRQ callbacks: I2C

void (* volatile I2C1_Cb)(void *) = Void_Handler; void * volatile I2C1_CbArg;
void (* volatile I2C2_Cb)(void *) = Void_Handler; void * volatile I2C2_CbArg;
#ifdef I2C3
void (* volatile I2C3_Cb)(void *) = Void_Handler; void * volatile I2C3_CbArg;
#endif

//------------------------------------------------------------------------------------------------- IRQ callbacks: SPI

void (* volatile SPI1_Cb)(void *) = Void_Handler; void * volatile SPI1_CbArg;
void (* volatile SPI2_Cb)(void *) = Void_Handler; void * volatile SPI2_CbArg;
#ifdef SPI3
void (* volatile SPI3_Cb)(void *) = Void_Handler; void * volatile SPI3_CbArg;
#endif

//------------------------------------------------------------------------------------------------- IRQ callbacks: UART

void (* volatile USART1_Cb)(void *) = Void_Handler;  void * volatile USART1_CbArg;
void (* volatile USART2_Cb)(void *) = Void_Handler;  void * volatile USART2_CbArg;
void (* volatile USART3_Cb)(void *) = Void_Handler;  void * volatile USART3_CbArg;
void (* volatile USART4_Cb)(void *) = Void_Handler;  void * volatile USART4_CbArg;
#ifdef USART5
void (* volatile USART5_Cb)(void *) = Void_Handler;  void * volatile USART5_CbArg;
#endif
#ifdef USART6
void (* volatile USART6_Cb)(void *) = Void_Handler;  void * volatile USART6_CbArg;
#endif
void (* volatile LPUART1_Cb)(void *) = Void_Handler; void * volatile LPUART1_CbArg;
#ifdef LPUART2
void (* volatile LPUART2_Cb)(void *) = Void_Handler; void * volatile LPUART2_CbArg;
#endif

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
__weak void USB_UCPD_IRQHandler(void) { Default_Handler(); }
__weak void CEC_IRQHandler(void) { Default_Handler(); }
__weak void TIM1_CC_IRQHandler(void) { Default_Handler(); }
__weak void RNG_IRQHandler(void) { Default_Handler(); }

__weak void ADC_COMP_IRQHandler(void)
{
  ADC_Cb(ADC_CbArg);
}

__weak void EXTI0_1_IRQHandler(void)
{
  EXTI0_Cb(EXTI0_CbArg);
  EXTI1_Cb(EXTI1_CbArg);
}

__weak void EXTI2_3_IRQHandler(void)
{
  EXTI2_Cb(EXTI2_CbArg);
  EXTI3_Cb(EXTI3_CbArg);
}

__weak void EXTI4_15_IRQHandler(void)
{
  EXTI4_Cb(EXTI4_CbArg);
  EXTI5_Cb(EXTI5_CbArg);
  EXTI6_Cb(EXTI6_CbArg);
  EXTI7_Cb(EXTI7_CbArg);
  EXTI8_Cb(EXTI8_CbArg);
  EXTI9_Cb(EXTI9_CbArg);
  EXTI10_Cb(EXTI10_CbArg);
  EXTI11_Cb(EXTI11_CbArg);
  EXTI12_Cb(EXTI12_CbArg);
  EXTI13_Cb(EXTI13_CbArg);
  EXTI14_Cb(EXTI14_CbArg);
  EXTI15_Cb(EXTI15_CbArg);
}

__weak void DMA_Channel1_IRQHandler(void)
{
  DMA_CH1_Cb(DMA_CH1_CbArg);
}

__weak void DMA_Channel2_3_IRQHandler(void)
{
  DMA_CH2_Cb(DMA_CH2_CbArg);
  DMA_CH3_Cb(DMA_CH3_CbArg);
}

__weak void DMA_Channel4_7_DMA2_IRQHandler(void)
{
  DMA_CH4_Cb(DMA_CH4_CbArg);
  DMA_CH5_Cb(DMA_CH5_CbArg);
  DMA_CH6_Cb(DMA_CH6_CbArg);
  DMA_CH7_Cb(DMA_CH7_CbArg);
  #ifdef DMA2
  DMA_CH8_Cb(DMA_CH8_CbArg);
  DMA_CH9_Cb(DMA_CH9_CbArg);
  DMA_CH10_Cb(DMA_CH10_CbArg);
  DMA_CH11_Cb(DMA_CH11_CbArg);
  DMA_CH12_Cb(DMA_CH12_CbArg);
  #endif
}

__weak void TIM1_BRK_UP_TRG_COMP_IRQHandler(void) { TIM1_Cb(TIM1_CbArg); }
__weak void TIM2_IRQHandler(void) { TIM2_Cb(TIM2_CbArg); }

__weak void TIM3_TIM4_IRQHandler(void)
{
  TIM3_Cb(TIM3_CbArg);
  #ifdef TIM4
  TIM4_Cb(TIM4_CbArg);
  #endif
}

__weak void TIM6_DAC_LPTIM1_IRQHandler(void) { TIM6_Cb(TIM6_CbArg); }
__weak void TIM7_LPTIM2_IRQHandler(void) { TIM7_Cb(TIM7_CbArg); }
__weak void TIM14_IRQHandler(void) { TIM14_Cb(TIM14_CbArg); }
__weak void TIM15_IRQHandler(void) { TIM15_Cb(TIM15_CbArg); }
__weak void TIM16_IRQHandler(void) { TIM16_Cb(TIM16_CbArg); }
__weak void TIM17_IRQHandler(void) { TIM17_Cb(TIM17_CbArg); }

__weak void I2C1_IRQHandler(void) { I2C1_Cb(I2C1_CbArg); }

__weak void I2C2_I2C3_IRQHandler(void)
{
  I2C2_Cb(I2C2_CbArg);
  #ifdef I2C3
  I2C3_Cb(I2C3_CbArg);
  #endif
}

__weak void SPI1_IRQHandler(void) { SPI1_Cb(SPI1_CbArg); }

__weak void SPI2_SPI3_IRQHandler(void)
{
  SPI2_Cb(SPI2_CbArg);
  #ifdef SPI3
  SPI3_Cb(SPI3_CbArg);
  #endif
}

__weak void USART1_IRQHandler(void) { USART1_Cb(USART1_CbArg); }

__weak void USART2_LPUART2_IRQHandler(void)
{
  USART2_Cb(USART2_CbArg);
  #ifdef LPUART2
  LPUART2_Cb(LPUART2_CbArg);
  #endif
}

__weak void USART3456_LPUART1_IRQHandler(void)
{
  USART3_Cb(USART3_CbArg);
  USART4_Cb(USART4_CbArg);
  #ifdef USART5
  USART5_Cb(USART5_CbArg);
  #endif
  #ifdef USART6
  USART6_Cb(USART6_CbArg);
  #endif
  LPUART1_Cb(LPUART1_CbArg);
}

//------------------------------------------------------------------------------------------------- Vector table

__attribute__((section(".isr_vector")))
const void (* const IRQ[])(void) = {
  (const void (*)(void))&_estack,
  Reset_Handler,
  NMI_Handler,
  HardFault_Handler,
  0, 0, 0, 0, 0, 0, 0,
  SVC_Handler,
  0, 0,
  PendSV_Handler,
  SysTick_Handler,
  WWDG_IRQHandler,                    // 0  IRQ_Watchdog
  PVD_IRQHandler,                     // 1  IRQ_PVD
  RTC_STAMP_IRQHandler,               // 2  IRQ_RTC
  FLASH_IRQHandler,                   // 3  IRQ_FLASH
  RCC_IRQHandler,                     // 4  IRQ_RCC
  EXTI0_1_IRQHandler,                 // 5  IRQ_EXTI01
  EXTI2_3_IRQHandler,                 // 6  IRQ_EXTI23
  EXTI4_15_IRQHandler,                // 7  IRQ_EXTI4F
  USB_UCPD_IRQHandler,                // 8  IRQ_USB
  DMA_Channel1_IRQHandler,            // 9  IRQ_DMA1_CH1
  DMA_Channel2_3_IRQHandler,          // 10 IRQ_DMA1_CH23
  DMA_Channel4_7_DMA2_IRQHandler,     // 11 IRQ_DMA1_CH47_DMA2
  ADC_COMP_IRQHandler,                // 12 IRQ_ADC
  TIM1_BRK_UP_TRG_COMP_IRQHandler,    // 13 IRQ_TIM1
  TIM1_CC_IRQHandler,                 // 14 IRQ_TIM1_CC
  TIM2_IRQHandler,                    // 15 IRQ_TIM2
  TIM3_TIM4_IRQHandler,               // 16 IRQ_TIM3_TIM4
  TIM6_DAC_LPTIM1_IRQHandler,         // 17 IRQ_TIM6_DAC_LPTIM1
  TIM7_LPTIM2_IRQHandler,             // 18 IRQ_TIM7_LPTIM2
  TIM14_IRQHandler,                   // 19 IRQ_TIM14
  TIM15_IRQHandler,                   // 20 IRQ_TIM15
  TIM16_IRQHandler,                   // 21 IRQ_TIM16
  TIM17_IRQHandler,                   // 22 IRQ_TIM17
  I2C1_IRQHandler,                    // 23 IRQ_I2C1
  I2C2_I2C3_IRQHandler,               // 24 IRQ_I2C23
  SPI1_IRQHandler,                    // 25 IRQ_SPI1
  SPI2_SPI3_IRQHandler,               // 26 IRQ_SPI23
  USART1_IRQHandler,                  // 27 IRQ_UART1
  USART2_LPUART2_IRQHandler,          // 28 IRQ_UART2_LPUART2
  USART3456_LPUART1_IRQHandler,       // 29 IRQ_UART3456_LPUART1
  CEC_IRQHandler,                     // 30 IRQ_CEC
  RNG_IRQHandler                      // 31 IRQ_RNG
};
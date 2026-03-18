// hal/stm32g0/sys/irq_g0.h

#ifndef IRQ_G0_H_
#define IRQ_G0_H_

#include "stm32g0xx.h"

//-------------------------------------------------------------------------------------------------

// 16 priorities for WB compatibility, G0 uses only upper 2 bits (value >> 2)
// Cross-platform code uses: VeryHigh/High/Medium/Low (0/4/8/12)
typedef enum {
  IRQ_Priority_VeryHigh   = 0,
  IRQ_Priority_VeryHigh_1 = 1,
  IRQ_Priority_VeryHigh_2 = 2,
  IRQ_Priority_VeryHigh_3 = 3,
  IRQ_Priority_High       = 4,
  IRQ_Priority_High_1     = 5,
  IRQ_Priority_High_2     = 6,
  IRQ_Priority_High_3     = 7,
  IRQ_Priority_Medium     = 8,
  IRQ_Priority_Medium_1   = 9,
  IRQ_Priority_Medium_2   = 10,
  IRQ_Priority_Medium_3   = 11,
  IRQ_Priority_Low        = 12,
  IRQ_Priority_Low_1      = 13,
  IRQ_Priority_Low_2      = 14,
  IRQ_Priority_Low_3      = 15
} IRQ_Priority_t;

//-------------------------------------------------------------------------------------------------

typedef enum {
  IRQ_NonMaskableInt = -14,
  IRQ_HardFault = -13,
  IRQ_SVCall = -5,
  IRQ_PendSV = -2,
  IRQ_SysTick = -1,
  IRQ_Watchdog = 0,
  IRQ_PVD = 1,
  IRQ_RTC = 2,
  IRQ_FLASH = 3,
  IRQ_RCC = 4,
  IRQ_EXTI01 = 5,
  IRQ_EXTI23 = 6,
  IRQ_EXTI4F = 7,
  IRQ_USB = 8,
  IRQ_DMA1_CH1 = 9,
  IRQ_DMA1_CH23 = 10,
  IRQ_DMA1_CH47_DMA2 = 11,
  IRQ_ADC = 12,
  IRQ_TIM1 = 13,
  IRQ_TIM1_CC = 14,
  IRQ_TIM2 = 15,
  IRQ_TIM3_TIM4 = 16,
  IRQ_TIM6_DAC_LPTIM1 = 17,
  IRQ_TIM7_LPTIM2 = 18,
  IRQ_TIM14 = 19,
  IRQ_TIM15 = 20,
  IRQ_TIM16 = 21,
  IRQ_TIM17 = 22,
  IRQ_I2C1 = 23,
  IRQ_I2C23 = 24,
  IRQ_SPI1 = 25,
  IRQ_SPI23 = 26,
  IRQ_UART1 = 27,
  IRQ_UART2_LPUART2 = 28,
  IRQ_UART3456_LPUART1 = 29,
  IRQ_CEC = 30,
  IRQ_RNG = 31
} IRQ_t;

//-------------------------------------------------------------------------------------------------

#endif
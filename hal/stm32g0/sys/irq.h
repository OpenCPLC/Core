#ifndef IRQ_H_
#define IRQ_H_

#include <stdint.h>
#include "stm32g0xx.h"
#include "startup.h"

//------------------------------------------------------------------------------------------------- IRQ numbers

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

//------------------------------------------------------------------------------------------------- Priority

typedef enum {
  IRQ_Priority_VeryHigh = 0,
  IRQ_Priority_High = 1,
  IRQ_Priority_Medium = 2,
  IRQ_Priority_Low = 3
} IRQ_Priority_t;

//------------------------------------------------------------------------------------------------- DMA

typedef enum {
  DMA_Nbr_None = 0,
  DMA_Nbr_1 = 1,
  DMA_Nbr_2 = 2,
  DMA_Nbr_3 = 3,
  DMA_Nbr_4 = 4,
  DMA_Nbr_5 = 5,
  DMA_Nbr_6 = 6,
  DMA_Nbr_7 = 7,
  #ifdef STM32G0C1xx
    DMA_Nbr_8 = 8,
    DMA_Nbr_9 = 9,
    DMA_Nbr_10 = 10,
    DMA_Nbr_11 = 11,
    DMA_Nbr_12 = 12
  #endif
} DMA_Nbr_t;

typedef struct {
  DMA_TypeDef *reg;
  DMA_Channel_TypeDef *cha;
  DMAMUX_Channel_TypeDef *mux;
  uint8_t pos;
} DMA_t;

typedef enum {
  DMAMUX_REQ_GEN0 = 1,
  DMAMUX_REQ_GEN1 = 2,
  DMAMUX_REQ_GEN2 = 3,
  DMAMUX_REQ_GEN3 = 4,
  DMAMUX_REQ_ADC = 5,
  DMAMUX_REQ_AES_IN = 6,
  DMAMUX_REQ_AES_OUT = 7,
  DMAMUX_REQ_DAC_CH1 = 8,
  DMAMUX_REQ_DAC_CH2 = 9,
  DMAMUX_REQ_I2C1_RX = 10,
  DMAMUX_REQ_I2C1_TX = 11,
  DMAMUX_REQ_I2C2_RX = 12,
  DMAMUX_REQ_I2C2_TX = 13,
  DMAMUX_REQ_LPUART1_RX = 14,
  DMAMUX_REQ_LPUART1_TX = 15,
  DMAMUX_REQ_SPI1_RX = 16,
  DMAMUX_REQ_SPI1_TX = 17,
  DMAMUX_REQ_SPI2_RX = 18,
  DMAMUX_REQ_SPI2_TX = 19,
  DMAMUX_REQ_TIM1_CH1 = 20,
  DMAMUX_REQ_TIM1_CH2 = 21,
  DMAMUX_REQ_TIM1_CH3 = 22,
  DMAMUX_REQ_TIM1_CH4 = 23,
  DMAMUX_REQ_TIM1_TRIG_COM = 24,
  DMAMUX_REQ_TIM1_UP = 25,
  DMAMUX_REQ_TIM2_CH1 = 26,
  DMAMUX_REQ_TIM2_CH2 = 27,
  DMAMUX_REQ_TIM2_CH3 = 28,
  DMAMUX_REQ_TIM2_CH4 = 29,
  DMAMUX_REQ_TIM2_TRIG = 30,
  DMAMUX_REQ_TIM2_UP = 31,
  DMAMUX_REQ_TIM3_CH1 = 32,
  DMAMUX_REQ_TIM3_CH2 = 33,
  DMAMUX_REQ_TIM3_CH3 = 34,
  DMAMUX_REQ_TIM3_CH4 = 35,
  DMAMUX_REQ_TIM3_TRIG = 36,
  DMAMUX_REQ_TIM3_UP = 37,
  DMAMUX_REQ_TIM6_UP = 38,
  DMAMUX_REQ_TIM7_UP = 39,
  DMAMUX_REQ_TIM15_CH1 = 40,
  DMAMUX_REQ_TIM15_CH2 = 41,
  DMAMUX_REQ_TIM15_TRIG_COM = 42,
  DMAMUX_REQ_TIM15_UP = 43,
  DMAMUX_REQ_TIM16_CH1 = 44,
  DMAMUX_REQ_TIM16_COM = 45,
  DMAMUX_REQ_TIM16_UP = 46,
  DMAMUX_REQ_TIM17_CH1 = 47,
  DMAMUX_REQ_TIM17_COM = 48,
  DMAMUX_REQ_TIM17_UP = 49,
  DMAMUX_REQ_USART1_RX = 50,
  DMAMUX_REQ_USART1_TX = 51,
  DMAMUX_REQ_USART2_RX = 52,
  DMAMUX_REQ_USART2_TX = 53,
  DMAMUX_REQ_USART3_RX = 54,
  DMAMUX_REQ_USART3_TX = 55,
  DMAMUX_REQ_USART4_RX = 56,
  DMAMUX_REQ_USART4_TX = 57,
  DMAMUX_REQ_UCPD1_RX = 58,
  DMAMUX_REQ_UCPD1_TX = 59,
  DMAMUX_REQ_UCPD2_RX = 60,
  DMAMUX_REQ_UCPD2_TX = 61,
  DMAMUX_REQ_I2C3_RX = 62,
  DMAMUX_REQ_I2C3_TX = 63,
  DMAMUX_REQ_LPUART2_RX = 64,
  DMAMUX_REQ_LPUART2_TX = 65,
  DMAMUX_REQ_SPI3_RX = 66,
  DMAMUX_REQ_SPI3_TX = 67,
  DMAMUX_REQ_TIM4_CH1 = 68,
  DMAMUX_REQ_TIM4_CH2 = 69,
  DMAMUX_REQ_TIM4_CH3 = 70,
  DMAMUX_REQ_TIM4_CH4 = 71,
  DMAMUX_REQ_TIM4_TRIG = 72,
  DMAMUX_REQ_TIM4_UP = 73,
  DMAMUX_REQ_USART5_RX = 74,
  DMAMUX_REQ_USART5_TX = 75,
  DMAMUX_REQ_USART6_RX = 76,
  DMAMUX_REQ_USART6_TX = 77
} DMAMUX_REQ_t;

// DMA ISR flag macros
#define DMA_ISR_TEIF(pos) (8U << (4U * (pos)))
#define DMA_ISR_HTIF(pos) (4U << (4U * (pos)))
#define DMA_ISR_TCIF(pos) (2U << (4U * (pos)))
#define DMA_ISR_GIF(pos)  (1U << (4U * (pos)))

//------------------------------------------------------------------------------------------------- Functions

void IRQ_EnableTIM(TIM_TypeDef *tim, IRQ_Priority_t priority, void (*handler)(void *), void *object);
void IRQ_EnableUART(USART_TypeDef *uart, IRQ_Priority_t priority, void (*handler)(void *), void *object);
void IRQ_EnableI2C(I2C_TypeDef *i2c, IRQ_Priority_t priority, void (*handler)(void *), void *object);
void IRQ_EnableADC(IRQ_Priority_t priority, void (*handler)(void *), void *object);
void IRQ_EnableDMA(DMA_Nbr_t dma_nbr, IRQ_Priority_t priority, void (*handler)(void *), void *object);
void IRQ_EnableEXTI(uint8_t exti_nbr, IRQ_Priority_t priority, void (*handler)(void *), void *object);
void DMA_SetRegisters(DMA_Nbr_t dma_nbr, DMA_t *dma);
void DMA_ClearFlags(DMA_t *dma);

//-------------------------------------------------------------------------------------------------
#endif
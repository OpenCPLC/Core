#ifndef IRQ_H_
#define IRQ_H_

/**
 * @file  irq.h
 * @brief Interrupt enable functions for STM32WBxx
 */

#include <stdint.h>
#include "stm32wbxx.h"
#include "startup.h"

//------------------------------------------------------------------------------------------------- Priority grouping

typedef enum {
  IRQ_GroupPriorities_16 = 0b000, // 16 group priorities, 1 subpriority
  IRQ_GroupPriorities_8 = 0b100,  // 8 group priorities, 2 subpriorities
  IRQ_GroupPriorities_4 = 0b101,  // 4 group priorities, 4 subpriorities
  IRQ_GroupPriorities_2 = 0b110,  // 2 group priorities, 8 subpriorities
  IRQ_GroupPriorities_1 = 0b111   // 1 group priority, 16 subpriorities
} IRQ_GroupPriorities_t;

//------------------------------------------------------------------------------------------------- Priority levels

typedef enum {
  IRQ_Priority_VeryHigh = 0,
  IRQ_Priority_High = 1,
  IRQ_Priority_Medium = 2,
  IRQ_Priority_Low = 3
} IRQ_Priority_t;

//------------------------------------------------------------------------------------------------- DMA channels

typedef enum {
  DMA_Channel_None = 0,
  DMA_Channel_1 = 1,
  DMA_Channel_2 = 2,
  DMA_Channel_3 = 3,
  DMA_Channel_4 = 4,
  DMA_Channel_5 = 5,
  DMA_Channel_6 = 6,
  DMA_Channel_7 = 7,
  DMA_Channel_8 = 8,
  DMA_Channel_9 = 9,
  DMA_Channel_10 = 10,
  DMA_Channel_11 = 11,
  DMA_Channel_12 = 12,
  DMA_Channel_13 = 13,
  DMA_Channel_14 = 14
} DMA_Channel_t;

//------------------------------------------------------------------------------------------------- Functions

void IRQ_Init(void);
void IRQ_EnableTIM(TIM_TypeDef *tim, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object);
void IRQ_EnableUART(USART_TypeDef *uart, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object);
void IRQ_EnableI2C(I2C_TypeDef *i2c, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*event)(void *), void (*error)(void *), void *object);
void IRQ_EnableADC(IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object);
void IRQ_EnableDMA(uint8_t dma_nbr, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object);
void IRQ_EnableEXTI(uint8_t exti_nbr, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object);

//-------------------------------------------------------------------------------------------------

#endif
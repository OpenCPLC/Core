// hal/stm32/sys/irq.h

#ifndef IRQ_H_
#define IRQ_H_

#include <stdint.h>
#include <stdbool.h>
#include "dma.h"

#if defined(STM32G0)
  #include "irq_g0.h"
#elif defined(STM32WB)
  #include "irq_wb.h"
#endif

//------------------------------------------------------------------------------------------------- Types

typedef void (*IRQ_Handler_t)(void *);

//------------------------------------------------------------------------------------------------- Enable functions (unified API)

void IRQ_EnableTIM(void *tim, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object);
void IRQ_EnableUART(void *uart, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object);
void IRQ_EnableI2C(void *i2c, IRQ_Priority_t priority, IRQ_Handler_t event, IRQ_Handler_t error, void *object);
void IRQ_EnableSPI(void *spi, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object);
void IRQ_EnableADC(IRQ_Priority_t priority, IRQ_Handler_t handler, void *object);
void IRQ_EnableDMA(DMA_CHx_t channel, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object);
void IRQ_EnableEXTI(uint8_t line, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object);

//------------------------------------------------------------------------------------------------- Disable functions

void IRQ_DisableTIM(void *tim);
void IRQ_DisableUART(void *uart);
void IRQ_DisableI2C(void *i2c);
void IRQ_DisableSPI(void *spi);
void IRQ_DisableADC(void);
void IRQ_DisableDMA(DMA_CHx_t channel);
void IRQ_DisableEXTI(uint8_t line);

//-------------------------------------------------------------------------------------------------
#endif

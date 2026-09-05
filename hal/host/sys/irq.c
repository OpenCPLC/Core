// hal/host/sys/irq.c

#include "irq.h"
#include "xdef.h"

void IRQ_EnableUSB(IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_DisableUSB(void) {}
void IRQ_ClearPendingUSB(void) {}

//-------------------------------------------------------------------------------------------------

/**
 * No interrupt controller off-target. Every entry point is accepted and does nothing:
 * a handler runs where the model calls it, not where a vector fires.
 */

void IRQ_EnableTIM(void *tim, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  unused(tim);
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_EnableTIMCC(void *tim, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  unused(tim);
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_EnableUART(void *uart, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  unused(uart);
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_EnableI2C(void *i2c, IRQ_Priority_t priority, IRQ_Handler_t event,
  IRQ_Handler_t error, void *object)
{
  unused(i2c);
  unused(priority);
  unused(event);
  unused(error);
  unused(object);
}

void IRQ_EnableSPI(void *spi, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  unused(spi);
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_EnableADC(IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_EnableDMA(DMA_CHx_t channel, IRQ_Priority_t priority, IRQ_Handler_t handler,
  void *object)
{
  unused(channel);
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_EnableEXTI(uint8_t line, IRQ_Priority_t priority, IRQ_Handler_t handler,
  void *object)
{
  unused(line);
  unused(priority);
  unused(handler);
  unused(object);
}

void IRQ_DisableTIM(void *tim)
{
  unused(tim);
}

void IRQ_DisableUART(void *uart)
{
  unused(uart);
}

void IRQ_DisableI2C(void *i2c)
{
  unused(i2c);
}

void IRQ_DisableSPI(void *spi)
{
  unused(spi);
}

void IRQ_DisableADC(void)
{
}

void IRQ_DisableDMA(DMA_CHx_t channel)
{
  unused(channel);
}

void IRQ_DisableEXTI(uint8_t line)
{
  unused(line);
}

void IRQ_ClearPendingTIM(void *tim)
{
  unused(tim);
}

void IRQ_ClearPendingUART(void *uart)
{
  unused(uart);
}

void IRQ_ClearPendingI2C(void *i2c)
{
  unused(i2c);
}

void IRQ_ClearPendingSPI(void *spi)
{
  unused(spi);
}

void IRQ_ClearPendingADC(void)
{
}

void IRQ_ClearPendingDMA(DMA_CHx_t channel)
{
  unused(channel);
}

void IRQ_ClearPendingEXTI(uint8_t line)
{
  unused(line);
}

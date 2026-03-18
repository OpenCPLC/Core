// hal/stm32g0/sys/irq_g0.c

#include "irq.h"
#include "startup.h"
#include "dma.h"

// M0+ has only 4 priority levels (2 bits), extract from unified 16-level enum
#define IRQ_PRIO(p) ((p) >> 2)

//------------------------------------------------------------------------------------------------- TIM

void IRQ_EnableTIM(void *tim, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQ_t irq;
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:
      TIM1_Cb = handler; TIM1_CbArg = object;
      irq = IRQ_TIM1;
      break;
    case (uint32_t)TIM2:
      TIM2_Cb = handler; TIM2_CbArg = object;
      irq = IRQ_TIM2;
      break;
    case (uint32_t)TIM3:
      TIM3_Cb = handler; TIM3_CbArg = object;
      irq = IRQ_TIM3_TIM4;
      break;
    #ifdef TIM4
    case (uint32_t)TIM4:
      TIM4_Cb = handler; TIM4_CbArg = object;
      irq = IRQ_TIM3_TIM4;
      break;
    #endif
    case (uint32_t)TIM6:
      TIM6_Cb = handler; TIM6_CbArg = object;
      irq = IRQ_TIM6_DAC_LPTIM1;
      break;
    case (uint32_t)TIM7:
      TIM7_Cb = handler; TIM7_CbArg = object;
      irq = IRQ_TIM7_LPTIM2;
      break;
    case (uint32_t)TIM14:
      TIM14_Cb = handler; TIM14_CbArg = object;
      irq = IRQ_TIM14;
      break;
    case (uint32_t)TIM15:
      TIM15_Cb = handler; TIM15_CbArg = object;
      irq = IRQ_TIM15;
      break;
    case (uint32_t)TIM16:
      TIM16_Cb = handler; TIM16_CbArg = object;
      irq = IRQ_TIM16;
      break;
    case (uint32_t)TIM17:
      TIM17_Cb = handler; TIM17_CbArg = object;
      irq = IRQ_TIM17;
      break;
    default: return;
  }
  NVIC_SetPriority(irq, IRQ_PRIO(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableTIM(void *tim)
{
  IRQ_t irq;
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:  irq = IRQ_TIM1; break;
    case (uint32_t)TIM2:  irq = IRQ_TIM2; break;
    case (uint32_t)TIM3:  irq = IRQ_TIM3_TIM4; break;
    #ifdef TIM4
    case (uint32_t)TIM4:  irq = IRQ_TIM3_TIM4; break;
    #endif
    case (uint32_t)TIM6:  irq = IRQ_TIM6_DAC_LPTIM1; break;
    case (uint32_t)TIM7:  irq = IRQ_TIM7_LPTIM2; break;
    case (uint32_t)TIM14: irq = IRQ_TIM14; break;
    case (uint32_t)TIM15: irq = IRQ_TIM15; break;
    case (uint32_t)TIM16: irq = IRQ_TIM16; break;
    case (uint32_t)TIM17: irq = IRQ_TIM17; break;
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- UART

void IRQ_EnableUART(void *uart, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQ_t irq;
  switch((uint32_t)uart) {
    case (uint32_t)USART1:
      USART1_Cb = handler; USART1_CbArg = object;
      irq = IRQ_UART1;
      break;
    case (uint32_t)USART2:
      USART2_Cb = handler; USART2_CbArg = object;
      irq = IRQ_UART2_LPUART2;
      break;
    case (uint32_t)USART3:
      USART3_Cb = handler; USART3_CbArg = object;
      irq = IRQ_UART3456_LPUART1;
      break;
    case (uint32_t)USART4:
      USART4_Cb = handler; USART4_CbArg = object;
      irq = IRQ_UART3456_LPUART1;
      break;
    #ifdef USART5
    case (uint32_t)USART5:
      USART5_Cb = handler; USART5_CbArg = object;
      irq = IRQ_UART3456_LPUART1;
      break;
    #endif
    #ifdef USART6
    case (uint32_t)USART6:
      USART6_Cb = handler; USART6_CbArg = object;
      irq = IRQ_UART3456_LPUART1;
      break;
    #endif
    case (uint32_t)LPUART1:
      LPUART1_Cb = handler; LPUART1_CbArg = object;
      irq = IRQ_UART3456_LPUART1;
      break;
    #ifdef LPUART2
    case (uint32_t)LPUART2:
      LPUART2_Cb = handler; LPUART2_CbArg = object;
      irq = IRQ_UART2_LPUART2;
      break;
    #endif
    default: return;
  }
  NVIC_SetPriority(irq, IRQ_PRIO(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableUART(void *uart)
{
  IRQ_t irq;
  switch((uint32_t)uart) {
    case (uint32_t)USART1: irq = IRQ_UART1; break;
    case (uint32_t)USART2: irq = IRQ_UART2_LPUART2; break;
    #ifdef LPUART2
    case (uint32_t)LPUART2: irq = IRQ_UART2_LPUART2; break;
    #endif
    case (uint32_t)USART3:
    case (uint32_t)USART4:
    #ifdef USART5
    case (uint32_t)USART5:
    #endif
    #ifdef USART6
    case (uint32_t)USART6:
    #endif
    case (uint32_t)LPUART1: irq = IRQ_UART3456_LPUART1; break;
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- I2C

void IRQ_EnableI2C(void *i2c, IRQ_Priority_t priority, IRQ_Handler_t event, IRQ_Handler_t error, void *object)
{
  (void)error; // G0 has single IRQ per I2C
  IRQ_t irq;
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1:
      I2C1_Cb = event; I2C1_CbArg = object;
      irq = IRQ_I2C1;
      break;
    case (uint32_t)I2C2:
      I2C2_Cb = event; I2C2_CbArg = object;
      irq = IRQ_I2C23;
      break;
    #ifdef I2C3
    case (uint32_t)I2C3:
      I2C3_Cb = event; I2C3_CbArg = object;
      irq = IRQ_I2C23;
      break;
    #endif
    default: return;
  }
  NVIC_SetPriority(irq, IRQ_PRIO(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableI2C(void *i2c)
{
  IRQ_t irq;
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1: irq = IRQ_I2C1; break;
    case (uint32_t)I2C2: irq = IRQ_I2C23; break;
    #ifdef I2C3
    case (uint32_t)I2C3: irq = IRQ_I2C23; break;
    #endif
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- SPI

void IRQ_EnableSPI(void *spi, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQ_t irq;
  switch((uint32_t)spi) {
    case (uint32_t)SPI1:
      SPI1_Cb = handler; SPI1_CbArg = object;
      irq = IRQ_SPI1;
      break;
    case (uint32_t)SPI2:
      SPI2_Cb = handler; SPI2_CbArg = object;
      irq = IRQ_SPI23;
      break;
    #ifdef SPI3
    case (uint32_t)SPI3:
      SPI3_Cb = handler; SPI3_CbArg = object;
      irq = IRQ_SPI23;
      break;
    #endif
    default: return;
  }
  NVIC_SetPriority(irq, IRQ_PRIO(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableSPI(void *spi)
{
  IRQ_t irq;
  switch((uint32_t)spi) {
    case (uint32_t)SPI1: irq = IRQ_SPI1; break;
    case (uint32_t)SPI2: irq = IRQ_SPI23; break;
    #ifdef SPI3
    case (uint32_t)SPI3: irq = IRQ_SPI23; break;
    #endif
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- ADC

void IRQ_EnableADC(IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  ADC_Cb = handler;
  ADC_CbArg = object;
  NVIC_SetPriority(IRQ_ADC, IRQ_PRIO(priority));
  NVIC_EnableIRQ(IRQ_ADC);
}

void IRQ_DisableADC(void)
{
  NVIC_DisableIRQ(IRQ_ADC);
}

//------------------------------------------------------------------------------------------------- DMA

void IRQ_EnableDMA(DMA_CHx_t channel, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQ_t irq;
  switch(channel) {
    case DMA_CH1:
      DMA_CH1_Cb = handler; DMA_CH1_CbArg = object;
      irq = IRQ_DMA1_CH1;
      break;
    case DMA_CH2:
      DMA_CH2_Cb = handler; DMA_CH2_CbArg = object;
      irq = IRQ_DMA1_CH23;
      break;
    case DMA_CH3:
      DMA_CH3_Cb = handler; DMA_CH3_CbArg = object;
      irq = IRQ_DMA1_CH23;
      break;
    case DMA_CH4: case DMA_CH5: case DMA_CH6: case DMA_CH7:
      if(channel == DMA_CH4) { DMA_CH4_Cb = handler; DMA_CH4_CbArg = object; }
      if(channel == DMA_CH5) { DMA_CH5_Cb = handler; DMA_CH5_CbArg = object; }
      if(channel == DMA_CH6) { DMA_CH6_Cb = handler; DMA_CH6_CbArg = object; }
      if(channel == DMA_CH7) { DMA_CH7_Cb = handler; DMA_CH7_CbArg = object; }
      irq = IRQ_DMA1_CH47_DMA2;
      break;
    #ifdef DMA2
    case DMA_CH8: case DMA_CH9: case DMA_CH10: case DMA_CH11: case DMA_CH12:
      if(channel == DMA_CH8)  { DMA_CH8_Cb = handler;  DMA_CH8_CbArg = object; }
      if(channel == DMA_CH9)  { DMA_CH9_Cb = handler;  DMA_CH9_CbArg = object; }
      if(channel == DMA_CH10) { DMA_CH10_Cb = handler; DMA_CH10_CbArg = object; }
      if(channel == DMA_CH11) { DMA_CH11_Cb = handler; DMA_CH11_CbArg = object; }
      if(channel == DMA_CH12) { DMA_CH12_Cb = handler; DMA_CH12_CbArg = object; }
      irq = IRQ_DMA1_CH47_DMA2;
      break;
    #endif
    default: return;
  }
  NVIC_SetPriority(irq, IRQ_PRIO(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableDMA(DMA_CHx_t channel)
{
  IRQ_t irq;
  switch(channel) {
    case DMA_CH1: irq = IRQ_DMA1_CH1; break;
    case DMA_CH2: case DMA_CH3: irq = IRQ_DMA1_CH23; break;
    default: irq = IRQ_DMA1_CH47_DMA2; break;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- EXTI

void IRQ_EnableEXTI(uint8_t line, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQ_t irq;
  if(line <= 1) irq = IRQ_EXTI01;
  else if(line <= 3) irq = IRQ_EXTI23;
  else if(line <= 15) irq = IRQ_EXTI4F;
  else return;
  switch(line) {
    case 0:  EXTI0_Cb = handler;  EXTI0_CbArg = object;  break;
    case 1:  EXTI1_Cb = handler;  EXTI1_CbArg = object;  break;
    case 2:  EXTI2_Cb = handler;  EXTI2_CbArg = object;  break;
    case 3:  EXTI3_Cb = handler;  EXTI3_CbArg = object;  break;
    case 4:  EXTI4_Cb = handler;  EXTI4_CbArg = object;  break;
    case 5:  EXTI5_Cb = handler;  EXTI5_CbArg = object;  break;
    case 6:  EXTI6_Cb = handler;  EXTI6_CbArg = object;  break;
    case 7:  EXTI7_Cb = handler;  EXTI7_CbArg = object;  break;
    case 8:  EXTI8_Cb = handler;  EXTI8_CbArg = object;  break;
    case 9:  EXTI9_Cb = handler;  EXTI9_CbArg = object;  break;
    case 10: EXTI10_Cb = handler; EXTI10_CbArg = object; break;
    case 11: EXTI11_Cb = handler; EXTI11_CbArg = object; break;
    case 12: EXTI12_Cb = handler; EXTI12_CbArg = object; break;
    case 13: EXTI13_Cb = handler; EXTI13_CbArg = object; break;
    case 14: EXTI14_Cb = handler; EXTI14_CbArg = object; break;
    case 15: EXTI15_Cb = handler; EXTI15_CbArg = object; break;
  }
  NVIC_SetPriority(irq, IRQ_PRIO(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableEXTI(uint8_t line)
{
  IRQ_t irq;
  if(line <= 1) irq = IRQ_EXTI01;
  else if(line <= 3) irq = IRQ_EXTI23;
  else if(line <= 15) irq = IRQ_EXTI4F;
  else return;
  NVIC_DisableIRQ(irq);
}

//-------------------------------------------------------------------------------------------------
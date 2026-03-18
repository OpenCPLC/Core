// hal/stm32wb/sys/irq_wb.c

#include "irq.h"
#include "startup.h"

#define IRQ_PRIORITY_GROUP 5  // 2 bits preempt, 2 bits sub

//------------------------------------------------------------------------------------------------- Priority

void IRQ_Init(void)
{
  NVIC_SetPriorityGrouping(IRQ_PRIORITY_GROUP);
}

static inline uint32_t EncodePriority(IRQ_Priority_t prio)
{
  uint8_t preempt = prio >> 2;
  uint8_t sub = prio & 0x03;
  return NVIC_EncodePriority(NVIC_GetPriorityGrouping(), preempt, sub);
}

void IRQ_SetPriority(IRQn_Type irq, IRQ_Priority_t priority)
{
  NVIC_SetPriority(irq, EncodePriority(priority));
}

//------------------------------------------------------------------------------------------------- TIM

void IRQ_EnableTIM(void *tim, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQn_Type irq;
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:
      TIM1_Cb = handler; TIM1_CbArg = object;
      irq = TIM1_UP_TIM16_IRQn;
      break;
    case (uint32_t)TIM2:
      TIM2_Cb = handler; TIM2_CbArg = object;
      irq = TIM2_IRQn;
      break;
    case (uint32_t)TIM16:
      TIM16_Cb = handler; TIM16_CbArg = object;
      irq = TIM1_UP_TIM16_IRQn;
      break;
    case (uint32_t)TIM17:
      TIM17_Cb = handler; TIM17_CbArg = object;
      irq = TIM1_TRG_COM_TIM17_IRQn;
      break;
    case (uint32_t)LPTIM1:
      LPTIM1_Cb = handler; LPTIM1_CbArg = object;
      irq = LPTIM1_IRQn;
      break;
    case (uint32_t)LPTIM2:
      LPTIM2_Cb = handler; LPTIM2_CbArg = object;
      irq = LPTIM2_IRQn;
      break;
    default: return;
  }
  NVIC_SetPriority(irq, EncodePriority(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableTIM(void *tim)
{
  IRQn_Type irq;
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:
    case (uint32_t)TIM16: irq = TIM1_UP_TIM16_IRQn; break;
    case (uint32_t)TIM2:  irq = TIM2_IRQn; break;
    case (uint32_t)TIM17: irq = TIM1_TRG_COM_TIM17_IRQn; break;
    case (uint32_t)LPTIM1: irq = LPTIM1_IRQn; break;
    case (uint32_t)LPTIM2: irq = LPTIM2_IRQn; break;
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- UART

void IRQ_EnableUART(void *uart, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQn_Type irq;
  switch((uint32_t)uart) {
    case (uint32_t)USART1:
      USART1_Cb = handler; USART1_CbArg = object;
      irq = USART1_IRQn;
      break;
    case (uint32_t)LPUART1:
      LPUART1_Cb = handler; LPUART1_CbArg = object;
      irq = LPUART1_IRQn;
      break;
    default: return;
  }
  NVIC_SetPriority(irq, EncodePriority(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableUART(void *uart)
{
  IRQn_Type irq;
  switch((uint32_t)uart) {
    case (uint32_t)USART1:  irq = USART1_IRQn; break;
    case (uint32_t)LPUART1: irq = LPUART1_IRQn; break;
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- I2C

void IRQ_EnableI2C(void *i2c, IRQ_Priority_t priority, IRQ_Handler_t event, IRQ_Handler_t error, void *object)
{
  IRQn_Type ev_irq, er_irq;
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1:
      I2C1_EventCallback = event; I2C1_CbArg = object;
      if(error) I2C1_ErrorCallback = error;
      ev_irq = I2C1_EV_IRQn;
      er_irq = I2C1_ER_IRQn;
      break;
    case (uint32_t)I2C3:
      I2C3_EventCallback = event; I2C3_CbArg = object;
      if(error) I2C3_ErrorCallback = error;
      ev_irq = I2C3_EV_IRQn;
      er_irq = I2C3_ER_IRQn;
      break;
    default: return;
  }
  uint32_t prio = EncodePriority(priority);
  NVIC_SetPriority(ev_irq, prio);
  NVIC_EnableIRQ(ev_irq);
  if(error) {
    NVIC_SetPriority(er_irq, prio);
    NVIC_EnableIRQ(er_irq);
  }
}

void IRQ_DisableI2C(void *i2c)
{
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1:
      NVIC_DisableIRQ(I2C1_EV_IRQn);
      NVIC_DisableIRQ(I2C1_ER_IRQn);
      break;
    case (uint32_t)I2C3:
      NVIC_DisableIRQ(I2C3_EV_IRQn);
      NVIC_DisableIRQ(I2C3_ER_IRQn);
      break;
  }
}

//------------------------------------------------------------------------------------------------- SPI

void IRQ_EnableSPI(void *spi, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQn_Type irq;
  switch((uint32_t)spi) {
    case (uint32_t)SPI1:
      SPI1_Cb = handler; SPI1_CbArg = object;
      irq = SPI1_IRQn;
      break;
    case (uint32_t)SPI2:
      SPI2_Cb = handler; SPI2_CbArg = object;
      irq = SPI2_IRQn;
      break;
    default: return;
  }
  NVIC_SetPriority(irq, EncodePriority(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableSPI(void *spi)
{
  IRQn_Type irq;
  switch((uint32_t)spi) {
    case (uint32_t)SPI1: irq = SPI1_IRQn; break;
    case (uint32_t)SPI2: irq = SPI2_IRQn; break;
    default: return;
  }
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- ADC

void IRQ_EnableADC(IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  ADC_Cb = handler;
  ADC_CbArg = object;
  NVIC_SetPriority(ADC1_IRQn, EncodePriority(priority));
  NVIC_EnableIRQ(ADC1_IRQn);
}

void IRQ_DisableADC(void)
{
  NVIC_DisableIRQ(ADC1_IRQn);
}

//------------------------------------------------------------------------------------------------- DMA

void IRQ_EnableDMA(DMA_CHx_t channel, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQn_Type irq;
  switch(channel) {
    case DMA_CH1:  DMA_CH1_Cb = handler;  DMA_CH1_CbArg = object;  irq = DMA1_Channel1_IRQn; break;
    case DMA_CH2:  DMA_CH2_Cb = handler;  DMA_CH2_CbArg = object;  irq = DMA1_Channel2_IRQn; break;
    case DMA_CH3:  DMA_CH3_Cb = handler;  DMA_CH3_CbArg = object;  irq = DMA1_Channel3_IRQn; break;
    case DMA_CH4:  DMA_CH4_Cb = handler;  DMA_CH4_CbArg = object;  irq = DMA1_Channel4_IRQn; break;
    case DMA_CH5:  DMA_CH5_Cb = handler;  DMA_CH5_CbArg = object;  irq = DMA1_Channel5_IRQn; break;
    case DMA_CH6:  DMA_CH6_Cb = handler;  DMA_CH6_CbArg = object;  irq = DMA1_Channel6_IRQn; break;
    case DMA_CH7:  DMA_CH7_Cb = handler;  DMA_CH7_CbArg = object;  irq = DMA1_Channel7_IRQn; break;
    case DMA_CH8:  DMA_CH8_Cb = handler;  DMA_CH8_CbArg = object;  irq = DMA2_Channel1_IRQn; break;
    case DMA_CH9:  DMA_CH9_Cb = handler;  DMA_CH9_CbArg = object;  irq = DMA2_Channel2_IRQn; break;
    case DMA_CH10: DMA_CH10_Cb = handler; DMA_CH10_CbArg = object; irq = DMA2_Channel3_IRQn; break;
    case DMA_CH11: DMA_CH11_Cb = handler; DMA_CH11_CbArg = object; irq = DMA2_Channel4_IRQn; break;
    case DMA_CH12: DMA_CH12_Cb = handler; DMA_CH12_CbArg = object; irq = DMA2_Channel5_IRQn; break;
    case DMA_CH13: DMA_CH13_Cb = handler; DMA_CH13_CbArg = object; irq = DMA2_Channel6_IRQn; break;
    case DMA_CH14: DMA_CH14_Cb = handler; DMA_CH14_CbArg = object; irq = DMA2_Channel7_IRQn; break;
    default: return;
  }
  NVIC_SetPriority(irq, EncodePriority(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableDMA(DMA_CHx_t channel)
{
  IRQn_Type irq;
  if(channel >= 1 && channel <= 7) irq = (IRQn_Type)(DMA1_Channel1_IRQn + channel - 1);
  else if(channel >= 8 && channel <= 14) irq = (IRQn_Type)(DMA2_Channel1_IRQn + channel - 8);
  else return;
  NVIC_DisableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- EXTI

void IRQ_EnableEXTI(uint8_t line, IRQ_Priority_t priority, IRQ_Handler_t handler, void *object)
{
  IRQn_Type irq;
  if(line <= 4) irq = (IRQn_Type)(EXTI0_IRQn + line);
  else if(line <= 9) irq = EXTI9_5_IRQn;
  else if(line <= 15) irq = EXTI15_10_IRQn;
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
  NVIC_SetPriority(irq, EncodePriority(priority));
  NVIC_EnableIRQ(irq);
}

void IRQ_DisableEXTI(uint8_t line)
{
  IRQn_Type irq;
  if(line <= 4) irq = (IRQn_Type)(EXTI0_IRQn + line);
  else if(line <= 9) irq = EXTI9_5_IRQn;
  else if(line <= 15) irq = EXTI15_10_IRQn;
  else return;
  NVIC_DisableIRQ(irq);
}

//-------------------------------------------------------------------------------------------------
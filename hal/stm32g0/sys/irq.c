#include "irq.h"

//------------------------------------------------------------------------------------------------- TIM

/**
 * @brief Enable TIM interrupt with callback.
 * @param tim TIM peripheral pointer
 * @param priority Interrupt priority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableTIM(TIM_TypeDef *tim, IRQ_Priority_t priority, void (*handler)(void *), void *object)
{
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:
      TIM1_IRQFnc = handler;
      TIM1_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM1, priority);
      NVIC_EnableIRQ(IRQ_TIM1);
      break;
    case (uint32_t)TIM2:
      TIM2_IRQFnc = handler;
      TIM2_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM2, priority);
      NVIC_EnableIRQ(IRQ_TIM2);
      break;
    case (uint32_t)TIM3:
      TIM3_IRQFnc = handler;
      TIM3_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM3_TIM4, priority);
      NVIC_EnableIRQ(IRQ_TIM3_TIM4);
      break;
    case (uint32_t)TIM6:
      TIM6_IRQFnc = handler;
      TIM6_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM6_DAC_LPTIM1, priority);
      NVIC_EnableIRQ(IRQ_TIM6_DAC_LPTIM1);
      break;
    case (uint32_t)TIM7:
      TIM7_IRQFnc = handler;
      TIM7_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM7_LPTIM2, priority);
      NVIC_EnableIRQ(IRQ_TIM7_LPTIM2);
      break;
    case (uint32_t)TIM14:
      TIM14_IRQFnc = handler;
      TIM14_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM14, priority);
      NVIC_EnableIRQ(IRQ_TIM14);
      break;
    case (uint32_t)TIM15:
      TIM15_IRQFnc = handler;
      TIM15_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM15, priority);
      NVIC_EnableIRQ(IRQ_TIM15);
      break;
    case (uint32_t)TIM16:
      TIM16_IRQFnc = handler;
      TIM16_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM16, priority);
      NVIC_EnableIRQ(IRQ_TIM16);
      break;
    case (uint32_t)TIM17:
      TIM17_IRQFnc = handler;
      TIM17_IRQSrc = object;
      NVIC_SetPriority(IRQ_TIM17, priority);
      NVIC_EnableIRQ(IRQ_TIM17);
      break;
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- UART

/**
 * @brief Enable UART interrupt with callback.
 * @param uart USART peripheral pointer
 * @param priority Interrupt priority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableUART(USART_TypeDef *uart, IRQ_Priority_t priority, void (*handler)(void *), void *object)
{
  switch((uint32_t)uart) {
    case (uint32_t)USART1:
      USART1_IRQFnc = handler;
      USART1_IRQSrc = object;
      NVIC_SetPriority(IRQ_UART1, priority);
      NVIC_EnableIRQ(IRQ_UART1);
      break;
    case (uint32_t)USART2:
      USART2_IRQFnc = handler;
      USART2_IRQSrc = object;
      NVIC_SetPriority(IRQ_UART2_LPUART2, priority);
      NVIC_EnableIRQ(IRQ_UART2_LPUART2);
      break;
    case (uint32_t)USART3:
      USART3_IRQFnc = handler;
      USART3_IRQSrc = object;
      NVIC_SetPriority(IRQ_UART3456_LPUART1, priority);
      NVIC_EnableIRQ(IRQ_UART3456_LPUART1);
      break;
    case (uint32_t)USART4:
      USART4_IRQFnc = handler;
      USART4_IRQSrc = object;
      NVIC_SetPriority(IRQ_UART3456_LPUART1, priority);
      NVIC_EnableIRQ(IRQ_UART3456_LPUART1);
      break;
    case (uint32_t)LPUART1:
      LPUART1_IRQFnc = handler;
      LPUART1_IRQSrc = object;
      NVIC_SetPriority(IRQ_UART3456_LPUART1, priority);
      NVIC_EnableIRQ(IRQ_UART3456_LPUART1);
      break;
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- I2C

/**
 * @brief Enable I2C interrupt with callback.
 * @param i2c I2C peripheral pointer
 * @param priority Interrupt priority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableI2C(I2C_TypeDef *i2c, IRQ_Priority_t priority, void (*handler)(void *), void *object)
{
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1:
      I2C1_IRQFnc = handler;
      I2C1_IRQSrc = object;
      NVIC_SetPriority(IRQ_I2C1, priority);
      NVIC_EnableIRQ(IRQ_I2C1);
      break;
    case (uint32_t)I2C2:
      I2C2_IRQFnc = handler;
      I2C2_IRQSrc = object;
      NVIC_SetPriority(IRQ_I2C23, priority);
      NVIC_EnableIRQ(IRQ_I2C23);
      break;
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- ADC

/**
 * @brief Enable ADC interrupt with callback.
 * @param priority Interrupt priority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableADC(IRQ_Priority_t priority, void (*handler)(void *), void *object)
{
  ADC_IRQFnc = handler;
  ADC_IRQSrc = object;
  NVIC_SetPriority(IRQ_ADC, priority);
  NVIC_EnableIRQ(IRQ_ADC);
}

//------------------------------------------------------------------------------------------------- DMA

/**
 * @brief Enable DMA channel interrupt with callback.
 * @param dma_nbr DMA channel number (1-based)
 * @param priority Interrupt priority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableDMA(DMA_Nbr_t dma_nbr, IRQ_Priority_t priority, void (*handler)(void *), void *object)
{
  switch(dma_nbr) {
    case DMA_Nbr_1:
      DMA1_IRQFnc = handler;
      DMA1_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH1, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH1);
      break;
    case DMA_Nbr_2:
      DMA2_IRQFnc = handler;
      DMA2_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH23, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH23);
      break;
    case DMA_Nbr_3:
      DMA3_IRQFnc = handler;
      DMA3_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH23, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH23);
      break;
    case DMA_Nbr_4:
      DMA4_IRQFnc = handler;
      DMA4_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
      break;
    case DMA_Nbr_5:
      DMA5_IRQFnc = handler;
      DMA5_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
      break;
    case DMA_Nbr_6:
      DMA6_IRQFnc = handler;
      DMA6_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
      break;
    case DMA_Nbr_7:
      DMA7_IRQFnc = handler;
      DMA7_IRQSrc = object;
      NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
      NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
      break;
    #ifdef STM32G0C1xx
      case DMA_Nbr_8:
        DMA8_IRQFnc = handler;
        DMA8_IRQSrc = object;
        NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
        NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
        break;
      case DMA_Nbr_9:
        DMA9_IRQFnc = handler;
        DMA9_IRQSrc = object;
        NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
        NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
        break;
      case DMA_Nbr_10:
        DMA10_IRQFnc = handler;
        DMA10_IRQSrc = object;
        NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
        NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
        break;
      case DMA_Nbr_11:
        DMA11_IRQFnc = handler;
        DMA11_IRQSrc = object;
        NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
        NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
        break;
      case DMA_Nbr_12:
        DMA12_IRQFnc = handler;
        DMA12_IRQSrc = object;
        NVIC_SetPriority(IRQ_DMA1_CH47_DMA2, priority);
        NVIC_EnableIRQ(IRQ_DMA1_CH47_DMA2);
        break;
    #endif
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- EXTI

/**
 * @brief Enable EXTI line interrupt with callback.
 * @param exti_nbr EXTI line number (0-15)
 * @param priority Interrupt priority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableEXTI(uint8_t exti_nbr, IRQ_Priority_t priority, void (*handler)(void *), void *object)
{
  IRQ_t irq;
  if(exti_nbr <= 1) irq = IRQ_EXTI01;
  else if(exti_nbr <= 3) irq = IRQ_EXTI23;
  else if(exti_nbr <= 15) irq = IRQ_EXTI4F;
  else return;
  switch(exti_nbr) {
    case 0:  EXTI0_IRQFnc = handler;  EXTI0_IRQSrc = object;  break;
    case 1:  EXTI1_IRQFnc = handler;  EXTI1_IRQSrc = object;  break;
    case 2:  EXTI2_IRQFnc = handler;  EXTI2_IRQSrc = object;  break;
    case 3:  EXTI3_IRQFnc = handler;  EXTI3_IRQSrc = object;  break;
    case 4:  EXTI4_IRQFnc = handler;  EXTI4_IRQSrc = object;  break;
    case 5:  EXTI5_IRQFnc = handler;  EXTI5_IRQSrc = object;  break;
    case 6:  EXTI6_IRQFnc = handler;  EXTI6_IRQSrc = object;  break;
    case 7:  EXTI7_IRQFnc = handler;  EXTI7_IRQSrc = object;  break;
    case 8:  EXTI8_IRQFnc = handler;  EXTI8_IRQSrc = object;  break;
    case 9:  EXTI9_IRQFnc = handler;  EXTI9_IRQSrc = object;  break;
    case 10: EXTI10_IRQFnc = handler; EXTI10_IRQSrc = object; break;
    case 11: EXTI11_IRQFnc = handler; EXTI11_IRQSrc = object; break;
    case 12: EXTI12_IRQFnc = handler; EXTI12_IRQSrc = object; break;
    case 13: EXTI13_IRQFnc = handler; EXTI13_IRQSrc = object; break;
    case 14: EXTI14_IRQFnc = handler; EXTI14_IRQSrc = object; break;
    case 15: EXTI15_IRQFnc = handler; EXTI15_IRQSrc = object; break;
    default: return;
  }
  NVIC_SetPriority(irq, priority);
  NVIC_EnableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- DMA utilities

/**
 * @brief Initialize DMA register pointers based on channel number.
 * @param[in] dma_nbr DMA channel number (1-based)
 * @param[out] dma Pointer to DMA_t structure to initialize
 */
void DMA_SetRegisters(DMA_Nbr_t dma_nbr, DMA_t *dma)
{
  if(dma_nbr == DMA_Nbr_None || !dma) return;
  if(dma_nbr <= DMA_Nbr_7) {
    dma->reg = DMA1;
    dma->pos = (uint8_t)(dma_nbr - 1);
    dma->cha = (DMA_Channel_TypeDef *)(DMA1_BASE + 8U + (20U * dma->pos));
  }
  #ifdef STM32G0C1xx
    else {
      dma->reg = DMA2;
      dma->pos = (uint8_t)(dma_nbr - 1 - DMA_Nbr_7);
      dma->cha = (DMA_Channel_TypeDef *)(DMA2_BASE + 8U + (20U * dma->pos));
    }
  #endif
  dma->mux = (DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + (4U * ((uint32_t)dma_nbr - 1U)));
}

/**
 * @brief Clear all interrupt flags for DMA channel.
 * @param[in] dma Pointer to DMA_t structure
 */
void DMA_ClearFlags(DMA_t *dma)
{
  if(!dma || !dma->reg) return;
  dma->reg->IFCR = (DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1) << (dma->pos * 4U);
}

//-------------------------------------------------------------------------------------------------
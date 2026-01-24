/**
 * @file  irq.c
 * @brief Interrupt enable functions for STM32WBxx
 */

#include "irq.h"

//------------------------------------------------------------------------------------------------- Helper

/**
 * @brief Encode priority for NVIC (4-bit priority with grouping)
 * @param priority Group priority (0-3)
 * @param subpriority Subpriority (0-3)
 * @return Encoded priority value
 */
static inline uint32_t IRQ_EncodePriority(IRQ_Priority_t priority, IRQ_Priority_t subpriority)
{
  return ((uint32_t)priority << 2U) | (uint32_t)subpriority;
}

//------------------------------------------------------------------------------------------------- Init

/**
 * @brief Configure NVIC priority grouping (4 group priorities, 4 subpriorities)
 */
void IRQ_Init(void)
{
  NVIC_SetPriorityGrouping(IRQ_GroupPriorities_4);
}

//------------------------------------------------------------------------------------------------- TIM

/**
 * @brief Enable TIM interrupt with callback
 * @param tim TIM peripheral pointer
 * @param priority Group priority
 * @param subpriority Subpriority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableTIM(TIM_TypeDef *tim, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object)
{
  uint32_t prio = IRQ_EncodePriority(priority, subpriority);
  switch((uint32_t)tim) {
    case (uint32_t)TIM1:
      TIM1_IRQFnc = handler;
      TIM1_IRQSrc = object;
      NVIC_SetPriority(TIM1_UP_TIM16_IRQn, prio);
      NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
      break;
    case (uint32_t)TIM2:
      TIM2_IRQFnc = handler;
      TIM2_IRQSrc = object;
      NVIC_SetPriority(TIM2_IRQn, prio);
      NVIC_EnableIRQ(TIM2_IRQn);
      break;
    case (uint32_t)TIM16:
      TIM16_IRQFnc = handler;
      TIM16_IRQSrc = object;
      NVIC_SetPriority(TIM1_UP_TIM16_IRQn, prio);
      NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
      break;
    case (uint32_t)TIM17:
      TIM17_IRQFnc = handler;
      TIM17_IRQSrc = object;
      NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, prio);
      NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
      break;
    case (uint32_t)LPTIM1:
      LPTIM1_IRQFnc = handler;
      LPTIM1_IRQSrc = object;
      NVIC_SetPriority(LPTIM1_IRQn, prio);
      NVIC_EnableIRQ(LPTIM1_IRQn);
      break;
    case (uint32_t)LPTIM2:
      LPTIM2_IRQFnc = handler;
      LPTIM2_IRQSrc = object;
      NVIC_SetPriority(LPTIM2_IRQn, prio);
      NVIC_EnableIRQ(LPTIM2_IRQn);
      break;
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- UART

/**
 * @brief Enable UART interrupt with callback
 * @param uart USART peripheral pointer
 * @param priority Group priority
 * @param subpriority Subpriority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableUART(USART_TypeDef *uart, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object)
{
  uint32_t prio = IRQ_EncodePriority(priority, subpriority);
  switch((uint32_t)uart) {
    case (uint32_t)USART1:
      USART1_IRQFnc = handler;
      USART1_IRQSrc = object;
      NVIC_SetPriority(USART1_IRQn, prio);
      NVIC_EnableIRQ(USART1_IRQn);
      break;
    case (uint32_t)LPUART1:
      LPUART1_IRQFnc = handler;
      LPUART1_IRQSrc = object;
      NVIC_SetPriority(LPUART1_IRQn, prio);
      NVIC_EnableIRQ(LPUART1_IRQn);
      break;
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- I2C

/**
 * @brief Enable I2C interrupt with event and optional error callback
 * @param i2c I2C peripheral pointer
 * @param priority Group priority
 * @param subpriority Subpriority
 * @param event Event callback function
 * @param error Error callback function (optional, can be NULL)
 * @param object Argument passed to callbacks
 */
void IRQ_EnableI2C(I2C_TypeDef *i2c, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*event)(void *), void (*error)(void *), void *object)
{
  uint32_t prio = IRQ_EncodePriority(priority, subpriority);
  switch((uint32_t)i2c) {
    case (uint32_t)I2C1:
      I2C1_Event_IRQFnc = event;
      I2C1_IRQSrc = object;
      NVIC_SetPriority(I2C1_EV_IRQn, prio);
      NVIC_EnableIRQ(I2C1_EV_IRQn);
      if(error) {
        I2C1_Error_IRQFnc = error;
        NVIC_SetPriority(I2C1_ER_IRQn, prio);
        NVIC_EnableIRQ(I2C1_ER_IRQn);
      }
      break;
    case (uint32_t)I2C3:
      I2C3_Event_IRQFnc = event;
      I2C3_IRQSrc = object;
      NVIC_SetPriority(I2C3_EV_IRQn, prio);
      NVIC_EnableIRQ(I2C3_EV_IRQn);
      if(error) {
        I2C3_Error_IRQFnc = error;
        NVIC_SetPriority(I2C3_ER_IRQn, prio);
        NVIC_EnableIRQ(I2C3_ER_IRQn);
      }
      break;
    default: break;
  }
}

//------------------------------------------------------------------------------------------------- ADC

/**
 * @brief Enable ADC interrupt with callback
 * @param priority Group priority
 * @param subpriority Subpriority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableADC(IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object)
{
  ADC_IRQFnc = handler;
  ADC_IRQSrc = object;
  NVIC_SetPriority(ADC1_IRQn, IRQ_EncodePriority(priority, subpriority));
  NVIC_EnableIRQ(ADC1_IRQn);
}

//------------------------------------------------------------------------------------------------- DMA

/**
 * @brief Enable DMA channel interrupt with callback
 * @param dma_nbr DMA channel number (1-14: 1-7 = DMA1, 8-14 = DMA2)
 * @param priority Group priority
 * @param subpriority Subpriority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableDMA(uint8_t dma_nbr, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object)
{
  uint32_t prio = IRQ_EncodePriority(priority, subpriority);
  IRQn_Type irq;

  switch(dma_nbr) {
    // DMA1 channels
    case 1:
      DMA1_CH1_IRQFnc = handler;
      DMA1_CH1_IRQSrc = object;
      irq = DMA1_Channel1_IRQn;
      break;
    case 2:
      DMA1_CH2_IRQFnc = handler;
      DMA1_CH2_IRQSrc = object;
      irq = DMA1_Channel2_IRQn;
      break;
    case 3:
      DMA1_CH3_IRQFnc = handler;
      DMA1_CH3_IRQSrc = object;
      irq = DMA1_Channel3_IRQn;
      break;
    case 4:
      DMA1_CH4_IRQFnc = handler;
      DMA1_CH4_IRQSrc = object;
      irq = DMA1_Channel4_IRQn;
      break;
    case 5:
      DMA1_CH5_IRQFnc = handler;
      DMA1_CH5_IRQSrc = object;
      irq = DMA1_Channel5_IRQn;
      break;
    case 6:
      DMA1_CH6_IRQFnc = handler;
      DMA1_CH6_IRQSrc = object;
      irq = DMA1_Channel6_IRQn;
      break;
    case 7:
      DMA1_CH7_IRQFnc = handler;
      DMA1_CH7_IRQSrc = object;
      irq = DMA1_Channel7_IRQn;
      break;
    // DMA2 channels (8-14 → CH1-CH7)
    case 8:
      DMA2_CH1_IRQFnc = handler;
      DMA2_CH1_IRQSrc = object;
      irq = DMA2_Channel1_IRQn;
      break;
    case 9:
      DMA2_CH2_IRQFnc = handler;
      DMA2_CH2_IRQSrc = object;
      irq = DMA2_Channel2_IRQn;
      break;
    case 10:
      DMA2_CH3_IRQFnc = handler;
      DMA2_CH3_IRQSrc = object;
      irq = DMA2_Channel3_IRQn;
      break;
    case 11:
      DMA2_CH4_IRQFnc = handler;
      DMA2_CH4_IRQSrc = object;
      irq = DMA2_Channel4_IRQn;
      break;
    case 12:
      DMA2_CH5_IRQFnc = handler;
      DMA2_CH5_IRQSrc = object;
      irq = DMA2_Channel5_IRQn;
      break;
    case 13:
      DMA2_CH6_IRQFnc = handler;
      DMA2_CH6_IRQSrc = object;
      irq = DMA2_Channel6_IRQn;
      break;
    case 14:
      DMA2_CH7_IRQFnc = handler;
      DMA2_CH7_IRQSrc = object;
      irq = DMA2_Channel7_IRQn;
      break;
    default:
      return;
  }
  NVIC_SetPriority(irq, prio);
  NVIC_EnableIRQ(irq);
}

//------------------------------------------------------------------------------------------------- EXTI

/**
 * @brief Enable EXTI line interrupt with callback
 * @param exti_nbr EXTI line number (0-15)
 * @param priority Group priority
 * @param subpriority Subpriority
 * @param handler Callback function
 * @param object Argument passed to callback
 */
void IRQ_EnableEXTI(uint8_t exti_nbr, IRQ_Priority_t priority, IRQ_Priority_t subpriority, void (*handler)(void *), void *object)
{
  uint32_t prio = IRQ_EncodePriority(priority, subpriority);
  IRQn_Type irq;
  // Determine IRQ based on EXTI line
  if(exti_nbr <= 4) irq = (IRQn_Type)(EXTI0_IRQn + exti_nbr);
  else if(exti_nbr <= 9) irq = EXTI9_5_IRQn;
  else if(exti_nbr <= 15) irq = EXTI15_10_IRQn;
  else return;
  // Set callback
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
  NVIC_SetPriority(irq, prio);
  NVIC_EnableIRQ(irq);
}

//-------------------------------------------------------------------------------------------------
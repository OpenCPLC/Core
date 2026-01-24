#ifndef STARTUP_H_
#define STARTUP_H_

#include <stdint.h>

/**
 * @file  startup.h
 * @brief IRQ callback declarations for STM32WBxx
 */

//------------------------------------------------------------------------------------------------- ADC

extern void (* volatile ADC_IRQFnc)(void *);
extern void * volatile ADC_IRQSrc;

//------------------------------------------------------------------------------------------------- EXTI

extern void (* volatile EXTI0_IRQFnc)(void *);  extern void * volatile EXTI0_IRQSrc;
extern void (* volatile EXTI1_IRQFnc)(void *);  extern void * volatile EXTI1_IRQSrc;
extern void (* volatile EXTI2_IRQFnc)(void *);  extern void * volatile EXTI2_IRQSrc;
extern void (* volatile EXTI3_IRQFnc)(void *);  extern void * volatile EXTI3_IRQSrc;
extern void (* volatile EXTI4_IRQFnc)(void *);  extern void * volatile EXTI4_IRQSrc;
extern void (* volatile EXTI5_IRQFnc)(void *);  extern void * volatile EXTI5_IRQSrc;
extern void (* volatile EXTI6_IRQFnc)(void *);  extern void * volatile EXTI6_IRQSrc;
extern void (* volatile EXTI7_IRQFnc)(void *);  extern void * volatile EXTI7_IRQSrc;
extern void (* volatile EXTI8_IRQFnc)(void *);  extern void * volatile EXTI8_IRQSrc;
extern void (* volatile EXTI9_IRQFnc)(void *);  extern void * volatile EXTI9_IRQSrc;
extern void (* volatile EXTI10_IRQFnc)(void *); extern void * volatile EXTI10_IRQSrc;
extern void (* volatile EXTI11_IRQFnc)(void *); extern void * volatile EXTI11_IRQSrc;
extern void (* volatile EXTI12_IRQFnc)(void *); extern void * volatile EXTI12_IRQSrc;
extern void (* volatile EXTI13_IRQFnc)(void *); extern void * volatile EXTI13_IRQSrc;
extern void (* volatile EXTI14_IRQFnc)(void *); extern void * volatile EXTI14_IRQSrc;
extern void (* volatile EXTI15_IRQFnc)(void *); extern void * volatile EXTI15_IRQSrc;

//------------------------------------------------------------------------------------------------- DMA1

extern void (* volatile DMA1_CH1_IRQFnc)(void *); extern void * volatile DMA1_CH1_IRQSrc;
extern void (* volatile DMA1_CH2_IRQFnc)(void *); extern void * volatile DMA1_CH2_IRQSrc;
extern void (* volatile DMA1_CH3_IRQFnc)(void *); extern void * volatile DMA1_CH3_IRQSrc;
extern void (* volatile DMA1_CH4_IRQFnc)(void *); extern void * volatile DMA1_CH4_IRQSrc;
extern void (* volatile DMA1_CH5_IRQFnc)(void *); extern void * volatile DMA1_CH5_IRQSrc;
extern void (* volatile DMA1_CH6_IRQFnc)(void *); extern void * volatile DMA1_CH6_IRQSrc;
extern void (* volatile DMA1_CH7_IRQFnc)(void *); extern void * volatile DMA1_CH7_IRQSrc;

//------------------------------------------------------------------------------------------------- DMA2

extern void (* volatile DMA2_CH1_IRQFnc)(void *); extern void * volatile DMA2_CH1_IRQSrc;
extern void (* volatile DMA2_CH2_IRQFnc)(void *); extern void * volatile DMA2_CH2_IRQSrc;
extern void (* volatile DMA2_CH3_IRQFnc)(void *); extern void * volatile DMA2_CH3_IRQSrc;
extern void (* volatile DMA2_CH4_IRQFnc)(void *); extern void * volatile DMA2_CH4_IRQSrc;
extern void (* volatile DMA2_CH5_IRQFnc)(void *); extern void * volatile DMA2_CH5_IRQSrc;
extern void (* volatile DMA2_CH6_IRQFnc)(void *); extern void * volatile DMA2_CH6_IRQSrc;
extern void (* volatile DMA2_CH7_IRQFnc)(void *); extern void * volatile DMA2_CH7_IRQSrc;

//------------------------------------------------------------------------------------------------- TIM

extern void (* volatile TIM1_IRQFnc)(void *);  extern void * volatile TIM1_IRQSrc;
extern void (* volatile TIM2_IRQFnc)(void *);  extern void * volatile TIM2_IRQSrc;
extern void (* volatile TIM16_IRQFnc)(void *); extern void * volatile TIM16_IRQSrc;
extern void (* volatile TIM17_IRQFnc)(void *); extern void * volatile TIM17_IRQSrc;

//------------------------------------------------------------------------------------------------- LPTIM

extern void (* volatile LPTIM1_IRQFnc)(void *); extern void * volatile LPTIM1_IRQSrc;
extern void (* volatile LPTIM2_IRQFnc)(void *); extern void * volatile LPTIM2_IRQSrc;

//------------------------------------------------------------------------------------------------- I2C

extern void (* volatile I2C1_Event_IRQFnc)(void *); extern void * volatile I2C1_IRQSrc;
extern void (* volatile I2C1_Error_IRQFnc)(void *);
extern void (* volatile I2C3_Event_IRQFnc)(void *); extern void * volatile I2C3_IRQSrc;
extern void (* volatile I2C3_Error_IRQFnc)(void *);

//------------------------------------------------------------------------------------------------- UART

extern void (* volatile USART1_IRQFnc)(void *);  extern void * volatile USART1_IRQSrc;
extern void (* volatile LPUART1_IRQFnc)(void *); extern void * volatile LPUART1_IRQSrc;

//-------------------------------------------------------------------------------------------------

#endif
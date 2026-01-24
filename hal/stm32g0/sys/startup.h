#ifndef STARTUP_H_
#define STARTUP_H_

#include <stdint.h>

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

//------------------------------------------------------------------------------------------------- DMA

extern void (* volatile DMA1_IRQFnc)(void *);  extern void * volatile DMA1_IRQSrc;
extern void (* volatile DMA2_IRQFnc)(void *);  extern void * volatile DMA2_IRQSrc;
extern void (* volatile DMA3_IRQFnc)(void *);  extern void * volatile DMA3_IRQSrc;
extern void (* volatile DMA4_IRQFnc)(void *);  extern void * volatile DMA4_IRQSrc;
extern void (* volatile DMA5_IRQFnc)(void *);  extern void * volatile DMA5_IRQSrc;
extern void (* volatile DMA6_IRQFnc)(void *);  extern void * volatile DMA6_IRQSrc;
extern void (* volatile DMA7_IRQFnc)(void *);  extern void * volatile DMA7_IRQSrc;
#ifdef STM32G0C1xx
  extern void (* volatile DMA8_IRQFnc)(void *);  extern void * volatile DMA8_IRQSrc;
  extern void (* volatile DMA9_IRQFnc)(void *);  extern void * volatile DMA9_IRQSrc;
  extern void (* volatile DMA10_IRQFnc)(void *); extern void * volatile DMA10_IRQSrc;
  extern void (* volatile DMA11_IRQFnc)(void *); extern void * volatile DMA11_IRQSrc;
  extern void (* volatile DMA12_IRQFnc)(void *); extern void * volatile DMA12_IRQSrc;
#endif

//------------------------------------------------------------------------------------------------- TIM

extern void (* volatile TIM1_IRQFnc)(void *);  extern void * volatile TIM1_IRQSrc;
extern void (* volatile TIM2_IRQFnc)(void *);  extern void * volatile TIM2_IRQSrc;
extern void (* volatile TIM3_IRQFnc)(void *);  extern void * volatile TIM3_IRQSrc;
extern void (* volatile TIM6_IRQFnc)(void *);  extern void * volatile TIM6_IRQSrc;
extern void (* volatile TIM7_IRQFnc)(void *);  extern void * volatile TIM7_IRQSrc;
extern void (* volatile TIM14_IRQFnc)(void *); extern void * volatile TIM14_IRQSrc;
extern void (* volatile TIM15_IRQFnc)(void *); extern void * volatile TIM15_IRQSrc;
extern void (* volatile TIM16_IRQFnc)(void *); extern void * volatile TIM16_IRQSrc;
extern void (* volatile TIM17_IRQFnc)(void *); extern void * volatile TIM17_IRQSrc;

//------------------------------------------------------------------------------------------------- I2C

extern void (* volatile I2C1_IRQFnc)(void *); extern void * volatile I2C1_IRQSrc;
extern void (* volatile I2C2_IRQFnc)(void *); extern void * volatile I2C2_IRQSrc;

//------------------------------------------------------------------------------------------------- UART

extern void (* volatile USART1_IRQFnc)(void *);  extern void * volatile USART1_IRQSrc;
extern void (* volatile USART2_IRQFnc)(void *);  extern void * volatile USART2_IRQSrc;
extern void (* volatile USART3_IRQFnc)(void *);  extern void * volatile USART3_IRQSrc;
extern void (* volatile USART4_IRQFnc)(void *);  extern void * volatile USART4_IRQSrc;
extern void (* volatile LPUART1_IRQFnc)(void *); extern void * volatile LPUART1_IRQSrc;

//-------------------------------------------------------------------------------------------------
#endif
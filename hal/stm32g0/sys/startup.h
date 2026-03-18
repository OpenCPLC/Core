// hal/stm32g0/sys/startup.h

#ifndef STARTUP_H_
#define STARTUP_H_

#include <stdint.h>
#include "system_stm32g0xx.h"
#include "stm32g0xx.h"

//------------------------------------------------------------------------------------------------- ADC

extern void (* volatile ADC_Cb)(void *);
extern void * volatile ADC_CbArg;

//------------------------------------------------------------------------------------------------- EXTI

extern void (* volatile EXTI0_Cb)(void *);  extern void * volatile EXTI0_CbArg;
extern void (* volatile EXTI1_Cb)(void *);  extern void * volatile EXTI1_CbArg;
extern void (* volatile EXTI2_Cb)(void *);  extern void * volatile EXTI2_CbArg;
extern void (* volatile EXTI3_Cb)(void *);  extern void * volatile EXTI3_CbArg;
extern void (* volatile EXTI4_Cb)(void *);  extern void * volatile EXTI4_CbArg;
extern void (* volatile EXTI5_Cb)(void *);  extern void * volatile EXTI5_CbArg;
extern void (* volatile EXTI6_Cb)(void *);  extern void * volatile EXTI6_CbArg;
extern void (* volatile EXTI7_Cb)(void *);  extern void * volatile EXTI7_CbArg;
extern void (* volatile EXTI8_Cb)(void *);  extern void * volatile EXTI8_CbArg;
extern void (* volatile EXTI9_Cb)(void *);  extern void * volatile EXTI9_CbArg;
extern void (* volatile EXTI10_Cb)(void *); extern void * volatile EXTI10_CbArg;
extern void (* volatile EXTI11_Cb)(void *); extern void * volatile EXTI11_CbArg;
extern void (* volatile EXTI12_Cb)(void *); extern void * volatile EXTI12_CbArg;
extern void (* volatile EXTI13_Cb)(void *); extern void * volatile EXTI13_CbArg;
extern void (* volatile EXTI14_Cb)(void *); extern void * volatile EXTI14_CbArg;
extern void (* volatile EXTI15_Cb)(void *); extern void * volatile EXTI15_CbArg;

//------------------------------------------------------------------------------------------------- DMA

extern void (* volatile DMA_CH1_Cb)(void *);  extern void * volatile DMA_CH1_CbArg;
extern void (* volatile DMA_CH2_Cb)(void *);  extern void * volatile DMA_CH2_CbArg;
extern void (* volatile DMA_CH3_Cb)(void *);  extern void * volatile DMA_CH3_CbArg;
extern void (* volatile DMA_CH4_Cb)(void *);  extern void * volatile DMA_CH4_CbArg;
extern void (* volatile DMA_CH5_Cb)(void *);  extern void * volatile DMA_CH5_CbArg;
extern void (* volatile DMA_CH6_Cb)(void *);  extern void * volatile DMA_CH6_CbArg;
extern void (* volatile DMA_CH7_Cb)(void *);  extern void * volatile DMA_CH7_CbArg;
#ifdef DMA2
extern void (* volatile DMA_CH8_Cb)(void *);  extern void * volatile DMA_CH8_CbArg;
extern void (* volatile DMA_CH9_Cb)(void *);  extern void * volatile DMA_CH9_CbArg;
extern void (* volatile DMA_CH10_Cb)(void *); extern void * volatile DMA_CH10_CbArg;
extern void (* volatile DMA_CH11_Cb)(void *); extern void * volatile DMA_CH11_CbArg;
extern void (* volatile DMA_CH12_Cb)(void *); extern void * volatile DMA_CH12_CbArg;
#endif

//------------------------------------------------------------------------------------------------- TIM

extern void (* volatile TIM1_Cb)(void *);  extern void * volatile TIM1_CbArg;
extern void (* volatile TIM2_Cb)(void *);  extern void * volatile TIM2_CbArg;
extern void (* volatile TIM3_Cb)(void *);  extern void * volatile TIM3_CbArg;
#ifdef TIM4
extern void (* volatile TIM4_Cb)(void *);  extern void * volatile TIM4_CbArg;
#endif
extern void (* volatile TIM6_Cb)(void *);  extern void * volatile TIM6_CbArg;
extern void (* volatile TIM7_Cb)(void *);  extern void * volatile TIM7_CbArg;
extern void (* volatile TIM14_Cb)(void *); extern void * volatile TIM14_CbArg;
extern void (* volatile TIM15_Cb)(void *); extern void * volatile TIM15_CbArg;
extern void (* volatile TIM16_Cb)(void *); extern void * volatile TIM16_CbArg;
extern void (* volatile TIM17_Cb)(void *); extern void * volatile TIM17_CbArg;

//------------------------------------------------------------------------------------------------- I2C

extern void (* volatile I2C1_Cb)(void *); extern void * volatile I2C1_CbArg;
extern void (* volatile I2C2_Cb)(void *); extern void * volatile I2C2_CbArg;
#ifdef I2C3
extern void (* volatile I2C3_Cb)(void *); extern void * volatile I2C3_CbArg;
#endif

//------------------------------------------------------------------------------------------------- SPI

extern void (* volatile SPI1_Cb)(void *); extern void * volatile SPI1_CbArg;
extern void (* volatile SPI2_Cb)(void *); extern void * volatile SPI2_CbArg;
#ifdef SPI3
extern void (* volatile SPI3_Cb)(void *); extern void * volatile SPI3_CbArg;
#endif

//------------------------------------------------------------------------------------------------- UART

extern void (* volatile USART1_Cb)(void *);  extern void * volatile USART1_CbArg;
extern void (* volatile USART2_Cb)(void *);  extern void * volatile USART2_CbArg;
extern void (* volatile USART3_Cb)(void *);  extern void * volatile USART3_CbArg;
extern void (* volatile USART4_Cb)(void *);  extern void * volatile USART4_CbArg;
#ifdef USART5
extern void (* volatile USART5_Cb)(void *);  extern void * volatile USART5_CbArg;
#endif
#ifdef USART6
extern void (* volatile USART6_Cb)(void *);  extern void * volatile USART6_CbArg;
#endif
extern void (* volatile LPUART1_Cb)(void *); extern void * volatile LPUART1_CbArg;
#ifdef LPUART2
extern void (* volatile LPUART2_Cb)(void *); extern void * volatile LPUART2_CbArg;
#endif

//-------------------------------------------------------------------------------------------------

#endif
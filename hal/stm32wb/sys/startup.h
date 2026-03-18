// hal/stm32wb/sys/startup.h

#ifndef STARTUP_H_
#define STARTUP_H_

#include <stdint.h>
#include "system_stm32wbxx.h"
#include "stm32wbxx.h"

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

//------------------------------------------------------------------------------------------------- DMA1

extern void (* volatile DMA_CH1_Cb)(void *);  extern void * volatile DMA_CH1_CbArg;
extern void (* volatile DMA_CH2_Cb)(void *);  extern void * volatile DMA_CH2_CbArg;
extern void (* volatile DMA_CH3_Cb)(void *);  extern void * volatile DMA_CH3_CbArg;
extern void (* volatile DMA_CH4_Cb)(void *);  extern void * volatile DMA_CH4_CbArg;
extern void (* volatile DMA_CH5_Cb)(void *);  extern void * volatile DMA_CH5_CbArg;
extern void (* volatile DMA_CH6_Cb)(void *);  extern void * volatile DMA_CH6_CbArg;
extern void (* volatile DMA_CH7_Cb)(void *);  extern void * volatile DMA_CH7_CbArg;

//------------------------------------------------------------------------------------------------- DMA2

extern void (* volatile DMA_CH8_Cb)(void *);  extern void * volatile DMA_CH8_CbArg;
extern void (* volatile DMA_CH9_Cb)(void *);  extern void * volatile DMA_CH9_CbArg;
extern void (* volatile DMA_CH10_Cb)(void *); extern void * volatile DMA_CH10_CbArg;
extern void (* volatile DMA_CH11_Cb)(void *); extern void * volatile DMA_CH11_CbArg;
extern void (* volatile DMA_CH12_Cb)(void *); extern void * volatile DMA_CH12_CbArg;
extern void (* volatile DMA_CH13_Cb)(void *); extern void * volatile DMA_CH13_CbArg;
extern void (* volatile DMA_CH14_Cb)(void *); extern void * volatile DMA_CH14_CbArg;

//------------------------------------------------------------------------------------------------- TIM

extern void (* volatile TIM1_Cb)(void *);  extern void * volatile TIM1_CbArg;
extern void (* volatile TIM2_Cb)(void *);  extern void * volatile TIM2_CbArg;
extern void (* volatile TIM16_Cb)(void *); extern void * volatile TIM16_CbArg;
extern void (* volatile TIM17_Cb)(void *); extern void * volatile TIM17_CbArg;

//------------------------------------------------------------------------------------------------- LPTIM

extern void (* volatile LPTIM1_Cb)(void *); extern void * volatile LPTIM1_CbArg;
extern void (* volatile LPTIM2_Cb)(void *); extern void * volatile LPTIM2_CbArg;

//------------------------------------------------------------------------------------------------- I2C

extern void (* volatile I2C1_EventCallback)(void *); extern void * volatile I2C1_CbArg;
extern void (* volatile I2C1_ErrorCallback)(void *);
extern void (* volatile I2C3_EventCallback)(void *); extern void * volatile I2C3_CbArg;
extern void (* volatile I2C3_ErrorCallback)(void *);

//------------------------------------------------------------------------------------------------- SPI

extern void (* volatile SPI1_Cb)(void *); extern void * volatile SPI1_CbArg;
extern void (* volatile SPI2_Cb)(void *); extern void * volatile SPI2_CbArg;

//------------------------------------------------------------------------------------------------- UART

extern void (* volatile USART1_Cb)(void *);  extern void * volatile USART1_CbArg;
extern void (* volatile LPUART1_Cb)(void *); extern void * volatile LPUART1_CbArg;

//-------------------------------------------------------------------------------------------------

#endif

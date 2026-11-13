// hal/host/inc/device.h

#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdint.h>

//-------------------------------------------------------------------------------------------------

/**
 * `hal/host` is the chip of a test build: a model answers as the hardware does,
 * and a test checks the state it keeps, never the calls a module made.
 * What a test reaches past the driver API is named like `FLASH_HostCut` and `UART_HostSent`.
 * Logic of `hal/stm32*` sits in a file of its own beside the registers,
 * and the twin here includes that file:
 * logic written again for the host is a copy, and a test of a copy says nothing of the firmware,
 * so a module moves its logic out at its next change.
 * A thread loop and an interrupt get no model of VRTS or timers:
 * the loop body is a step function, the handler a plain one, and a test calls both directly,
 * which gives what fibers would at a fraction of the cost.
 */

/**
 * Register blocks a host build compiles against.
 * Only the fields the headers and the drivers reach are here:
 * a block is plain memory that reads back what was written,
 * which is all a peripheral can offer without silicon behind it.
 */

#ifndef HOST_GPIO_PORTS
  // Ports the build provides, `GPIOA` upwards
  #define HOST_GPIO_PORTS 6
#endif

#ifndef HOST_TIM_COUNT
  // Timers the build provides
  #define HOST_TIM_COUNT 9
#endif

typedef struct { volatile uint32_t ODR; } GPIO_TypeDef;

typedef struct {
  volatile uint32_t CR1, DIER, SR, EGR, CNT, PSC, ARR;
  volatile uint32_t CCR1, CCR2, CCR3, CCR4, BDTR;
} TIM_TypeDef;

typedef struct { volatile uint32_t DR; } ADC_TypeDef;
typedef struct { volatile uint32_t ISR; } DMA_TypeDef;
typedef struct { volatile uint32_t CCR; } DMA_Channel_TypeDef;
typedef struct { volatile uint32_t CCR; } DMAMUX_Channel_TypeDef;

// Buses a board file names and no host driver reaches: one register makes each a type of its own
typedef struct { volatile uint32_t CR1; } I2C_TypeDef;
typedef struct { volatile uint32_t CR1; } SPI_TypeDef;
typedef struct { volatile uint32_t CR1; } USART_TypeDef;

extern GPIO_TypeDef HostGpio[HOST_GPIO_PORTS];
extern TIM_TypeDef HostTim[HOST_TIM_COUNT];
extern ADC_TypeDef HostAdc;
extern I2C_TypeDef HostI2c[3];
extern SPI_TypeDef HostSpi[3];
extern USART_TypeDef HostUsart[8];

#define GPIOA (&HostGpio[0])
#define GPIOB (&HostGpio[1])
#define GPIOC (&HostGpio[2])
#define GPIOD (&HostGpio[3])
#define GPIOE (&HostGpio[4])
#define GPIOF (&HostGpio[5])

#define TIM1  (&HostTim[0])
#define TIM2  (&HostTim[1])
#define TIM3  (&HostTim[2])
#define TIM6  (&HostTim[3])
#define TIM7  (&HostTim[4])
#define TIM14 (&HostTim[5])
#define TIM15 (&HostTim[6])
#define TIM16 (&HostTim[7])
#define TIM17 (&HostTim[8])
#define ADC1  (&HostAdc)

#define I2C1 (&HostI2c[0])
#define I2C2 (&HostI2c[1])
#define I2C3 (&HostI2c[2])

#define SPI1 (&HostSpi[0])
#define SPI2 (&HostSpi[1])
#define SPI3 (&HostSpi[2])

#define USART1  (&HostUsart[0])
#define USART2  (&HostUsart[1])
#define USART3  (&HostUsart[2])
#define USART4  (&HostUsart[3])
#define USART5  (&HostUsart[4])
#define USART6  (&HostUsart[5])
#define LPUART1 (&HostUsart[6])
#define LPUART2 (&HostUsart[7])

// Bit positions follow the silicon, so code reaching a register reads what it expects
#define TIM_CR1_CEN      (1u << 0)
#define TIM_CR1_CEN_Msk  TIM_CR1_CEN
#define TIM_CR1_DIR      (1u << 4)
#define TIM_DIER_UIE     (1u << 0)
#define TIM_SR_UIF       (1u << 0)
#define TIM_EGR_UG       (1u << 0)
#define TIM_BDTR_MOE     (1u << 15)

//-------------------------------------------------------------------------------------------------

#endif

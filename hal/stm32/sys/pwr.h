#ifndef PWR_H_
#define PWR_H_

#include <stdbool.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "main.h"

//------------------------------------------------------------------------------------------------- SYS

void RAMP_PA11_PA12(void);

//------------------------------------------------------------------------------------------------- RCC

typedef enum {
  RCC_HSI_16MHz = 0,
  RCC_HSI_8MHz = 1,
  RCC_HSI_4MHz = 2,
  RCC_HSI_2MHz = 3,
  RCC_HSI_1MHz = 4,
  RCC_HSI_500kHz = 5,
  RCC_HSI_250kHz = 6,
  RCC_HSI_125kHz = 7
} RCC_HSI_e;

extern uint32_t rcc_xtal_value;

void RCC_EnableTIM(TIM_TypeDef *tim_typedef);
void RCC_EnableGPIO(GPIO_TypeDef *gpio_typedef);
void RCC_EnableUART(USART_TypeDef *uart_typedef);
void RCC_DisableUART(USART_TypeDef *uart_typedef);
void RCC_EnableI2C(I2C_TypeDef *i2c_typedef);
void RCC_DisableI2C(I2C_TypeDef *i2c_typedef);
void RCC_EnableSPI(SPI_TypeDef *spi_typedef);
void RCC_EnableDMA(DMA_TypeDef *dam_typedef);

uint32_t RCC_HSE(uint32_t xtal_value);
uint32_t RCC_PLL(uint32_t hse_xtal_value, uint8_t m, uint8_t n, uint8_t r);
uint32_t RCC_2MHz(void);
uint32_t RCC_16MHz(void);
uint32_t RCC_48MHz(void);
uint32_t RCC_64MHz(void);

//------------------------------------------------------------------------------------------------- PWR

#define PWR_SCR_CLEAR 0x013B

typedef enum {
  PWR_WakeupPin_PA0 = 0,
  PWR_WakeupPin_PA4 = 1, // 25-pin, 28-pin, 32-pin pacage
  PWR_WakeupPin_PC13 = 1, // 48-pin, 64-pin pacage
  PWR_WakeupPin_PA2 = 3,
  PWR_WakeupPin_PC5 = 4,
  PWR_WakeupPin_PB5 = 5
} PWR_WakeupPin_e;

typedef enum {
  PWR_WakeupDir_Rise = 0,
  PWR_WakeupDir_Fall = 1
} PWR_WakeupDir_e;

typedef enum {
  PWR_SleepMode_Error = -1,
  PWR_SleepMode_Stop0 = 0,
  PWR_SleepMode_Stop1 = 1,
  PWR_SleepMode_StandbySRAM = 2,
  PWR_SleepMode_Standby = 3,
  PWR_SleepMode_Shutdown = 4
} PWR_SleepMode_e;

void PWR_Reset(void);
void PWR_Sleep(PWR_SleepMode_e mode);
void PWR_Wakeup(PWR_WakeupPin_e wakeup_pin, PWR_WakeupDir_e dir);

//------------------------------------------------------------------------------------------------- BKPR

typedef enum {
  BKPR_0 = 0,
  BKPR_1 = 1,
  BKPR_2 = 2,
  BKPR_3 = 3,
  BKPR_4 = 4
} BKPR_e;

void BKPR_Write(BKPR_e reg, uint32_t value);
uint32_t BKPR_Read(BKPR_e reg);

//------------------------------------------------------------------------------------------------- IWDG

#define IWDG_REFRESH 0x0000AAAA
#define IWDG_WRITE_ACCESS 0x00005555
#define IWDG_START 0x0000CCCC

typedef enum {
	IWDG_Time_125us = 0,
	IWDG_Time_250us = 1,
	IWDG_Time_500us = 2,
	IWDG_Time_1ms = 3,
	IWDG_Time_2ms = 4,
	IWDG_Time_4ms = 5,
	IWDG_Time_8ms = 6
} IWDG_Time_e;

void IWDG_Init(IWDG_Time_e time, uint16_t reload_counter);
void IWDG_Refresh(void);
bool IWDG_Status(void);

//-------------------------------------------------------------------------------------------------
#endif

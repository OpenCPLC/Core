// hal/stm32/sys/pwr.h

#ifndef PWR_H_
#define PWR_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(STM32G0)
  #include "stm32g0xx.h"
  #include "pwr_g0.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
  #include "pwr_wb.h"
#endif

//------------------------------------------------------------------------------------------------- RCC: Clock Enable

void RCC_EnableTIM(void *tim);
void RCC_EnableGPIO(void *gpio);
void RCC_EnableUART(void *uart);
void RCC_DisableUART(void *uart);
void RCC_EnableI2C(void *i2c);
void RCC_DisableI2C(void *i2c);
void RCC_EnableSPI(void *spi);
void RCC_EnableDMA(void *dma);

//------------------------------------------------------------------------------------------------- RCC: System Clock

uint32_t RCC_GetClock(void);
uint32_t RCC_SetHSE(uint32_t xtal_Hz);
uint32_t RCC_SetPLL(uint32_t hse_Hz, uint8_t m, uint8_t n, uint8_t r);

uint32_t RCC_2MHz(void);
uint32_t RCC_16MHz(void);
uint32_t RCC_48MHz(void);
uint32_t RCC_64MHz(void);

//------------------------------------------------------------------------------------------------- PWR: Sleep modes

typedef enum {
  PWR_SleepMode_Stop0 = 0,
  PWR_SleepMode_Stop1 = 1,
  PWR_SleepMode_Stop2 = 2, // WB only (G0 maps to Stop1)
  PWR_SleepMode_StandbySRAM = 3,
  PWR_SleepMode_Standby = 4,
  PWR_SleepMode_Shutdown = 5,
  PWR_SleepMode_Error = 6
} PWR_SleepMode_t;

typedef enum {
  PWR_Edge_Rising = 0,
  PWR_Edge_Falling = 1
} PWR_Edge_t;

void PWR_Reset(void);
void PWR_Sleep(PWR_SleepMode_t mode);
void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge);

//------------------------------------------------------------------------------------------------- PWR: Backup registers

typedef enum {
  BKPR_0 = 0, BKPR_1, BKPR_2, BKPR_3, BKPR_4
} BKPR_t;

void BKPR_Write(BKPR_t reg, uint32_t value);
uint32_t BKPR_Read(BKPR_t reg);

//------------------------------------------------------------------------------------------------- IWDG: Watchdog

typedef enum {
	IWDG_Time_125us = 0,
	IWDG_Time_250us = 1,
	IWDG_Time_500us = 2,
	IWDG_Time_1ms = 3,
	IWDG_Time_2ms = 4,
	IWDG_Time_4ms = 5,
	IWDG_Time_8ms = 6
} IWDG_Time_t;

void IWDG_Init(IWDG_Time_t prescaler, uint16_t reload);
void IWDG_Refresh(void);
bool IWDG_WasReset(void);

//-------------------------------------------------------------------------------------------------

#endif
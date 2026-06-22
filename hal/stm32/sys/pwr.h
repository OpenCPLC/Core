// hal/stm32/sys/pwr.h

#ifndef PWR_H_
#define PWR_H_

#include <stdint.h>
#include <stdbool.h>
#include "xdef.h"

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

/**
 * @brief Reset the backup domain (`RCC_BDCR`, RTC, backup registers) after a power-on.
 * @note No-op except on a power-on reset, where the domain may be corrupt. Wipes RTC
 *       time and backup registers when it runs.
 */
void BKP_DomainReset(void);

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

//------------------------------------------------------------------------------------------------- BOR: Brown-Out Reset

// Brown-out reset threshold, stored in option bytes (`FLASH->OPTR`), non-volatile.
// Higher level resets earlier on supply droop, before MCU enters undefined state.
// `BOR_Level_1V7` leaves only always-on power-down detector. Voltages approximate.
typedef enum {
  BOR_Level_1V7 = 0, // ~1.7V power-down only (always on)
  BOR_Level_2V0 = 1, // rising ~2.0V
  BOR_Level_2V2 = 2, // rising ~2.2V
  BOR_Level_2V5 = 3, // rising ~2.5V
  BOR_Level_2V8 = 4  // rising ~2.8V
} BOR_Level_t;

/**
 * @brief Read currently programmed BOR threshold from option bytes.
 * @return Active `BOR_Level_t`
 */
BOR_Level_t BOR_GetLevel(void);

/**
 * @brief Set BOR threshold in option bytes (non-volatile).
 * @param[in] level Requested threshold
 * @return `OK` if level already matches (no-op), `ERR` on programming failure
 * @note Changing level reprograms option bytes and triggers `OBL_LAUNCH`: full reset,
 *       no return. Matching level is a no-op, safe to call every boot.
 * @note WB55: run before BLE stack (`CPU2`) takes the flash semaphore.
 */
status_t BOR_SetLevel(BOR_Level_t level);

/**
 * @brief Check if last reset came from BOR (brown-out).
 * @return `true` if BOR reset flag was set (cleared on read)
 */
bool BOR_WasReset(void);

//-------------------------------------------------------------------------------------------------

#endif
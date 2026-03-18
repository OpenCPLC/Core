// hal/host/pwr.h

#ifndef PWR_H_
#define PWR_H_

#include <stdint.h>
#include <stdbool.h>
#include "xdef.h"

//------------------------------------------------------------------------------------------------- RCC: Clock Enable (stubs)

static inline void RCC_EnableTIM(void *tim) { (void)tim; }
static inline void RCC_EnableGPIO(void *gpio) { (void)gpio; }
static inline void RCC_EnableUART(void *uart) { (void)uart; }
static inline void RCC_DisableUART(void *uart) { (void)uart; }
static inline void RCC_EnableI2C(void *i2c) { (void)i2c; }
static inline void RCC_DisableI2C(void *i2c) { (void)i2c; }
static inline void RCC_EnableSPI(void *spi) { (void)spi; }
static inline void RCC_EnableDMA(void *dma) { (void)dma; }

//------------------------------------------------------------------------------------------------- RCC: System Clock (stubs)

static inline uint32_t RCC_GetClock(void) { return 64000000; } // Fake 64MHz
static inline uint32_t RCC_SetHSE(uint32_t xtal_Hz) { return xtal_Hz; }
static inline uint32_t RCC_SetPLL(uint32_t hse_Hz, uint8_t m, uint8_t n, uint8_t r)
{
  (void)m; (void)n; (void)r;
  return hse_Hz ? hse_Hz : 64000000;
}

static inline uint32_t RCC_2MHz(void) { return 2000000; }
static inline uint32_t RCC_16MHz(void) { return 16000000; }
static inline uint32_t RCC_48MHz(void) { return 48000000; }
static inline uint32_t RCC_64MHz(void) { return 64000000; }

//------------------------------------------------------------------------------------------------- PWR: Sleep modes

/**
 * @brief Store program path for PWR_Reset()
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @note Call at start of main() to enable restart functionality
 */
void PWR_StoreArgs(int argc, char **argv);

typedef enum {
  PWR_SleepMode_Stop0 = 0,
  PWR_SleepMode_Stop1 = 1,
  PWR_SleepMode_Stop2 = 2,
  PWR_SleepMode_StandbySRAM = 3,
  PWR_SleepMode_Standby = 4,
  PWR_SleepMode_Shutdown = 5,
  PWR_SleepMode_Error = 6
} PWR_SleepMode_t;

typedef enum {
  PWR_Edge_Rising = 0,
  PWR_Edge_Falling = 1
} PWR_Edge_t;

// Wakeup pins (stub enum for API compatibility)
typedef enum {
  PWR_Wakeup_0 = 0,
  PWR_Wakeup_1 = 1,
  PWR_Wakeup_2 = 2,
  PWR_Wakeup_3 = 3,
  PWR_Wakeup_4 = 4,
  PWR_Wakeup_5 = 5
} PWR_WakeupPin_t;

/**
 * @brief Reset (restart) the application
 * @note On host: restarts the process using execv()
 */
void PWR_Reset(void);

/**
 * @brief Enter sleep mode (exit application)
 * @param mode Sleep mode (determines exit code)
 * @note On host: calls exit() with mode as exit code
 */
void PWR_Sleep(PWR_SleepMode_t mode);

/**
 * @brief Set wakeup pin (stub, does nothing on host)
 */
static inline void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge)
{
  (void)pin; (void)edge;
}

//------------------------------------------------------------------------------------------------- PWR: Backup registers (RAM-based simulation)

typedef enum {
  BKPR_0 = 0, BKPR_1, BKPR_2, BKPR_3, BKPR_4
} BKPR_t;

void BKPR_Write(BKPR_t reg, uint32_t value);
uint32_t BKPR_Read(BKPR_t reg);

//------------------------------------------------------------------------------------------------- IWDG: Watchdog (stubs)

typedef enum {
  IWDG_Prescaler_4 = 0,
  IWDG_Prescaler_8 = 1,
  IWDG_Prescaler_16 = 2,
  IWDG_Prescaler_32 = 3,
  IWDG_Prescaler_64 = 4,
  IWDG_Prescaler_128 = 5,
  IWDG_Prescaler_256 = 6
} IWDG_Prescaler_t;

static inline void IWDG_Init(IWDG_Prescaler_t prescaler, uint16_t reload)
{
  (void)prescaler; (void)reload;
}

static inline void IWDG_Refresh(void) {}
static inline bool IWDG_WasReset(void) { return false; }

//-------------------------------------------------------------------------------------------------

#endif
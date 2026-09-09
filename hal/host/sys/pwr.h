// hal/host/sys/pwr.h

#ifndef PWR_H_
#define PWR_H_

#include <stdint.h>
#include <stdbool.h>
#include "xdef.h"

//------------------------------------------------------------------------------- RCC: Clock Enable

// No clock tree off-target, every enable is accepted

static inline void RCC_EnableTIM(void *tim) { unused(tim); }
static inline void RCC_EnableGPIO(void *gpio) { unused(gpio); }
static inline void RCC_EnableUART(void *uart) { unused(uart); }
static inline void RCC_DisableUART(void *uart) { unused(uart); }
static inline void RCC_EnableI2C(void *i2c) { unused(i2c); }
static inline void RCC_DisableI2C(void *i2c) { unused(i2c); }
static inline void RCC_EnableSPI(void *spi) { unused(spi); }
static inline void RCC_EnableDMA(void *dma) { unused(dma); }
static inline void RCC_EnableUSB(void) {}
static inline void RCC_EnableCRC(void) {}
static inline void RCC_EnableRNG(void) {}

//------------------------------------------------------------------------------- RCC: System Clock

// The clock a host pretends to run at, the target default
#define HOST_CLOCK_Hz 64000000u

static inline uint32_t RCC_GetClock(void) { return HOST_CLOCK_Hz; }
static inline uint32_t RCC_SetHSE(uint32_t xtal_Hz) { return xtal_Hz; }
static inline uint32_t RCC_SetPLL(uint32_t hse_Hz, uint8_t m, uint8_t n, uint8_t r)
{
  unused(m); unused(n); unused(r);
  return hse_Hz ? hse_Hz : HOST_CLOCK_Hz;
}

static inline uint32_t RCC_2MHz(void) { return 2000000; }
static inline uint32_t RCC_16MHz(void) { return 16000000; }
static inline uint32_t RCC_48MHz(void) { return 48000000; }
static inline uint32_t RCC_64MHz(void) { return HOST_CLOCK_Hz; }

//--------------------------------------------------------------------------------------------- PWR

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

typedef enum {
  PWR_Wakeup_0 = 0,
  PWR_Wakeup_1 = 1,
  PWR_Wakeup_2 = 2,
  PWR_Wakeup_3 = 3,
  PWR_Wakeup_4 = 4,
  PWR_Wakeup_5 = 5
} PWR_WakeupPin_t;

typedef enum {
  PWR_Wakeup_Pin1 = (1 << 0),
  PWR_Wakeup_Pin2 = (1 << 1),
  PWR_Wakeup_Pin3 = (1 << 2),
  PWR_Wakeup_Pin4 = (1 << 3),
  PWR_Wakeup_Pin5 = (1 << 4)
} PWR_Wakeup_t;

// Program path and arguments, so `PWR_Reset` can restart the process; call from `main`
void PWR_StoreArgs(int argc, char **argv);

// Reset ends the process, sleep too: a message names the mode first
void PWR_Reset(void);
void PWR_Sleep(PWR_SleepMode_t mode);

static inline void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge)
{
  unused(pin); unused(edge);
}

// No power domain to leave on host, the request is refused
static inline status_t PWR_Shutdown(uint8_t wakeup_mask, uint8_t falling_mask)
{
  unused(wakeup_mask);
  unused(falling_mask);
  return ERR;
}

//-------------------------------------------------------------------------------------------- BKPR

// Backup registers in RAM, lost with the process

typedef enum {
  BKPR_0 = 0, BKPR_1, BKPR_2, BKPR_3, BKPR_4
} BKPR_t;

void BKPR_Write(BKPR_t reg, uint32_t value);
uint32_t BKPR_Read(BKPR_t reg);

// No backup domain on host, reset is a no-op
static inline void BKP_DomainReset(void) {}

//-------------------------------------------------------------------------------------------- IWDG

// No watchdog off-target, a refresh is accepted and never awaited

typedef enum {
  IWDG_Time_125us = 0,
  IWDG_Time_250us = 1,
  IWDG_Time_500us = 2,
  IWDG_Time_1ms = 3,
  IWDG_Time_2ms = 4,
  IWDG_Time_4ms = 5,
  IWDG_Time_8ms = 6
} IWDG_Time_t;

static inline void IWDG_Init(IWDG_Time_t prescaler, uint16_t reload)
{
  unused(prescaler); unused(reload);
}

static inline void IWDG_Init_ms(uint32_t timeout_ms) { unused(timeout_ms); }
static inline void IWDG_Refresh(void) {}
static inline bool IWDG_WasReset(void) { return false; }

//--------------------------------------------------------------------------------------------- BOR

// No supply rail to guard on host, BOR is a no-op
typedef enum {
  BOR_Level_1V7 = 0,
  BOR_Level_2V0 = 1,
  BOR_Level_2V2 = 2,
  BOR_Level_2V5 = 3,
  BOR_Level_2V8 = 4
} BOR_Level_t;

static inline BOR_Level_t BOR_GetLevel(void) { return BOR_Level_1V7; }
static inline status_t BOR_SetLevel(BOR_Level_t level) { unused(level); return OK; }
static inline bool BOR_WasReset(void) { return false; }

//-------------------------------------------------------------------------------------------------
#endif

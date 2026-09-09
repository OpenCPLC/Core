// hal/stm32/sys/pwr.h

#ifndef PWR_H_
#define PWR_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"

#if defined(STM32G0)
  #include "stm32g0xx.h"
  #include "pwr_g0.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
  #include "pwr_wb.h"
#endif

//------------------------------------------------------------------------------- RCC: Clock enable

void RCC_EnableTIM(void *tim);
void RCC_EnableGPIO(void *gpio);
void RCC_EnableUART(void *uart);
void RCC_DisableUART(void *uart);
void RCC_EnableI2C(void *i2c);
void RCC_DisableI2C(void *i2c);
void RCC_EnableSPI(void *spi);
void RCC_EnableDMA(void *dma);
void RCC_EnableCRC(void);
void RCC_EnableRNG(void);

// USB controller clock: `HSI48` trimmed by `CRS` on bus SOF, transceiver supply valid
void RCC_EnableUSB(void);

//------------------------------------------------------------------------------- RCC: System clock

// Every setter returns the resulting `SystemCoreClock` [Hz]
uint32_t RCC_GetClock(void);
uint32_t RCC_SetHSE(uint32_t xtal_Hz);

/**
 * @brief Run from the PLL: `source / m × n / r`, `hse_Hz` of `0` picks HSI16.
 * @param[in] hse_Hz Crystal frequency, `0` = HSI16 as the PLL source
 * @param[in] m Input divider `1..8`
 * @param[in] n Multiplier `8..86`
 * @param[in] r Output divider `2..8`
 * @return Resulting system clock [Hz]
 */
uint32_t RCC_SetPLL(uint32_t hse_Hz, uint8_t m, uint8_t n, uint8_t r);

uint32_t RCC_2MHz(void);
uint32_t RCC_16MHz(void);
uint32_t RCC_48MHz(void);
uint32_t RCC_64MHz(void);

//---------------------------------------------------------------------------------------- PWR: Run

typedef enum {
  PWR_SleepMode_Stop0 = 0,
  PWR_SleepMode_Stop1 = 1,
  PWR_SleepMode_Stop2 = 2, // WB only, G0 maps it to Stop1
  PWR_SleepMode_StandbySRAM = 3,
  PWR_SleepMode_Standby = 4,
  PWR_SleepMode_Shutdown = 5,
  PWR_SleepMode_Error = 6
} PWR_SleepMode_t;

typedef enum {
  PWR_Edge_Rising = 0,
  PWR_Edge_Falling = 1
} PWR_Edge_t;

// Wakeup pin bits for `PWR_Shutdown`, numbering after the reference manual
typedef enum {
  PWR_Wakeup_Pin1 = (1 << 0),
  PWR_Wakeup_Pin2 = (1 << 1),
  PWR_Wakeup_Pin3 = (1 << 2),
  PWR_Wakeup_Pin4 = (1 << 3),
  PWR_Wakeup_Pin5 = (1 << 4)
} PWR_Wakeup_t;

// System reset
void PWR_Reset(void);

/**
 * @brief Enter a low-power mode, returns on wakeup.
 * @param[in] mode Mode, `PWR_SleepMode_Error` is refused
 */
void PWR_Sleep(PWR_SleepMode_t mode);

/**
 * @brief Arm a wakeup pin for the standby and shutdown modes.
 * @param[in] pin Pin, family enum
 * @param[in] edge Active edge
 */
void PWR_SetWakeup(PWR_WakeupPin_t pin, PWR_Edge_t edge);

/**
 * @brief Enter shutdown: the lowest power state, RAM lost, exit is a reset.
 *   LSI dies with it, so an armed watchdog stays silent.
 *   On STM32WB this is only legal before CPU2 is booted. Once the wireless core has
 *   started, reset the system and call again early in boot; CPU2 cannot be forced into
 *   Shutdown by CPU1.
 * @param[in] wakeup_mask Wakeup pins to arm (`PWR_Wakeup_...`)
 * @param[in] falling_mask Bits of `wakeup_mask` that wake on the falling edge
 * @return Nothing on success, the system is off; `ERR` when STM32WB CPU2 is already
 *   booted or the option bytes request a reset on Shutdown entry
 */
status_t PWR_Shutdown(uint8_t wakeup_mask, uint8_t falling_mask);

//--------------------------------------------------------------------------- PWR: Backup registers

// Registers in the domain `VBAT` keeps alive: their content outlives every reset
// and shutdown itself, so a state can wait there for the next boot.
// Five is what every supported family has
typedef enum {
  BKPR_0 = 0, BKPR_1, BKPR_2, BKPR_3, BKPR_4
} BKPR_t;

/**
 * @brief Store a value that outlives resets and shutdown.
 *   The backup domain must be unlocked, which `RTC_Init` does.
 * @param[in] reg Register to write
 * @param[in] value Value to keep
 */
void BKPR_Write(BKPR_t reg, uint32_t value);

/**
 * @brief Read a value left in the backup domain.
 * @param[in] reg Register to read
 * @return Stored value, zero once a power-on has reset the domain
 */
uint32_t BKPR_Read(BKPR_t reg);

// Reset the backup domain (`RCC_BDCR`, RTC, backup registers) after a power-on reset,
// the one event that can corrupt it; a no-op after every other reset
void BKP_DomainReset(void);

//---------------------------------------------------------------------------------- IWDG: Watchdog

// Watchdog tick per count: LSI 32kHz through the prescaler, 4096 counts top out at 32.7s
typedef enum {
  IWDG_Time_125us = 0,
  IWDG_Time_250us = 1,
  IWDG_Time_500us = 2,
  IWDG_Time_1ms = 3,
  IWDG_Time_2ms = 4,
  IWDG_Time_4ms = 5,
  IWDG_Time_8ms = 6
} IWDG_Time_t;

/**
 * @brief Arm the independent watchdog; from then on only a reset stops it.
 *   The countdown pauses while a debugger halts the core.
 * @param[in] prescaler Tick per count (`IWDG_Time_...`)
 * @param[in] reload Counts until reset, up to `4095`
 */
void IWDG_Init(IWDG_Time_t prescaler, uint16_t reload);

/**
 * @brief Arm the watchdog by timeout: the smallest tick that covers it.
 * @param[in] timeout_ms Time without a refresh that resets [ms], up to `32760`
 */
void IWDG_Init_ms(uint32_t timeout_ms);

// Reload the countdown, call more often than the timeout
void IWDG_Refresh(void);

// `true` when the last reset came from the watchdog; reset flags latch on first read
bool IWDG_WasReset(void);

//---------------------------------------------------------------------------- BOR: Brown-out reset

// Brown-out reset threshold, stored in option bytes (`FLASH->OPTR`), non-volatile.
// A higher level resets earlier on a supply droop, before the core enters an undefined
// state. `BOR_Level_1V7` leaves only the always-on power-down detector. Voltages approximate
typedef enum {
  BOR_Level_1V7 = 0, // ~1.7V power-down only, always on
  BOR_Level_2V0 = 1, // rising ~2.0V
  BOR_Level_2V2 = 2, // rising ~2.2V
  BOR_Level_2V5 = 3, // rising ~2.5V
  BOR_Level_2V8 = 4  // rising ~2.8V
} BOR_Level_t;

// Threshold programmed in the option bytes
BOR_Level_t BOR_GetLevel(void);

/**
 * @brief Program the brownout threshold option byte when it differs from `level`.
 *   The write relaunches the option bytes, which resets the system: the call never
 *   returns in that case and the next boot reads the new level back. Call early,
 *   on STM32WB before the CPU2 boots, so no flash arbitration is in play.
 * @param[in] level Requested threshold
 * @return `OK` when the level matches already, `ERR` when flash refuses the write
 */
status_t BOR_SetLevel(BOR_Level_t level);

// `true` when the last reset came from a brown-out; reset flags latch on first read
bool BOR_WasReset(void);

//-------------------------------------------------------------------------------------------------
#endif

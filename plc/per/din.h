// plc/per/din.h

#ifndef DIN_H_
#define DIN_H_

#include <stdint.h>
#include <stdbool.h>
#include "eeprom.h"
#include "gpio.h"
#include "gpif.h"
#include "pwmi.h"
#include "vrts.h"

//-------------------------------------------------------------------------------------------------

/** Special value passed to `DIN_Time` to restore the default */
#define DIN_DEFAULT_TIME 0xFFFFFFFF

#ifndef DIN_DEFAULT_HOLD_ms
  /** Covers zero-crossing gaps of rectified 50/60Hz mains (~10ms full, ~20ms half-wave) */
  #define DIN_DEFAULT_HOLD_ms 30
#endif

// Digital input operating mode
typedef enum {
  DIN_Mode_DC = 0,     // Standard debounced input
  DIN_Mode_AC,         // Rectified mains input: pulse-stretch before debounce
  DIN_Mode_FastCounter // PWMI capture, GPIF skipped
} DIN_Mode_t;

// Digital input time parameter
typedef enum {
  DIN_Time_Ton = 0,  // Debounce time for ON [ms]
  DIN_Time_Toff,     // Debounce time for OFF [ms]
  DIN_Time_TonLong,  // Long press threshold [ms]
  DIN_Time_ToffLong, // Long release threshold [ms]
  DIN_Time_Toggle,   // Double click window [ms]
  DIN_Time_Hold      // AC pulse-stretch hold [ms]
} DIN_Time_t;

/**
 * @brief Digital input configuration.
 * @param[in] name Name used in bash queries
 * @param[in] gpio Underlying GPIO configuration (`port`, `pin`, `reverse`)
 * @param[in] gpif Input filter instance `GPIF_t` (debounce, edge and toggle times)
 * @param[in] mode Operating mode.
 * `DIN_Mode_AC` stretches the raw high state by `hold_ms`,
 * so zero-crossing dropouts are ignored by the debounce filter,
 * with effective OFF latency `hold_ms + toff_ms` after the last pulse.
 * `DIN_Mode_FastCounter` uses `pwmi` + `channel` instead of GPIF
 * @param[in] hold_ms AC pulse-stretch hold [ms], must exceed the zero-crossing gap
 * @param[in] eeprom Pointer to `EEPROM_t` for non-volatile storage, `NULL` = no persistence
 * @param[in] pwmi Pointer to `PWMI_t` controller for fast counter.
 * One `PWMI_t` (one timer) is shared by up to 4 inputs,
 * the board assigns each DI one capture channel
 * and calls `PWMI_Init`/`PWMI_Loop` once per timer, after all assignments
 * @param[in] channel Channel of `PWMI_t` controller
 * Internal:
 * @param _tick_hold AC pulse-stretch timer
 */
typedef struct {
  const char *name;
  GPIO_t gpio;
  GPIF_t gpif;
  DIN_Mode_t mode;
  uint32_t hold_ms;
  EEPROM_t *eeprom;
  PWMI_t *pwmi;
  TIM_Channel_t channel;
  uint64_t _tick_hold;
} DIN_t;

/**
 * @brief Initialize digital input (DI).
 * If EEPROM is available, parameters are loaded automatically.
 * In fast counter mode the pin mux belongs to the `PWMI_t` controller
 * initialized by the caller, so only the filter state is prepared here.
 * @note A single sample cannot tell whether a pulsing AC input is live,
 * so `DIN_Mode_AC` always starts low and reports a rising edge once the filter catches up.
 * @param[in,out] din Pointer to digital input (DI)
 */
void DIN_Init(DIN_t *din);

/**
 * @brief Update digital input (DI) state.
 * Call every main loop or thread pass: this is the sampler,
 * so the call period bounds both the debounce resolution
 * and, in `DIN_Mode_AC`, the ability to catch mains pulses between zero crossings.
 * Extra calls are cheap, the filter itself advances once per system tick.
 * No-op in fast counter mode (`PWMI_Loop` handles measurement).
 * @param[in,out] din Pointer to digital input (DI)
 */
void DIN_Loop(DIN_t *din);

/**
 * @brief Get filtered digital input (DI) state.
 * @param[in] din Pointer to digital input (DI)
 * @return `true` if high, `false` if low
 */
bool DIN_State(DIN_t *din);

/**
 * @brief Get raw digital input (DI) state (without filter).
 * In AC mode this shows raw pulses, not the stretched signal.
 * @param[in] din Pointer to digital input (DI)
 * @return `true` if high, `false` if low
 */
bool DIN_Raw(DIN_t *din);

/**
 * @brief Get toggle watchdog state, flipping while the input keeps switching faster
 * than `toggle_ms` (see `GPIF_Toggle`). A settled input never flips it.
 * @param[in] din Pointer to digital input (DI)
 * @return Current toggle state
 */
bool DIN_Toggle(DIN_t *din);

/**
 * @brief Check DI rising edge and clear flag.
 * @param[in,out] din Pointer to digital input (DI)
 * @return `true` if rising edge occurred
 */
bool DIN_Rise(DIN_t *din);

/**
 * @brief Check DI falling edge and clear flag.
 * @param[in,out] din Pointer to digital input (DI)
 * @return `true` if falling edge occurred
 */
bool DIN_Fall(DIN_t *din);

/**
 * @brief Check any DI edge and clear flag.
 * @param[in,out] din Pointer to digital input (DI)
 * @return `true` if rising or falling edge occurred
 */
bool DIN_Edge(DIN_t *din);

/**
 * @brief Check DI long press and clear flag.
 * @param[in,out] din Pointer to digital input (DI)
 * @return `true` if input held high for `ton_long_ms`
 */
bool DIN_RiseLong(DIN_t *din);

/**
 * @brief Check DI long release and clear flag.
 * @param[in,out] din Pointer to digital input (DI)
 * @return `true` if input held low for `toff_long_ms`
 */
bool DIN_FallLong(DIN_t *din);

/**
 * @brief Check any DI long edge and clear flag.
 * @param[in,out] din Pointer to digital input (DI)
 * @return `true` if long press or long release occurred
 */
bool DIN_EdgeLong(DIN_t *din);

/**
 * @brief Read or set a digital input time parameter, saved to EEPROM if available.
 * @param[in,out] din Pointer to digital input (DI)
 * @param[in] time Which parameter to access
 * @param[in] ms New value [ms], `0` only reads, `DIN_DEFAULT_TIME` restores the default
 * @return Current value [ms]
 */
uint32_t DIN_Time(DIN_t *din, DIN_Time_t time, uint32_t ms);

/**
 * @brief Get duty cycle from fast counter, corrected for `gpio.reverse`.
 * @param[in] din Pointer to digital input (DI)
 * @return Duty cycle [%] or `NaN` when fast counter inactive or no signal
 */
float DIN_Duty_Percent(DIN_t *din);

/**
 * @brief Get frequency from fast counter.
 * @param[in] din Pointer to digital input (DI)
 * @return Frequency [Hz] or `NaN` when fast counter inactive or no signal
 */
float DIN_Frequency_Hz(DIN_t *din);

//-------------------------------------------------------------------------------------------------
#endif

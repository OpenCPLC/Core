// lib/sys/gpif.h

#ifndef GPIF_H_
#define GPIF_H_

#include <stdint.h>
#include <stdbool.h>
#include "vrts.h"

//----------------------------------------------------------------------------------- GPIF Defaults

#ifndef GPIF_DEFAULT_TON_ms
  #define GPIF_DEFAULT_TON_ms 50
#endif
#ifndef GPIF_DEFAULT_TOFF_ms
  #define GPIF_DEFAULT_TOFF_ms 50
#endif
#ifndef GPIF_DEFAULT_TON_LONG_ms
  #define GPIF_DEFAULT_TON_LONG_ms 2000
#endif
#ifndef GPIF_DEFAULT_TOFF_LONG_ms
  #define GPIF_DEFAULT_TOFF_LONG_ms 2000
#endif
#ifndef GPIF_DEFAULT_TOGGLE_ms
  #define GPIF_DEFAULT_TOGGLE_ms 400
#endif

//-------------------------------------------------------------------------------------- GPIF Types

/**
 * @brief Filtered binary input with debounce, edge detection and toggle. Source
 * agnostic: the caller samples the signal (GPIO, expander, register bit) and feeds
 * every sample to `GPIF_Loop`.
 * @param[in] ton_ms Debounce time for ON [ms]
 * @param[in] toff_ms Debounce time for OFF [ms]
 * @param[in] ton_long_ms Long press threshold [ms]
 * @param[in] toff_long_ms Long release threshold [ms]
 * @param[in] toggle_ms Toggle watchdog window for a continuously switching input [ms]
 * Internal:
 * @param _rise Rising edge flag
 * @param _fall Falling edge flag
 * @param _rise_long Long press flag
 * @param _fall_long Long release flag
 * @param _input Debounced input state
 * @param _toggle Toggle output state
 * @param _tick Low word of the tick of the last evaluated sample
 * @param _tick_debounce Debounce timer
 * @param _tick_long Long threshold timer
 * @param _tick_toggle Toggle watchdog timer
 * @param _tick_reset Watchdog disarm timer
 */
typedef struct {
  uint32_t ton_ms;
  uint32_t toff_ms;
  uint32_t ton_long_ms;
  uint32_t toff_long_ms;
  uint32_t toggle_ms;
  // internal
  uint32_t _tick;
  bool _rise;
  bool _fall;
  bool _rise_long;
  bool _fall_long;
  bool _input;
  bool _toggle;
  uint64_t _tick_debounce;
  uint64_t _tick_long;
  uint64_t _tick_toggle;
  uint64_t _tick_reset;
} GPIF_t;

//---------------------------------------------------------------------------------------- GPIF API

/**
 * @brief Initialize GPIF: applies defaults and seeds the filter state.
 * @param[in,out] gpif Pointer to GPIF structure
 * @param[in] input Initial input state
 */
void GPIF_Init(GPIF_t *gpif, bool input);

/**
 * @brief Update GPIF state with a fresh input sample (call periodically).
 * Timing resolution is one system tick,
 * so repeated calls within the same tick return at once,
 * and the filter is safe to drive straight from a fast main loop.
 * @param[in,out] gpif Pointer to GPIF structure
 * @param[in] input Raw input sample
 */
void GPIF_Loop(GPIF_t *gpif, bool input);

/**
 * @brief Get debounced input state.
 * @param[in] gpif Pointer to GPIF structure
 * @return Current input state
 */
bool GPIF_Input(GPIF_t *gpif);

/**
 * @brief Get toggle watchdog state. Flips while the input keeps switching:
 * transitions arriving closer together than `toggle_ms / 2` keep the watchdog armed,
 * and it fires `toggle_ms` after the burst began.
 * An input that settles disarms it before that,
 * so a single press or a steady level never flips the state.
 * @param[in] gpif Pointer to GPIF structure
 * @return Current toggle state
 */
bool GPIF_Toggle(GPIF_t *gpif);

/**
 * @brief Check and clear rising edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if rising edge occurred
 */
bool GPIF_Rise(GPIF_t *gpif);

/**
 * @brief Check and clear falling edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if falling edge occurred
 */
bool GPIF_Fall(GPIF_t *gpif);

/**
 * @brief Check and clear any edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if any edge occurred
 */
bool GPIF_Edge(GPIF_t *gpif);

/**
 * @brief Check and clear long press flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if long press occurred
 */
bool GPIF_RiseLong(GPIF_t *gpif);

/**
 * @brief Check and clear long release flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if long release occurred
 */
bool GPIF_FallLong(GPIF_t *gpif);

/**
 * @brief Check and clear any long edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if any long edge occurred
 */
bool GPIF_EdgeLong(GPIF_t *gpif);

//-------------------------------------------------------------------------------------------------

#endif

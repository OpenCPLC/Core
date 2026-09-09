// plc/per/dout.h

#ifndef DOUT_H_
#define DOUT_H_

#include <stdbool.h>
#include <stdint.h>
#include "eeprom.h"
#include "pwm.h"
#include "main.h"
#include "xstring.h"

//------------------------------------------------------------------------------------------ Config

#ifndef DOUT_RELAY_STUN_ms
  // Shortest state a relay may hold [ms], guards the contacts against chatter
  #define DOUT_RELAY_STUN_ms 200
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Digital output: a relay (RO), a transistor (TO) or a triac (XO).
 *   A relay drives its own pin, the others share a `PWM_t` timer and take one channel of it.
 * @param[in] relay Relay output, switching cycles are counted
 * @param[in] name Name shown in the shell
 * @param[in] gpio Pin of a relay output, `port` and `pin` set by the board
 * @param[in] pwm PWM timer of a transistor or triac output
 * @param[in] channel Channel of `pwm` this output takes
 * @param[in] eeprom Store of the retained state, `NULL` = nothing retained
 * @param[in,out] save Retain `value` across resets, kept in `eeprom` itself
 * @param[out] value Level of a relay, compare value of a PWM channel; set through the API
 * @param[out] cycles Switching cycles of a relay, retained in `eeprom`
 * Internal:
 * @param _stun Deadline until which the output holds its state
 * @param _ton_ms On time of the pulse sequence [ms]
 * @param _toff_ms Off time of the pulse sequence [ms]
 * @param _last_ms Off time after the last pulse [ms]
 * @param _pulse Half-periods left in the pulse sequence
 */
typedef struct {
  bool relay;
  const char *name;
  GPIO_t gpio;
  PWM_t *pwm;
  TIM_Channel_t channel;
  EEPROM_t *eeprom;
  uint32_t save;
  uint32_t value;
  uint32_t cycles;
  // internal
  uint64_t _stun;
  uint16_t _ton_ms;
  uint16_t _toff_ms;
  uint16_t _last_ms;
  uint8_t _pulse;
} DOUT_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Restore the retained state and configure the pin of a relay.
 *   The `PWM_t` of a transistor or triac output is initialized by the board.
 * @param[in,out] dout Digital output
 */
void DOUT_Init(DOUT_t *dout);

/**
 * @brief Apply the requested level and run the pulse sequence, every main loop pass.
 * @param[in,out] dout Digital output
 */
void DOUT_Loop(DOUT_t *dout);

/**
 * @brief PWM frequency of the timer behind the output.
 * @param[in] dout Digital output
 * @return Frequency [Hz], `0` for a relay
 */
float DOUT_GetFrequency(const DOUT_t *dout);

/**
 * @brief Retune the timer behind the output, every output sharing it follows.
 * @param[in,out] dout Digital output
 * @param[in] frequency Target frequency [Hz]
 * @return Frequency applied [Hz], `0` for a relay
 */
float DOUT_Frequency(DOUT_t *dout, float frequency);

/**
 * @brief Duty of the PWM channel behind the output.
 * @param[in] dout Digital output
 * @return Duty [%], `NaN` for a relay
 */
float DOUT_GetDuty(const DOUT_t *dout);

/**
 * @brief Set the duty of the PWM channel, retained when `save` is on.
 * @param[in,out] dout Digital output
 * @param[in] duty Duty [%], `0..100`
 * @return Duty applied [%], `NaN` for a relay
 */
float DOUT_Duty(DOUT_t *dout, float duty);

// Level requested of the output, applied by `DOUT_Loop`; `Preset` picks `Set` or `Rst`
void DOUT_Set(DOUT_t *dout);
void DOUT_Rst(DOUT_t *dout);
void DOUT_Tgl(DOUT_t *dout);
void DOUT_Preset(DOUT_t *dout, bool value);

/**
 * @brief Start a pulse sequence, the output returns to `value` after it.
 *   A relay refuses times below `DOUT_RELAY_STUN_ms`.
 * @param[in,out] dout Digital output
 * @param[in] count Pulses, at most `127`
 * @param[in] ton_ms On time [ms]
 * @param[in] toff_ms Off time [ms]
 * @param[in] freeze_ms Extra off time after the last pulse [ms]
 * @return `true` when started, `false` while a sequence runs or the arguments are refused
 */
bool DOUT_Pulse(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms);
bool DOUT_PulseFreeze(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms,
  uint16_t freeze_ms);

// Level at the pin now, and whether a pulse sequence runs
bool DOUT_State(const DOUT_t *dout);
bool DOUT_IsPulse(const DOUT_t *dout);

/**
 * @brief Retain `value` across resets, the choice itself is retained too.
 * @param[in,out] dout Digital output
 * @param[in] save Retain
 */
void DOUT_SaveValue(DOUT_t *dout, bool save);

//-------------------------------------------------------------------------------------------------
#endif

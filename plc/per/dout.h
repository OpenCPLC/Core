#ifndef DOUT_H_
#define DOUT_H_

#include <stdint.h>
#include <stdbool.h>
#include "eeprom.h"
#include "pwm.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef DOUT_RELAY_STUN_ms
  /** Safety delay [ms] to prevent relay over-switching */
  #define DOUT_RELAY_STUN_ms 200
#endif

#ifndef DOUT_BASH_LIMIT
  /** Enable bash for DOUT, max outputs */
  #define DOUT_BASH_LIMIT 0
#endif

/**
 * @brief Digital output descriptor
 * @param[in] relay Specifies if the output is relay-type (RO)
 * @param[in] name Display name used in `bash` queries
 * @param[in] gpio Reference to `GPIO_t`; fields `port` and `pin` must be configured
 * @param[in] pwm Pointer to `PWM_t` controller
 * @param[in] channel Channel of the `PWM_t` controller assigned to this output
 * @param[in] eeprom Pointer to `EEPROM_t` for non-volatile storage of the output value
 * @param[in] save Indicates whether the output state should be retained after reset
 * @param[out] value Output duty cycle [%]; should be set via `DOUT_Set()`
 * @param[out] cycles Total number of relay switching cycles
 * @param stun Internal timestamp guard that temporarily freezes output activity
 * @param ton_ms Internal storage of TON (on-time) used by `DOUT_Pulse`
 * @param toff_ms Internal storage of TOFF (off-time) used by `DOUT_Pulse`
 * @param pulse Internal counter of remaining pulses in a pulse sequence
 */
typedef struct {
  bool relay;
  char *name;
  GPIO_t gpio;
  PWM_t *pwm;
  TIM_Channel_t channel;
  EEPROM_t *eeprom;
  uint32_t save;
  uint32_t value;
  uint32_t cycles;
  uint64_t stun;
  uint16_t ton_ms;
  uint16_t toff_ms;
  uint16_t last_ms;
  uint8_t pulse;
} DOUT_t;

void DOUT_Init(DOUT_t *dout);
void DOUT_Loop(DOUT_t *dout);

float PWM_GetFrequency(const PWM_t *pwm);
float PWM_Frequency(PWM_t *pwm, float frequency);
float DOUT_GetFrequency(const DOUT_t *dout);
float DOUT_Frequency(DOUT_t *dout, float frequency);
float DOUT_GetDuty(const DOUT_t *dout);
float DOUT_Duty(DOUT_t *dout, float duty);
void DOUT_Set(DOUT_t *dout);
void DOUT_Rst(DOUT_t *dout);
void DOUT_Tgl(DOUT_t *dout);
void DOUT_Preset(DOUT_t *dout, bool value);
bool DOUT_Pulse(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms);
bool DOUT_PulseFreeze(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms, uint16_t freeze_ms);
bool DOUT_State(const DOUT_t *dout);
bool DOUT_IsPulse(DOUT_t *dout);
void DOUT_SaveValue(DOUT_t *dout, bool save);

#if(DOUT_BASH_LIMIT)
  void DOUT_Bash_Add(DOUT_t *dout);
  void DOUT_Bash(char **argv, uint16_t argc);
#endif

//-------------------------------------------------------------------------------------------------
#endif
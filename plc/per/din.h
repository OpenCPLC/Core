#ifndef DIN_H_
#define DIN_H_

#include <stdint.h>
#include <stdbool.h>
#include "eeprom.h"
#include "gpio.h"
#include "pwmi.h"
#include "vrts.h"

//-------------------------------------------------------------------------------------------------

/** Special value to restore default DI time in setters */
#define DIN_DEFAULT_TIME 0xFFFFFFFF

/**
 * @brief Digital input configuration.
 * @param[in] name Name used in bash queries
 * @param[in] gpif Input filter instance `GPIF_t` with `gpio.port` and `gpio.pin`
 * @param[in] eeprom Pointer to `EEPROM_t` for non-volatile storage
 * @param[in] fast_counter Enable fast counter mode
 * @param[in] pwmi Pointer to `PWMI_t` controller for fast counter
 * @param[in] channel Channel of `PWMI_t` controller
 */
typedef struct {
  const char *name;
  GPIF_t gpif;
  EEPROM_t *eeprom;
  bool fast_counter;
  PWMI_t *pwmi;
  TIM_Channel_t channel;
} DIN_t;

bool DIN_Init(DIN_t *din);
bool DIN_Loop(DIN_t *din);

bool DIN_State(DIN_t *din);
bool DIN_Raw(DIN_t *din);
bool DIN_Toggle(DIN_t *din);
bool DIN_Rise(DIN_t *din);
bool DIN_Fall(DIN_t *din);
bool DIN_Edge(DIN_t *din);
bool DIN_RiseLong(DIN_t *din);
bool DIN_FallLong(DIN_t *din);
bool DIN_EdgeLong(DIN_t *din);

uint32_t DIN_SetTon(DIN_t *din, uint32_t ton_ms);
uint32_t DIN_SetToff(DIN_t *din, uint32_t toff_ms);
uint32_t DIN_SetTonLong(DIN_t *din, uint32_t ton_long_ms);
uint32_t DIN_SetToffLong(DIN_t *din, uint32_t toff_long_ms);
uint32_t DIN_SetToggle(DIN_t *din, uint32_t toggle_ms);

float DIN_Duty_Percent(DIN_t *din);
float DIN_Frequency_Hz(DIN_t *din);

//-------------------------------------------------------------------------------------------------
#endif

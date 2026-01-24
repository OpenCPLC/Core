#include "din.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize digital input (DI).
 * If EEPROM is available, parameters are loaded automatically.
 * @param din Pointer to digital input (DI)
 * @return `true` if standard DI initialized, `false` if fast counter mode
 */
bool DIN_Init(DIN_t *din)
{
  if(din->eeprom) {
    if(!din->gpif.ton_ms) EEPROM_Load(din->eeprom, &din->gpif.ton_ms);
    if(!din->gpif.toff_ms) EEPROM_Load(din->eeprom, &din->gpif.toff_ms);
    if(!din->gpif.ton_long_ms) EEPROM_Load(din->eeprom, &din->gpif.ton_long_ms);
    if(!din->gpif.toff_long_ms) EEPROM_Load(din->eeprom, &din->gpif.toff_long_ms);
    if(!din->gpif.toggle_ms) EEPROM_Load(din->eeprom, &din->gpif.toggle_ms);
  }
  if(din->fast_counter) return false; // Fast counter mode active
  GPIF_Init(&din->gpif);
  return true; // Normal GPIF mode
}

/**
 * @brief Update digital input DI state.
 * Call in main loop or thread, recommended at least every 100 ms.
 * @param din Pointer to digital input DI
 * @return True if GPIF filter was updated, false if fast counter mode
 */
bool DIN_Loop(DIN_t *din)
{
  if(din->fast_counter) return false;
  GPIF_Loop(&din->gpif);
  return true;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Get filtered digital input (DI) state.
 * @param din Pointer to digital input (DI)
 * @return True if high, false if low
 */
bool DIN_State(DIN_t *din)
{
  return GPIF_Input(&din->gpif);
}

/**
 * @brief Get raw digital input (DI) state (without filter).
 * @param din Pointer to digital input (DI)
 * @return `true` if high, `false` if low
 */
bool DIN_Raw(DIN_t *din)
{
  return GPIO_In(&din->gpif.gpio);
}

/**
 * @brief Check if input is toggling.
 * @param din Pointer to digital input (DI)
 * @return `true` if input keeps changing, `false` otherwise
 */
bool DIN_Toggle(DIN_t *din)
{
  return GPIF_Toggle(&din->gpif);
}

/**
 * @brief Check DI rising edge and clear flag.
 * @param din Pointer to digital input (DI)
 * @return `true` if rising edge occurred, `false` otherwise
 */
bool DIN_Rise(DIN_t *din)
{
  return GPIF_Rise(&din->gpif);
}

/**
 * @brief Check DI falling edge and clear flag.
 * @param din Pointer to digital input (DI)
 * @return `true` if falling edge occurred, `false` otherwise
 */
bool DIN_Fall(DIN_t *din)
{
  return GPIF_Fall(&din->gpif);
}

/**
 * @brief Check any DI edge and clear flag.
 * @param din Pointer to digital input (DI)
 * @return `true` if rising or falling edge occurred, `false` otherwise
 */
bool DIN_Edge(DIN_t *din)
{
  return GPIF_Edge(&din->gpif);
}

/**
 * @brief Check DI long rising edge and clear flag.
 * @param din Pointer to digital input (DI)
 * @return `true` if long rising edge occurred, `false` otherwise
 */
bool DIN_RiseLong(DIN_t *din)
{
  return GPIF_RiseLong(&din->gpif);
}

/**
 * @brief Check DI long falling edge and clear flag.
 * @param din Pointer to digital input (DI)
 * @return `true` if long falling edge occurred, `false` otherwise
 */
bool DIN_FallLong(DIN_t *din)
{
  return GPIF_FallLong(&din->gpif);
}

/**
 * @brief Check any DI long edge and clear flag.
 * @param din Pointer to digital input (DI)
 * @return `true` if long rising or falling edge occurred, `false` otherwise
 */
bool DIN_EdgeLong(DIN_t *din)
{
  return GPIF_EdgeLong(&din->gpif);
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Set ton time for digital input (DI).
 * Applies new ton in ms or default if `DIN_DEFAULT_TIME`.
 * Saves to EEPROM if available.
 * @param din Pointer to digital input DI
 * @param ton_ms New ton[ms] or `DIN_DEFAULT_TIME`
 * @return Current ton[ms]
 */
uint32_t DIN_SetTon(DIN_t *din, uint32_t ton_ms)
{
  uint32_t ton_backup = din->gpif.ton_ms;
  if(ton_ms && ton_ms != din->gpif.ton_ms) {
    if(ton_ms == DIN_DEFAULT_TIME) din->gpif.ton_ms = GPIF_DEFAULT_TON_ms;
    else din->gpif.ton_ms = ton_ms;
    if(din->eeprom) {
      if(EEPROM_Save(din->eeprom, &din->gpif.ton_ms)) din->gpif.ton_ms = ton_backup;
    }
  }
  return din->gpif.ton_ms;
}

/**
 * @brief Set toff time for digital input (DI).
 * Applies new toff in ms or default if `DIN_DEFAULT_TIME`.
 * Saves to EEPROM if available.
 * @param din Pointer to digital input DI
 * @param toff_ms New toff[ms] or `DIN_DEFAULT_TIME`
 * @return Current toff[ms]
 */
uint32_t DIN_SetToff(DIN_t *din, uint32_t toff_ms)
{
  uint32_t toff_backup = din->gpif.toff_ms;
  if(toff_ms && toff_ms != din->gpif.toff_ms) {
    if(toff_ms == DIN_DEFAULT_TIME) din->gpif.toff_ms = GPIF_DEFAULT_TOFF_ms;
    else din->gpif.toff_ms = toff_ms;
    if(din->eeprom) {
      if(EEPROM_Save(din->eeprom, &din->gpif.toff_ms)) din->gpif.toff_ms = toff_backup;
    }
  }
  return din->gpif.toff_ms;
}

/**
 * @brief Set ton_long time for digital input (DI).
 * Applies new ton_long in ms or default if `DIN_DEFAULT_TIME`.
 * Saves to EEPROM if available.
 * @param din Pointer to digital input DI
 * @param ton_long_ms New ton_long[ms] or `DIN_DEFAULT_TIME`
 * @return Current ton_long[ms]
 */
uint32_t DIN_SetTonLong(DIN_t *din, uint32_t ton_long_ms)
{
  uint32_t backup = din->gpif.ton_long_ms;
  if(ton_long_ms && ton_long_ms != din->gpif.ton_long_ms) {
    if(ton_long_ms == DIN_DEFAULT_TIME) din->gpif.ton_long_ms = GPIF_DEFAULT_TON_LONG_ms;
    else din->gpif.ton_long_ms = ton_long_ms;
    if(din->eeprom) {
      if(EEPROM_Save(din->eeprom, &din->gpif.ton_long_ms)) din->gpif.ton_long_ms = backup;
    }
  }
  return din->gpif.ton_long_ms;
}

/**
 * @brief Set toff_long time for digital input (DI).
 * Applies new toff_long in ms or default if `DIN_DEFAULT_TIME`.
 * Saves to EEPROM if available.
 * @param din Pointer to digital input DI
 * @param toff_long_ms New toff_long[ms] or `DIN_DEFAULT_TIME`
 * @return Current toff_long[ms]
 */
uint32_t DIN_SetToffLong(DIN_t *din, uint32_t toff_long_ms)
{
  uint32_t backup = din->gpif.toff_long_ms;
  if(toff_long_ms && toff_long_ms != din->gpif.toff_long_ms) {
    if(toff_long_ms == DIN_DEFAULT_TIME) din->gpif.toff_long_ms = GPIF_DEFAULT_TOFF_LONG_ms;
    else din->gpif.toff_long_ms = toff_long_ms;
    if(din->eeprom) {
      if(EEPROM_Save(din->eeprom, &din->gpif.toff_long_ms)) din->gpif.toff_long_ms = backup;
    }
  }
  return din->gpif.toff_long_ms;
}

/**
 * @brief Set toggle time for digital input (DI).
 * Applies new toggle period in ms or default if `DIN_DEFAULT_TIME`.
 * Saves to EEPROM if available.
 * @param din Pointer to digital input DI
 * @param toggle_ms New toggle[ms] or `DIN_DEFAULT_TIME`
 * @return Current toggle[ms]
 */
uint32_t DIN_SetToggle(DIN_t *din, uint32_t toggle_ms)
{
  uint32_t backup = din->gpif.toggle_ms;
  if(toggle_ms && toggle_ms != din->gpif.toggle_ms) {
    if(toggle_ms == DIN_DEFAULT_TIME) din->gpif.toggle_ms = GPIF_DEFAULT_TOGGLE_ms;
    else din->gpif.toggle_ms = toggle_ms;
    if(din->eeprom) {
      if(EEPROM_Save(din->eeprom, &din->gpif.toggle_ms)) din->gpif.toggle_ms = backup;
    }
  }
  return din->gpif.toggle_ms;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Get duty cycle from fast counter for digital input (DI).
 * Returns duty in percent. If fast counter is not active
 * or no signal is present, returns `NaN`.
 * @param din Pointer to DI
 * @return Duty cycle [%] or `NaN`
 */
float DIN_Duty_Percent(DIN_t *din)
{
  float duty = din->pwmi->duty[din->channel];
  if(!din->fast_counter || isNaN(duty)) return NaN;
  return din->gpif.gpio.reverse ? (100.0f - duty) : duty;
}

/**
 * @brief Get frequency from fast counter for digital input (DI).
 * Returns frequency in Hz. If fast counter is not active
 * or no signal is present, returns `NaN`.
 * @param din Pointer to DI
 * @return Frequency [Hz] or `NaN`
 */
float DIN_Frequency_Hz(DIN_t *din)
{
  float frequency = din->pwmi->frequency[din->channel];
  if(!din->fast_counter || isNaN(frequency)) return NaN;
  return frequency;
}

//-------------------------------------------------------------------------------------------------

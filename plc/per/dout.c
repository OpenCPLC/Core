#include "dout.h"
#include "vrts.h"
#include "extstr.h"
#include "log.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Get current PWM signal frequency.
 * @param[in] pwm Pointer to `PWM_t` descriptor.
 * @return Current PWM frequency [Hz].
 */
float PWM_GetFrequency(const PWM_t *pwm)
{
  return (float)SystemCoreClock / pwm->prescaler  / pwm->auto_reload / (pwm->center_aligned + 1);
}

/**
 * @brief Set PWM signal frequency.
 * A single `PWM_t` controller may drive multiple digital outputs,
 * so the change affects all linked channels.
 * @param[in,out] pwm Pointer to `PWM_t` descriptor.
 * @param[in] frequency Target frequency [Hz].
 * @return Actual applied frequency [Hz].
 */
float PWM_Frequency(PWM_t *pwm, float frequency)
{
  uint32_t auto_reload_prev = pwm->auto_reload;
  uint32_t prescaler_prev = pwm->prescaler;
  uint32_t prescaler = 1;
  uint32_t auto_reload;
  do {
    PWM_SetPrescaler(pwm, prescaler++);
    auto_reload = (float)SystemCoreClock / pwm->prescaler / frequency / (pwm->center_aligned + 1);
  } while(auto_reload > 0xFFFF);
  PWM_SetAutoreload(pwm, auto_reload);
  for(TIM_Channel_t CH = TIM_CH1; CH <= TIM_CH4; CH++) {
    if(pwm->value[CH]) {
      PWM_SetValue(pwm, CH, (uint32_t)((uint64_t)pwm->value[CH] * pwm->prescaler * pwm->auto_reload / auto_reload_prev / prescaler_prev));
    }
  }
  return PWM_GetFrequency(pwm);
}

/**
 * @brief Get current PWM frequency linked to digital output.
 * @param[in] dout Pointer to `DOUT_t` descriptor for transistor (TO) or triac (XO) output.
 * @return Current PWM frequency [Hz], or 0 for non-PWM outputs (e.g. relay RO).
 */
float DOUT_GetFrequency(const DOUT_t *dout)
{
  if(!dout->pwm) return 0;
  return PWM_GetFrequency(dout->pwm);
}

/**
 * @brief Set PWM frequency for digital output.
 * A single `PWM_t` controller may drive multiple outputs,  
 * so the frequency change affects all linked channels.
 * @param[in,out] dout Pointer to `DOUT_t` descriptor for transistor (TO) or triac (XO) output.
 * @param[in] frequency Target PWM frequency [Hz].
 * @return Current PWM frequency [Hz], or 0 for non-PWM outputs (e.g. relay RO).
 */
float DOUT_Frequency(DOUT_t *dout, float frequency)
{
  if(!dout->pwm) return 0;
  return PWM_Frequency(dout->pwm, frequency);
}

/**
 * @brief Get current PWM duty cycle [%] for digital output.
 * Valid only for outputs with PWM channel assigned.
 * @param[in] dout Pointer to `DOUT_t` output descriptor.
 * @return Duty cycle in percent, or `NaN` if not PWM type.
 */
float DOUT_GetDuty(const DOUT_t *dout)
{
  if(!dout->pwm) return NaN;
  return (float)dout->value * 100.0f / dout->pwm->auto_reload;
}

/**
 * @brief Set PWM duty cycle on digital output.
 * If `save` is enabled, the value is stored in EEPROM.
 * @param[in,out] dout Pointer to `DOUT_t` descriptor for transistor (TO) or triac (XO) output.
 * @param[in] duty PWM duty cycle [%] (0–100).
 * @return Actual applied duty cycle [%], or `NaN` if not PWM type.
 */
float DOUT_Duty(DOUT_t *dout, float duty)
{
  if(!dout->pwm) {
    return NaN;
  }
  duty = clamp(duty, 0.0f, 100.0f);
  uint32_t old_value = dout->pwm->value[dout->channel];
  PWM_SetValue(dout->pwm, dout->channel, duty * dout->pwm->auto_reload / 100.0f);
  dout->value = dout->pwm->value[dout->channel];
  duty = ((float)dout->value * 100.0f) / dout->pwm->auto_reload;
  if(dout->eeprom && (old_value != dout->value) && dout->save) {
    EEPROM_Save(dout->eeprom, &dout->value);
  }
  return duty;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Set digital output to high state.
 * Relay output (RO): closes contact.
 * Transistor output (TO): drives supply or TCOM.
 * Triac output (XO): transfers AC voltage from XCOM.
 * If `save` is enabled, the new state is stored in EEPROM.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 */
void DOUT_Set(DOUT_t *dout)
{
  if(dout->pwm) {
    if(dout->value == dout->pwm->auto_reload) return;
    dout->value = dout->pwm->auto_reload;
    PWM_SetValue(dout->pwm, dout->channel, dout->value);
    if(dout->eeprom && dout->save) EEPROM_Save(dout->eeprom, &dout->value);
  }
  else {
    if(dout->value) return;
    dout->value = true;
    if(dout->eeprom && dout->save) EEPROM_Save(dout->eeprom, &dout->value);
  }
}

/**
 * @brief Set digital output to low state.
 * Relay output (RO): leaves contact open.
 * Transistor output (TO): leaves line floating.
 * Triac output (XO): leaves line floating.
 * If `save` is enabled, the new state is stored in EEPROM.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 */
void DOUT_Rst(DOUT_t *dout)
{
  if(dout->pwm) {
    if(dout->value == 0) return;
    dout->value = 0;
    PWM_SetValue(dout->pwm, dout->channel, dout->value);
    if(dout->eeprom && dout->save) EEPROM_Save(dout->eeprom, &dout->value);
  }
  else {
    if(!dout->value) return;
    dout->value = false;
    if(dout->eeprom && dout->save) EEPROM_Save(dout->eeprom, &dout->value);
    if(dout->relay) dout->stun = tick_keep(DOUT_RELAY_STUN_ms);
  }
}

/**
 * @brief Toggle digital output state.
 * If `save` is enabled, the new state is stored in EEPROM.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 */
void DOUT_Tgl(DOUT_t *dout)
{
  if(dout->pwm) {
    if(dout->value) DOUT_Rst(dout);
    else DOUT_Set(dout);
  }
  else {
    dout->value = !dout->value;
    if(dout->eeprom && dout->save) EEPROM_Save(dout->eeprom, &dout->value);
  }
}

/**
 * @brief Set digital output to high `true` or low `false` state.
 * @param[in] dout Pointer to `DOUT_t` output descriptor.
 * @param[in] value If `true`, same as `DOUT_Set`; otherwise `DOUT_Rst`.
 */
void DOUT_Preset(DOUT_t *dout, bool value)
{
  if(value) DOUT_Set(dout);
  else DOUT_Rst(dout); 
}

/**
 * @brief Generate one or more pulses on digital output.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 * @param[in] count Number of pulses to generate.
 * @param[in] ton_ms Pulse ON time in milliseconds.
 * @param[in] toff_ms Pulse OFF time in milliseconds.
 * @return `true` if pulse sequence started, otherwise `false`.
 */
bool DOUT_Pulse(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms)
{
  if(dout->pulse) return false;
  if(dout->relay && (ton_ms >= DOUT_RELAY_STUN_ms || toff_ms >= DOUT_RELAY_STUN_ms)) return false;
  dout->pulse = (count * 2u);
  dout->ton_ms = ton_ms;
  dout->toff_ms = toff_ms;
  dout->last_ms = toff_ms;
  return true;
}

/**
 * @brief Generate one or more pulses on digital output with freeze after last pulse.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 * @param[in] count Number of pulses to generate.
 * @param[in] ton_ms Pulse ON time in milliseconds.
 * @param[in] toff_ms Pulse OFF time in milliseconds.
 * @param[in] freeze_ms Extra OFF time added after last pulse.
 * @return `true` if pulse sequence started, otherwise `false`.
 */
bool DOUT_PulseFreeze(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms, uint16_t freeze_ms)
{
  if(dout->pulse) return false;
  if(dout->relay && (ton_ms >= DOUT_RELAY_STUN_ms || toff_ms >= DOUT_RELAY_STUN_ms)) return false;
  dout->pulse = (count * 2u);
  dout->ton_ms = ton_ms;
  dout->toff_ms = toff_ms;
  dout->last_ms = toff_ms + freeze_ms;
  return true;
}


/**
 * @brief Get current digital output state.
 * @param[in] dout Pointer to `DOUT_t` output descriptor.
 * @return `true` if output is active (non-zero), otherwise `false`.
 */
bool DOUT_State(const DOUT_t *dout)
{
  if(dout->pwm) return dout->pwm->value[dout->channel] ? true : false;
  else return dout->gpio.set;
}

/**
 * @brief Check if digital output is in pulse sequence.
 * @param[in] dout Pointer to `DOUT_t` output descriptor.
 * @return `true` if pulse is active, otherwise `false`.
 */
bool DOUT_IsPulse(DOUT_t *dout)
{
  return dout->pulse ? true : false;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize digital output.
 * For PWM-backed outputs, the `PWM_t` must be initialized by the caller.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 */
void DOUT_Init(DOUT_t *dout)
{
  if(dout->eeprom) {
    EEPROM_Init(dout->eeprom);
    if(!dout->save) EEPROM_Load(dout->eeprom, &dout->save);
    if(dout->save) EEPROM_Load(dout->eeprom, &dout->value);
    if(dout->relay) EEPROM_Load(dout->eeprom, &dout->cycles);
  }
  if(!dout->pwm) {
    dout->gpio.mode = GPIO_Mode_Output;
    GPIO_Init(&dout->gpio);
  }
}

/**
 * @brief Increment relay switch counter and store in EEPROM.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 */
static inline void DOUT_RelayCyclesInc(DOUT_t *dout)
{
  dout->cycles++;
  if(dout->eeprom) EEPROM_Save(dout->eeprom, &dout->cycles);
}

/**
 * @brief Digital output service loop.
 * Call every main loop; pulse timing uses even/odd `pulse` to select TON/TOFF.
 * @param[in,out] dout Pointer to DOUT_t.
 */
void DOUT_Loop(DOUT_t *dout)
{
  if(tick_away(&dout->stun)) return;
  if(dout->pulse) {
    if(dout->relay && !DOUT_State(dout)) DOUT_RelayCyclesInc(dout);
    if(dout->pwm) {
      uint32_t value = dout->value ? 0 : dout->pwm->auto_reload;
      PWM_SetValue(dout->pwm, dout->channel, value);
    }
    else {
      GPIO_Tgl(&dout->gpio);
    }
    dout->stun = (dout->pulse == 1) ?
      tick_keep(dout->last_ms) :
      tick_keep(dout->pulse % 2 ? dout->toff_ms : dout->ton_ms);
    dout->pulse--;
    return;
  }
  if(DOUT_State(dout) != dout->value) {
    if(dout->pwm) PWM_SetValue(dout->pwm, dout->channel, dout->value);
    else {
      if(dout->value) {
        GPIO_Set(&dout->gpio);
        if(dout->relay) DOUT_RelayCyclesInc(dout);
      }
      else {
        GPIO_Rst(&dout->gpio);
        dout->value = false;
      }
    }
    if(dout->relay) dout->stun = tick_keep(DOUT_RELAY_STUN_ms);
  }
}

/**
 * @brief Define whether digital output value should be retained after reset.
 * @param[in,out] dout Pointer to `DOUT_t` output descriptor.
 * @param[in] save `true` to enable saving, `false` to disable.
 */
void DOUT_SaveValue(DOUT_t *dout, bool save)
{
  if(dout->eeprom && save != dout->save) {
    dout->save = save;
    EEPROM_Save(dout->eeprom, &dout->save);
  }
}

//-------------------------------------------------------------------------------------------------
#if(DOUT_BASH_LIMIT)

#include "bash.h"

void DOUT_Print(DOUT_t *dout)
{
  #if(LOG_COLORS)
    DBG_String(ANSI_TEAL);
  #endif
  DBG_String(dout->name);
  #if(LOG_COLORS)
    DBG_String(ANSI_END);
  #endif
  if(dout->pwm) {
    float duty = DOUT_GetDuty(dout);
    #if(LOG_COLORS)
       DBG_String(ANSI_GREY" duty:"ANSI_END);
    #else
       DBG_String(" duty:");
    #endif
    DBG_Float(duty, 2);
    DBG_Char('%');
    float freq = PWM_GetFrequency(dout->pwm);
    #if(LOG_COLORS)
      DBG_String(ANSI_GREY" freq:"ANSI_END);
    #else
      DBG_String(" freq:");
    #endif
    DBG_Float(freq, 0);
    DBG_String("Hz");
  }
  else {
    if(DOUT_State(dout)) DBG_String(" ON ");
    else DBG_String(" OFF");
    if(DOUT_IsPulse(dout)) DBG_Char('*');
    if(dout->relay) {
      #if(LOG_COLORS)
        DBG_String(ANSI_GREY" cycles:"ANSI_END);
      #else
        DBG_String(" cycles:");
      #endif
      DBG_uDec(dout->cycles);
    }
  }
}

static struct {
  DOUT_t *dout[DOUT_BASH_LIMIT];
  uint16_t count;
  uint32_t hash[DOUT_BASH_LIMIT];
} bash;

void DOUT_Bash_Add(DOUT_t *dout)
{
  if(bash.count >= DOUT_BASH_LIMIT) {
    #if(LOG_COLORS)
      LOG_Error("Exceeded " ANSI_TURQUS "DOUT_t" ANSI_END" limit (max %u)", DOUT_BASH_LIMIT);
    #else
      LOG_Error("Exceeded DOUT_t limit (max %u)", DOUT_BASH_LIMIT);
    #endif
    return;
  }
  if(!dout->name) {
    #if(LOG_COLORS)
      LOG_Error("Object "ANSI_TURQUS"DOUT_t"ANSI_END" passed to BASH must be named", DOUT_BASH_LIMIT);
    #else
      LOG_Error("Object DOUT_t passed to BASH must be named", DOUT_BASH_LIMIT);
    #endif
    return;
  }
  bash.dout[bash.count] = dout;
  bash.hash[bash.count] = hash_djb2_ci(dout->name);
  bash.count++;
}

void DOUT_Bash(char **argv, uint16_t argc)
{
  BASH_Argc(2, 4);
  uint32_t argv1_hash = hash_djb2(argv[1]);
  if(argv1_hash == HASH_List) {
    const char *names[bash.count];
    for(uint16_t i = 0; i < bash.count; i++) {
      names[i] = bash.dout[i]->name;
    }
    #if(LOG_COLORS)
      LOG_Info("Digital outputs: "ANSI_TEAL"%a %s"ANSI_END, bash.count, names);
    #elif
      LOG_Info("Digital outputs: %a, %s", bash.count, names);
    #endif
    return;
  }
  DOUT_t *dout = NULL;
  for(uint16_t i = 0; i < bash.count; i++) {
    if(bash.hash[i] == argv1_hash) {
      dout = bash.dout[i];
      break;
    }
  }
  if(!dout) {
    #if(LOG_COLORS)
      LOG_Warning("Digital output "ANSI_TEAL"%s"ANSI_END" not found", argv[1]);
    #else
      LOG_Warning("Digital output %s not found", argv[1]);
    #endif
    return;
  }
  // bool pulse_todo = false;
  if(argc >= 3) {
    switch(hash_djb2(argv[2])) {
      case HASH_Set: case HASH_On: case HASH_Enable: DOUT_Set(dout); break;
      case HASH_Rst: case HASH_Reset: case HASH_Off: case HASH_Disable: DOUT_Rst(dout); break;
      case HASH_Tgl: case HASH_Toggle: case HASH_Sw: case HASH_Switch: DOUT_Tgl(dout); break;
      // case HASH_Pulse: case HASH_Impulse: case HASH_Burst: pulse_todo = true;
      case HASH_Duty: case HASH_Fill: {
        BASH_Argc(4);
        if(!str_is_u16(argv[3])) {
          LOG_ErrorParse(argv[3], "uint16_t");
          BASH_ArgvExit(3);
        }
        // uint16_t value = str_to_int(argv[3]);
        // if(pulse_todo) DOUT_Pulse(dout, value);
        // else DOUT_Duty(dout, value);
        break;
      }
      default: BASH_ArgvExit(2);
    }
  }
  LOG_Info("Digital output %o", dout, &DOUT_Print);
}

#endif
//-------------------------------------------------------------------------------------------------
// plc/per/dout.c

#include "dout.h"

#include "vrts.h"

//---------------------------------------------------------------------------------------- Internal

static void relay_cycles_inc(DOUT_t *dout)
{
  dout->cycles++;
  if(dout->eeprom) EEPROM_Save(dout->eeprom, &dout->cycles);
}

static void save_value(DOUT_t *dout)
{
  if(dout->eeprom && dout->save) EEPROM_Save(dout->eeprom, &dout->value);
}

// Shared by `DOUT_Pulse` and `DOUT_PulseFreeze`, `_pulse` counts half-periods
static bool pulse_start(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms,
  uint16_t last_ms)
{
  if(dout->_pulse) return false;
  if(dout->relay && (ton_ms < DOUT_RELAY_STUN_ms || toff_ms < DOUT_RELAY_STUN_ms)) return false;
  if(count > UINT8_MAX / 2) return false;
  dout->_pulse = count * 2u;
  dout->_ton_ms = ton_ms;
  dout->_toff_ms = toff_ms;
  dout->_last_ms = last_ms;
  return true;
}

//-------------------------------------------------------------------------------------------- Init

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

void DOUT_Loop(DOUT_t *dout)
{
  if(tick_away(&dout->_stun)) return;
  if(dout->_pulse) {
    if(dout->relay && !DOUT_State(dout)) relay_cycles_inc(dout);
    if(dout->pwm) {
      // Flips against the level the channel holds now, `value` is where the sequence ends
      uint32_t level = DOUT_State(dout) ? 0 : dout->pwm->auto_reload;
      PWM_SetValue(dout->pwm, dout->channel, level);
    }
    else {
      GPIO_Tgl(&dout->gpio);
    }
    // Even half-periods are on, odd ones off, the last one holds `_last_ms`
    if(dout->_pulse == 1) dout->_stun = tick_keep(dout->_last_ms);
    else dout->_stun = tick_keep(dout->_pulse % 2 ? dout->_toff_ms : dout->_ton_ms);
    dout->_pulse--;
    return;
  }
  if(DOUT_State(dout) != dout->value) {
    if(dout->pwm) PWM_SetValue(dout->pwm, dout->channel, dout->value);
    else if(dout->value) {
      GPIO_Set(&dout->gpio);
      if(dout->relay) relay_cycles_inc(dout);
    }
    else {
      GPIO_Rst(&dout->gpio);
      dout->value = false;
    }
    if(dout->relay) dout->_stun = tick_keep(DOUT_RELAY_STUN_ms);
  }
}

//--------------------------------------------------------------------------------------------- PWM

float DOUT_GetFrequency(const DOUT_t *dout)
{
  if(!dout->pwm) return 0;
  return PWM_GetFrequency(dout->pwm);
}

float DOUT_Frequency(DOUT_t *dout, float frequency)
{
  if(!dout->pwm) return 0;
  return PWM_Frequency(dout->pwm, frequency);
}

float DOUT_GetDuty(const DOUT_t *dout)
{
  if(!dout->pwm) return NaN;
  return (float)dout->value * 100.0f / dout->pwm->auto_reload;
}

float DOUT_Duty(DOUT_t *dout, float duty)
{
  if(!dout->pwm) return NaN;
  duty = clamp(duty, 0.0f, 100.0f);
  uint32_t old_value = dout->pwm->value[dout->channel];
  PWM_SetValue(dout->pwm, dout->channel, duty * dout->pwm->auto_reload / 100.0f);
  dout->value = dout->pwm->value[dout->channel];
  if(old_value != dout->value) save_value(dout);
  return (float)dout->value * 100.0f / dout->pwm->auto_reload;
}

//------------------------------------------------------------------------------------------- Level

void DOUT_Set(DOUT_t *dout)
{
  if(dout->pwm) {
    if(dout->value == dout->pwm->auto_reload) return;
    dout->value = dout->pwm->auto_reload;
    PWM_SetValue(dout->pwm, dout->channel, dout->value);
  }
  else {
    if(dout->value) return;
    dout->value = true;
  }
  save_value(dout);
}

void DOUT_Rst(DOUT_t *dout)
{
  if(dout->pwm) {
    if(!dout->value) return;
    dout->value = 0;
    PWM_SetValue(dout->pwm, dout->channel, dout->value);
  }
  else {
    if(!dout->value) return;
    dout->value = false;
    if(dout->relay) dout->_stun = tick_keep(DOUT_RELAY_STUN_ms);
  }
  save_value(dout);
}

void DOUT_Tgl(DOUT_t *dout)
{
  if(dout->pwm) {
    if(dout->value) DOUT_Rst(dout);
    else DOUT_Set(dout);
  }
  else {
    dout->value = !dout->value;
    save_value(dout);
  }
}

void DOUT_Preset(DOUT_t *dout, bool value)
{
  if(value) DOUT_Set(dout);
  else DOUT_Rst(dout);
}

//------------------------------------------------------------------------------------------- Pulse

bool DOUT_Pulse(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms)
{
  return pulse_start(dout, count, ton_ms, toff_ms, toff_ms);
}

bool DOUT_PulseFreeze(DOUT_t *dout, uint8_t count, uint16_t ton_ms, uint16_t toff_ms,
  uint16_t freeze_ms)
{
  return pulse_start(dout, count, ton_ms, toff_ms, toff_ms + freeze_ms);
}

bool DOUT_State(const DOUT_t *dout)
{
  if(dout->pwm) return dout->pwm->value[dout->channel] != 0;
  return dout->gpio.set;
}

bool DOUT_IsPulse(const DOUT_t *dout)
{
  return dout->_pulse != 0;
}

//------------------------------------------------------------------------------------------ Retain

void DOUT_SaveValue(DOUT_t *dout, bool save)
{
  if(dout->eeprom && save != dout->save) {
    dout->save = save;
    EEPROM_Save(dout->eeprom, &dout->save);
  }
}

//-------------------------------------------------------------------------------------------------

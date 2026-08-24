// plc/per/din.c

#include "din.h"

//-------------------------------------------------------------------------------------------------

// Field behind a time parameter, with its default. The EEPROM key is the field address,
// so both the loader and the setter must reach the very same field
static uint32_t *DIN_TimeField(DIN_t *din, DIN_Time_t time, uint32_t *fallback)
{
  switch(time) {
    case DIN_Time_Toff: *fallback = GPIF_DEFAULT_TOFF_ms; return &din->gpif.toff_ms;
    case DIN_Time_TonLong: *fallback = GPIF_DEFAULT_TON_LONG_ms; return &din->gpif.ton_long_ms;
    case DIN_Time_ToffLong:
      *fallback = GPIF_DEFAULT_TOFF_LONG_ms; return &din->gpif.toff_long_ms;
    case DIN_Time_Toggle: *fallback = GPIF_DEFAULT_TOGGLE_ms; return &din->gpif.toggle_ms;
    case DIN_Time_Hold: *fallback = DIN_DEFAULT_HOLD_ms; return &din->hold_ms;
    default: *fallback = GPIF_DEFAULT_TON_ms; return &din->gpif.ton_ms;
  }
}

void DIN_Init(DIN_t *din)
{
  if(din->eeprom) {
    // Idempotent, but the descriptor may be shared with no output owning it
    EEPROM_Init(din->eeprom);
    // A value set in the board configuration wins over the stored one
    for(uint8_t time = DIN_Time_Ton; time <= DIN_Time_Hold; time++) {
      uint32_t fallback;
      uint32_t *field = DIN_TimeField(din, (DIN_Time_t)time, &fallback);
      if(!*field) EEPROM_Load(din->eeprom, field);
    }
  }
  if(!din->hold_ms) din->hold_ms = DIN_DEFAULT_HOLD_ms;
  bool raw = false;
  if(din->mode != DIN_Mode_FastCounter) {
    din->gpio.mode = GPIO_Mode_Input;
    GPIO_Init(&din->gpio);
    raw = GPIO_In(&din->gpio);
    if(din->mode == DIN_Mode_AC) {
      // Pulsing AC signal makes a single sample unreliable, seed deterministically
      din->_tick_hold = raw ? tick_keep(din->hold_ms) : 0;
      raw = false;
    }
  }
  // Also in fast counter mode, so the times and the filter state stay defined
  GPIF_Init(&din->gpif, raw);
}

void DIN_Loop(DIN_t *din)
{
  if(din->mode == DIN_Mode_FastCounter) return;
  bool raw = GPIO_In(&din->gpio);
  // AC pulse-stretch: hold high state over zero-crossing dropouts
  if(din->mode == DIN_Mode_AC) {
    if(raw) din->_tick_hold = tick_keep(din->hold_ms);
    else raw = tick_away(&din->_tick_hold);
  }
  GPIF_Loop(&din->gpif, raw);
}

//-------------------------------------------------------------------------------------------------

bool DIN_State(DIN_t *din)
{
  return GPIF_Input(&din->gpif);
}

bool DIN_Raw(DIN_t *din)
{
  return GPIO_In(&din->gpio);
}

bool DIN_Toggle(DIN_t *din)
{
  return GPIF_Toggle(&din->gpif);
}

bool DIN_Rise(DIN_t *din)
{
  return GPIF_Rise(&din->gpif);
}

bool DIN_Fall(DIN_t *din)
{
  return GPIF_Fall(&din->gpif);
}

bool DIN_Edge(DIN_t *din)
{
  return GPIF_Edge(&din->gpif);
}

bool DIN_RiseLong(DIN_t *din)
{
  return GPIF_RiseLong(&din->gpif);
}

bool DIN_FallLong(DIN_t *din)
{
  return GPIF_FallLong(&din->gpif);
}

bool DIN_EdgeLong(DIN_t *din)
{
  return GPIF_EdgeLong(&din->gpif);
}

//-------------------------------------------------------------------------------------------------

uint32_t DIN_Time(DIN_t *din, DIN_Time_t time, uint32_t ms)
{
  uint32_t fallback;
  uint32_t *field = DIN_TimeField(din, time, &fallback);
  if(ms && ms != *field) {
    uint32_t backup = *field;
    *field = (ms == DIN_DEFAULT_TIME) ? fallback : ms;
    // Rollback on save failure keeps RAM and EEPROM consistent
    if(din->eeprom && EEPROM_Save(din->eeprom, field)) *field = backup;
  }
  return *field;
}

//-------------------------------------------------------------------------------------------------

float DIN_Duty_Percent(DIN_t *din)
{
  if(din->mode != DIN_Mode_FastCounter || !din->pwmi) return NaN;
  float duty = din->pwmi->duty[din->channel];
  if(isNaN(duty)) return NaN;
  return din->gpio.reverse ? (100.0f - duty) : duty;
}

float DIN_Frequency_Hz(DIN_t *din)
{
  if(din->mode != DIN_Mode_FastCounter || !din->pwmi) return NaN;
  return din->pwmi->frequency[din->channel];
}

//-------------------------------------------------------------------------------------------------

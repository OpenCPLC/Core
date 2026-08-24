// lib/sys/gpif.c

#include "gpif.h"

//-------------------------------------------------------------------------------------------- GPIF

void GPIF_Init(GPIF_t *gpif, bool input)
{
  if(!gpif->ton_ms) gpif->ton_ms = GPIF_DEFAULT_TON_ms;
  if(!gpif->toff_ms) gpif->toff_ms = GPIF_DEFAULT_TOFF_ms;
  if(!gpif->ton_long_ms) gpif->ton_long_ms = GPIF_DEFAULT_TON_LONG_ms;
  if(!gpif->toff_long_ms) gpif->toff_long_ms = GPIF_DEFAULT_TOFF_LONG_ms;
  if(!gpif->toggle_ms) gpif->toggle_ms = GPIF_DEFAULT_TOGGLE_ms;
  gpif->_input = input;
  gpif->_toggle = false;
  gpif->_rise = false;
  gpif->_fall = false;
  gpif->_rise_long = false;
  gpif->_fall_long = false;
  gpif->_tick = 0;
  gpif->_tick_debounce = 0;
  gpif->_tick_long = 0;
  gpif->_tick_toggle = 0;
  gpif->_tick_reset = 0;
}

void GPIF_Loop(GPIF_t *gpif, bool input)
{
  // Every deadline below has one tick of resolution,
  // so a second visit within the same tick can only repeat work.
  // Skipping it keeps the filter cheap in a fast main loop.
  // The low word is enough: consecutive calls are never 2^32 ticks apart
  uint32_t tick = (uint32_t)tick_now();
  if(tick == gpif->_tick) return;
  gpif->_tick = tick;
  // Debounce: timer armed only while a transition is pending
  if(input == gpif->_input) {
    gpif->_tick_debounce = 0;
  }
  else if(!gpif->_tick_debounce) {
    gpif->_tick_debounce = input ? tick_keep(gpif->ton_ms) : tick_keep(gpif->toff_ms);
  }
  else if(tick_over(&gpif->_tick_debounce)) {
    gpif->_input = input;
    // Toggle watchdog: `_tick_toggle` runs only while transitions
    // keep arriving closer together than the reset window,
    // so a settled input never reaches it
    if(!gpif->_tick_reset) gpif->_tick_toggle = tick_keep(gpif->toggle_ms);
    gpif->_tick_reset = tick_keep(gpif->toggle_ms / 2);
    // Edge detection
    if(input) {
      gpif->_rise = true;
      gpif->_tick_long = tick_keep(gpif->ton_long_ms);
    }
    else {
      gpif->_fall = true;
      gpif->_tick_long = tick_keep(gpif->toff_long_ms);
    }
  }
  // Long press/release
  if(tick_over(&gpif->_tick_long)) {
    if(gpif->_input) gpif->_rise_long = true;
    else gpif->_fall_long = true;
  }
  // A quiet spell disarms the watchdog before it can fire
  if(tick_over(&gpif->_tick_reset)) gpif->_tick_toggle = 0;
  if(tick_over(&gpif->_tick_toggle)) gpif->_toggle = !gpif->_toggle;
}

bool GPIF_Input(GPIF_t *gpif) { return gpif->_input; }
bool GPIF_Toggle(GPIF_t *gpif) { return gpif->_toggle; }

bool GPIF_Rise(GPIF_t *gpif)
{
  if(gpif->_rise) { gpif->_rise = false; return true; }
  return false;
}

bool GPIF_Fall(GPIF_t *gpif)
{
  if(gpif->_fall) { gpif->_fall = false; return true; }
  return false;
}

bool GPIF_Edge(GPIF_t *gpif)
{
  // Both flags can be pending when the consumer polls slower than the input moves,
  // so clear them together instead of leaving one to report a phantom edge later
  bool rise = GPIF_Rise(gpif);
  bool fall = GPIF_Fall(gpif);
  return rise || fall;
}

bool GPIF_RiseLong(GPIF_t *gpif)
{
  if(gpif->_rise_long) { gpif->_rise_long = false; return true; }
  return false;
}

bool GPIF_FallLong(GPIF_t *gpif)
{
  if(gpif->_fall_long) { gpif->_fall_long = false; return true; }
  return false;
}

bool GPIF_EdgeLong(GPIF_t *gpif)
{
  bool rise = GPIF_RiseLong(gpif);
  bool fall = GPIF_FallLong(gpif);
  return rise || fall;
}

//-------------------------------------------------------------------------------------------------

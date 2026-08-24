// plc/per/ain.c

#include "ain.h"

// Working range of the voltage input and the 4-20mA live-zero window, in microvolts
#define AIN_RANGE_uV      10000000u
#define AIN_OVERRANGE_uV  10250000u
#define AIN_420_OFFSET_uV 2000000u
#define AIN_420_SPAN_uV   8000000u
#define AIN_420_BREAK_uV  100000u

static AIN_t *ain_vref = NULL;

void AIN_SetVref(AIN_t *vref)
{
  ain_vref = vref;
}

uint32_t AIN_Raw(AIN_t *ain)
{
  if(tick_away(&ain->tick)) return ain->_raw;
  // Window mean carried in Q16: the fractional part feeds the EMA, so resolution
  // accumulates over time instead of being cut at every window
  uint32_t mean = mid_mean_u16(ain->data, ain->count, 65536);
  if(ain->ema && ain->_init) ain->_raw = ema_filter_u32(mean, ain->_raw, ain->ema);
  else {
    ain->_raw = mean; // first window seeds the filter, no ramp-up from zero
    ain->_init = true;
  }
  ain->tick = tick_keep(AIN_AVERAGE_TIME_ms); // one filter step per recording window
  LOG_Debug("Analog input %s raw-value: %d", ain->name, (ain->_raw + 32768) >> 16);
  return ain->_raw;
}

// Voltage at the ADC pin in microvolts, all-integer: the single place the ratiometric
// conversion lives. Q8 inputs leave 64-bit headroom for the resistor macros in any unit
static uint32_t AIN_Pin_uV(AIN_t *ain)
{
  uint32_t raw = AIN_Raw(ain) >> 8;
  if(ain_vref && ain_vref != ain) {
    uint32_t vref = AIN_Raw(ain_vref) >> 8;
    if(vref) {
      uint64_t num = (uint64_t)raw * 3000000 * ADC_VREFINT_CAL;
      uint64_t den = (uint64_t)vref * 4095;
      uint64_t uv = (num + den / 2) / den;
      return uv > 4000000 ? 4000000 : (uint32_t)uv; // a dying VREF cannot blow the scale
    }
  }
  uint64_t full = (uint64_t)AIN_FULL_SCALE * 256;
  return (uint32_t)(((uint64_t)raw * 3300000 + full / 2) / full);
}

// Input voltage in microvolts, after divider scaling:
// the full-precision value both the integer and the float conversions start from
static uint32_t AIN_uV(AIN_t *ain)
{
  return (uint32_t)(((uint64_t)AIN_Pin_uV(ain) * (AIN_RESISTOR_UP + AIN_RESISTOR_DOWN)
    + AIN_RESISTOR_DOWN / 2) / AIN_RESISTOR_DOWN);
}

// Range verdict: `0` in range, `±Inf` out of range.
// Logs the violation, so every unit function reports errors the same way
static float AIN_RangeError(AIN_t *ain, uint32_t uv)
{
  if(uv > AIN_OVERRANGE_uV || (ain->thresh_high_uV && uv > ain->thresh_high_uV)) {
    LOG_Message(AIN_LOG_LEVEL, "Analog input %s over-range", ain->name);
    return Inf;
  }
  if((ain->mode_4_20mA && uv < AIN_420_BREAK_uV) || uv < ain->thresh_low_uV) {
    LOG_Message(AIN_LOG_LEVEL, "Analog input %s under-range", ain->name);
    return -Inf;
  }
  return 0.0f;
}

//--------------------------------------------------------------------------------------- Threshold

// Threshold value in `unit` back to internal microvolts,
// the unit of the whole conversion path;
// zero passes through, keeping the threshold disabled.
// Percent honors the 4-20mA window
static uint32_t AIN_ThreshTo_uV(AIN_t *ain, float value, AIN_Thresh_t unit)
{
  if(value <= 0.0f) return 0;
  float uv;
  switch(unit) {
    case AIN_Thresh_mA: uv = 500000.0f * value; break; // 500Ω: 20mA is 10V
    case AIN_Thresh_Percent:
      uv = ain->mode_4_20mA
        ? AIN_420_OFFSET_uV + value * (AIN_420_SPAN_uV / 100)
        : value * (AIN_RANGE_uV / 100);
      break;
    default: uv = 1000000.0f * value; // volts
  }
  return (uint32_t)(uv + 0.5f);
}

void AIN_Threshold(AIN_t *ain, float low, float high, AIN_Thresh_t unit)
{
  ain->thresh_low_uV = AIN_ThreshTo_uV(ain, low, unit);
  ain->thresh_high_uV = AIN_ThreshTo_uV(ain, high, unit);
}

//--------------------------------------------------------------------------------------- Float API

float AIN_PinVoltage_V(AIN_t *ain)
{
  return (float)AIN_Pin_uV(ain) / 1000000;
}

float AIN_Voltage_V(AIN_t *ain)
{
  uint32_t uv = AIN_uV(ain);
  float error = AIN_RangeError(ain, uv);
  if(error) return error;
  return (float)uv / 1000000;
}

float AIN_Current_mA(AIN_t *ain)
{
  uint32_t uv = AIN_uV(ain);
  float error = AIN_RangeError(ain, uv);
  if(error) return error;
  return (float)uv / 500000;
}

float AIN_Normalized(AIN_t *ain)
{
  uint32_t uv = AIN_uV(ain);
  float error = AIN_RangeError(ain, uv);
  if(error) return error;
  float norm = ain->mode_4_20mA
    ? ((float)uv - AIN_420_OFFSET_uV) / AIN_420_SPAN_uV : (float)uv / AIN_RANGE_uV;
  if(norm < 0.0f) norm = 0.0f;
  if(norm > 1.0f) norm = 1.0f;
  return norm;
}

float AIN_Percent(AIN_t *ain)
{
  return 100.0f * AIN_Normalized(ain);
}

float POT_Normalized(AIN_t *ain)
{
  return (float)AIN_Raw(ain) / ((float)AIN_FULL_SCALE * 65536);
}

float POT_Percent(AIN_t *ain)
{
  return 100.0f * POT_Normalized(ain);
}

float POT_Voltage_V(AIN_t *ain)
{
  return 3.3f * POT_Normalized(ain);
}

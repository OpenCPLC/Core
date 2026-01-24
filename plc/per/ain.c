#include "ain.h"

/**
 * @brief Split ADC buffer into separate AIN channel arrays.
 * @param buff Pointer to ADC buffer.
 * @param data Output 2D array [channels][samples].
 * @param channels Number of ADC channels.
 * @param samples Number of samples per channel.
 */
void AIN_Sort(uint16_t *buff, uint16_t channels, uint16_t samples, uint16_t data[channels][samples])
{
  for(uint16_t n = 0; n < samples; n++) {
    for(uint8_t cha = 0; cha < channels; cha++) {
      data[cha][n] = buff[(n * channels) + cha];
    }
  }
}

/**
 * @brief Set lower and upper threshold for analog input.
 * @param ain Pointer to `AIN_t` structure.
 * @param low Lower threshold value.
 * @param high Upper threshold value.
 * @param scale Threshold unit: voltage `[V]`, current `[mA]`, or percent `[%]`.
 */
void AIN_Threshold(AIN_t *ain, float low, float high, AIN_Thresh_e scale)
{
  ain->threshold_low = low / scale;
  ain->threshold_high = high / scale;
}

/**
 * @brief Get filtered ADC value without unit conversion.
 * @param ain Pointer to `AIN_t` structure representing analog input.
 * @return Filtered 16-bit ADC value as `float`.
 */
float AIN_Raw(AIN_t *ain)
{
  if(tick_over(&ain->tick)) return ain->value;
  uint16_t size = ain->count / 3;
  sort_asc_u16(ain->data, ain->count);
  ain->value = avg_u16(&(ain->data[size]), size);
  ain->tick = tick_keep(AIN_AVERAGE_TIME_ms / 2);
  LOG_Debug("Analog input %s raw-value: %F", ain->name, ain->value);
  return ain->value;
}

/**
 * @brief Get input voltage `[V]` after ADC conversion and scaling.
 * @param ain Pointer to `AIN_t` structure.
 * @return Voltage `[V]`; returns `+Inf` if over-range, `-Inf` if under-range.
 */
static float AIN_Volts(AIN_t *ain)
{
  float raw = AIN_Raw(ain);
  float volts = resistor_divider_factor(3.3, AIN_RESISTOR_UP, AIN_RESISTOR_DOWN, 16) * raw;
  if(volts > 10.25 || (ain->threshold_high && volts > ain->threshold_high)) return Inf;
  if((ain->mode_4_20mA && (volts < 0.1)) || volts < ain->threshold_low) return -Inf;
  return volts;
}

/**
 * @brief Print error message for analog input if value is out of range.
 * @param ain Pointer to `AIN_t`.
 * @param value Result from `AIN_Volts()`.
 * @param subject Error context string (e.g. "voltage", "current").
 */
static void AIN_LogError(AIN_t *ain, float value, const char *subject)
{
  if(isInf(value)) {
    if(value > 0.0f) LOG_Message(AIN_LOG_LEVEL, "Analog input %s over-%s", ain->name, subject);
    else LOG_Message(AIN_LOG_LEVEL, "Analog input %s under-%s", ain->name, subject);
  }
  else if(isNaN(value)) {
    LOG_Message(AIN_LOG_LEVEL, "Analog input %s invalid %s", ain->name, subject);
  }
}

/**
 * @brief Get input voltage `[V]` with error check.
 * @param ain Pointer to `AIN_t` structure.
 * @return Value in `[V]`, `+Inf` if over-range, `-Inf` if under-range.
 */
float AIN_Voltage_V(AIN_t *ain)
{
  float volts = AIN_Volts(ain);
  if(AIN_IsError(volts)) return volts;
  AIN_LogError(ain, volts, "voltage");
  return volts;  
}

/**
 * @brief Get input current `[mA]` with error check.
 * @param ain Pointer to `AIN_t` structure.
 * @return Value in `[mA]`, `+Inf` if over-range, `-Inf` if under-range.
 */
float AIN_Current_mA(AIN_t *ain)
{
  float volts = AIN_Volts(ain);
  if(AIN_IsError(volts)) return volts;
  AIN_LogError(ain, volts, "current");
  return volts * 2.0f;
}

/**
 * @brief Get analog input as percent `[%]` with error check.
 * @param ain Pointer to `AIN_t` structure.
 * @return Value in `[0–100%]`, `+Inf` if over-range, `-Inf` if under-range.
 */
float AIN_Percent(AIN_t *ain)
{
  float volts = AIN_Volts(ain);
  if(AIN_IsError(volts)) return volts;
  AIN_LogError(ain, volts, "value");
  float percent = volts * 10.0f;
  if(ain->mode_4_20mA) {
    percent = (percent - 20.0f) * 5.0f / 4.0f;
    if(percent < 0.0f) percent = 0.0f;
  }
  if(percent > 100.0f) percent = 100.0f;
  return percent;
}

/**
 * @brief Get analog input as normalized value `[0–1]` with error check.
 * @param ain Pointer to `AIN_t` structure.
 * @return Value in `[0–1]`, `+INF` if over-range, `-INF` if under-range.
 */
float AIN_Normalized(AIN_t *ain)
{
  float percent = AIN_Percent(ain);
  if(AIN_IsError(percent)) return percent;
  return percent / 100.0f;
}

/**
 * @brief Get potentiometer voltage `[V]` from ADC raw value.
 * @param ain Pointer to `AIN_t` structure.
 * @return Voltage in range `[0–3.3V]`.
 */
float POT_Voltage_V(AIN_t *ain)
{
  float raw = AIN_Raw(ain);
  return volts_factor(3.3f, 16) * raw;
}

/**
 * @brief Get potentiometer value as percent `[%]`.
 * @param ain Pointer to `AIN_t` structure.
 * @return Value in range `[0–100%]`.
 */
float POT_Percent(AIN_t *ain)
{
  float volts = POT_Voltage_V(ain);
  return volts * 100.0f / 3.3f;
}

/**
 * @brief Get potentiometer value as normalized `[0–1]`.
 * @param ain Pointer to `AIN_t` structure.
 * @return Value in range `[0–1]`.
 */
float POT_Normalized(AIN_t *ain)
{
  return POT_Percent(ain) / 100.0f;
}

// plc/per/ain.h

#ifndef AIN_H_
#define AIN_H_

#include <stdint.h>
#include <stdbool.h>
#include <xmath.h>
#include "vrts.h"
#include "adc.h"
#include "log.h"
#include "main.h"

// Average ADC sampling (filtering) time in milliseconds
#ifndef AIN_AVERAGE_TIME_ms
  #define AIN_AVERAGE_TIME_ms 200
#endif

// Log level for range errors (over/under)
#ifndef AIN_LOG_LEVEL
  #define AIN_LOG_LEVEL LOG_LEVEL_ERR
#endif

// Upper resistor (near VCC) in measurement branch, can be adjusted in custom designs
#ifndef AIN_RESISTOR_UP
  #define AIN_RESISTOR_UP 340
#endif

// Lower resistor (near GND) in measurement branch, can be adjusted in custom designs
#ifndef AIN_RESISTOR_DOWN
  #define AIN_RESISTOR_DOWN 160
#endif

// Conversion time per sample in ADC cycles,
// one number driving both the enum name and the buffer math;
// an unsupported value fails at compile time.
// 52 cycles leaves ~2.5µs of sampling at 16MHz: fine with the input RC filter,
// raise for a high-impedance source without a capacitor
#ifndef AIN_SAMPLING_CYCLES
  #define AIN_SAMPLING_CYCLES 52
#endif
#define AIN_SAMPLING_ENUM_(cycles) ADC_SamplingTime_##cycles
#define AIN_SAMPLING_ENUM(cycles) AIN_SAMPLING_ENUM_(cycles)
#define AIN_SAMPLING_TIME AIN_SAMPLING_ENUM(AIN_SAMPLING_CYCLES)

// Hardware oversampling of the AIN recording.
// The shift and the full scale derive from the ratio,
// keeping results on the 16x scale (AIN_FULL_SCALE = 65520);
// overriding the ratio alone cannot break the conversions
#ifndef AIN_OVERSAMPLING_RATIO
  #define AIN_OVERSAMPLING_RATIO ADC_OversamplingRatio_256
#endif
#define AIN_OVERSAMPLING_SHIFT ((AIN_OVERSAMPLING_RATIO) + 1 - 4)
#define AIN_FULL_SCALE adc_oversampling_max(AIN_OVERSAMPLING_RATIO, AIN_OVERSAMPLING_SHIFT)

// Check if value is out of range (±INF: over/under)
#define AIN_IsError(value) isinf(value)
#define AIN_IsOK(value)    (!isinf(value))

// Unit of the threshold configuration
typedef enum {
  AIN_Thresh_V,       // Voltage [V]
  AIN_Thresh_mA,      // Current-loop [mA]
  AIN_Thresh_Percent  // Percent of the working range [%]
} AIN_Thresh_t;

//-------------------------------------------------------------------------------------------------

_Static_assert(AIN_OVERSAMPLING_RATIO >= ADC_OversamplingRatio_16,
  "AIN oversampling ratio below 16x cannot hold the 16x result scale");

/**
 * @brief Analog input (AIN) structure for ADC measurement handling.
 * @param name Name of analog input.
 * @param data Pointer to ADC data buffer.
 * @param count Number of samples in buffer.
 * @param mode_4_20mA `true` if input works in 4–20 mA (or 2–10V) mode.
 * @param ema Smoothing between windows for slowly varying signals, as a shift:
 * time constant ≈ window time × 2^ema, `0` = off. Integer filter, sub-LSB resolution.
 * @param thresh_high_uV Upper threshold limit [µV], `0` = off.
 * @param thresh_low_uV Lower threshold limit [µV], `0` = off.
 * @param tick Timestamp for sampling / filtering.
 * Internal:
 * @param _raw Filter cache: raw value in Q16
 * @param _init Filter already seeded
 */
typedef struct {
  const char *name;
  uint16_t *data;
  uint16_t count;
  bool mode_4_20mA;
  uint8_t ema;
  uint32_t thresh_high_uV;
  uint32_t thresh_low_uV;
  uint64_t tick;
  uint32_t _raw;
  bool _init;
} AIN_t;

/**
 * @brief Register the analog input recording the `ADC_IN_VREFEN` channel. Unit
 * conversions turn ratiometric: the raw value is divided by the filtered internal
 * reference, so the supply voltage and the oversampling scale cancel out and accuracy
 * follows the factory calibration. Without it a nominal 3.3V supply is assumed.
 * @param[in] vref Analog input fed by the VREFINT channel of the same recording
 */
void AIN_SetVref(AIN_t *vref);

/**
 * @brief Set lower and upper threshold for analog input, in any unit; `0` = off.
 * Ratio units follow the working range: 0-10V, or the 2-10V window when `mode_4_20mA`
 * is set. Stored internally in [µV].
 * @param[in] low Lower threshold, in `unit`
 * @param[in] high Upper threshold, in `unit`
 * @param[in] unit Threshold unit (`AIN_Thresh_...`)
 */
void AIN_Threshold(AIN_t *ain, float low, float high, AIN_Thresh_t unit);

/**
 * @brief Filtered ADC value without unit conversion, at full resolution:
 * med-mean over the recording window, then an optional EMA between windows (`ema` field).
 * @return Filtered ADC value in Q16, `0` to `AIN_FULL_SCALE << 16`
 */
uint32_t AIN_Raw(AIN_t *ain);

//--------------------------------------------------------------------------------------- Float API

/**
 * @brief Voltage at the ADC pin, before the input divider, full precision.
 * @return Voltage at the pin [V]
 */
float AIN_PinVoltage_V(AIN_t *ain);

/**
 * @brief Input voltage with range check, full precision.
 * @return Voltage [V], `±INF` out of range
 */
float AIN_Voltage_V(AIN_t *ain);

/**
 * @brief Input current with range check, full precision.
 * @return Current [mA], `±INF` out of range
 */
float AIN_Current_mA(AIN_t *ain);

/**
 * @brief Input value as percent of the working range, with range check, full precision.
 * @return Value [0-100%], `±INF` out of range
 */
float AIN_Percent(AIN_t *ain);

/**
 * @brief Input value normalized to the working range: 0-10V, or the 2-10V window
 * when `mode_4_20mA` is set, with range check, full precision.
 * @return Value [0-1], `±INF` out of range
 */
float AIN_Normalized(AIN_t *ain);

/**
 * @brief Potentiometer voltage, full precision.
 * @return Voltage in range [0-3.3V]
 */
float POT_Voltage_V(AIN_t *ain);

/**
 * @brief Potentiometer position as percent, full precision.
 * @return Position [0-100%]
 */
float POT_Percent(AIN_t *ain);

/**
 * @brief Potentiometer position normalized, full precision.
 * @return Position [0-1]
 */
float POT_Normalized(AIN_t *ain);

//-------------------------------------------------------------------------------------------------
#endif

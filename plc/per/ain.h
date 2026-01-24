#ifndef AIN_H_
#define AIN_H_

#include <stdint.h>
#include <stdbool.h>
#include "extmath.h"
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

// Check if value is out of range (±INF)
#define AIN_IsError(value) isinf(value)
#define AIN_IsOK(value)    (!isinf(value))

// Unit scale for threshold configuration.
typedef enum { 
  AIN_Thresh_Voltage_V = 1,  // Voltage in `[V]`
  AIN_Thresh_Current_mA = 2, // Current in `[mA]`
  AIN_Thresh_Percent = 5,    // Percent `[0–100%]`
} AIN_Thresh_e;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Analog input (AIN) structure for ADC measurement handling.
 * @param name Name of analog input.
 * @param data Pointer to ADC data buffer.
 * @param count Number of samples in buffer.
 * @param mode_4_20mA `true` if input works in 4–20 mA (or 2–10V) mode.
 * @param value Last raw value (without conversion).
 * @param threshold_high Upper threshold limit.
 * @param threshold_low Lower threshold limit.
 * @param tick Timestamp for sampling / filtering.
 */
typedef struct {
  const char *name;
  uint16_t *data;
  uint16_t count;
  bool mode_4_20mA;
  float value;
  float threshold_high;
  float threshold_low;
  uint64_t tick;
} AIN_t;

void AIN_Sort(uint16_t *buff, uint16_t channels, uint16_t samples, uint16_t data[channels][samples]);
void AIN_Threshold(AIN_t *ain, float down, float up, AIN_Thresh_e scale);
float AIN_Raw(AIN_t *ain);;
float AIN_Voltage_V(AIN_t *ain);
float AIN_Current_mA(AIN_t *ain);
float AIN_Percent(AIN_t *ain);
float AIN_Normalized(AIN_t *ain);
float POT_Voltage_V(AIN_t *ain);
float POT_Percent(AIN_t *ain);
float POT_Normalized(AIN_t *ain);

//-------------------------------------------------------------------------------------------------
#endif



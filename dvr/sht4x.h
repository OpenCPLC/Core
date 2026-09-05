// dvr/sht4x.h

#ifndef SHT4X_H_
#define SHT4X_H_

#include <stdint.h>
#include <xmath.h>
#include "i2c_master.h"

//--------------------------------------------------------------------------------------- Constants

#define SHT4X_ADDRESS   0x44  // SHT4x-A, factory default
#define SHT4X_ADDRESS_B 0x45  // SHT4x-B variant

//------------------------------------------------------------------------------------------- Types

// Measurement commands carry their own conversion time, see `SHT4X_Delay_ms`
typedef enum {
  SHT4X_Mode_High = 0xFD,        // Highest repeatability, 8.3ms
  SHT4X_Mode_Medium = 0xF6,      // Medium repeatability, 4.5ms
  SHT4X_Mode_Low = 0xE0,         // Lowest repeatability, 1.7ms
  SHT4X_CMD_SoftReset = 0x94,    // Soft reset, 1ms
  SHT4X_CMD_Serial = 0x89,       // Read serial number
  SHT4X_CMD_Heater200mW_1s = 0x39,
  SHT4X_CMD_Heater200mW_100ms = 0x32,
  SHT4X_CMD_Heater110mW_1s = 0x2F,
  SHT4X_CMD_Heater110mW_100ms = 0x24,
  SHT4X_CMD_Heater20mW_1s = 0x1E,
  SHT4X_CMD_Heater20mW_100ms = 0x15
} SHT4X_CMD_t;

typedef struct {
  int32_t humidity;     // [%RH]
  int32_t temperature;  // [°C]
} SHT4X_Raw_t;

/**
 * @brief Temperature and humidity sensor on I2C (SHT40, SHT41, SHT45).
 * @param[in] i2c Bus the sensor sits on
 * @param[in] address Device address (`0` = `SHT4X_ADDRESS`)
 * @param[in] mode Measurement repeatability, see `SHT4X_CMD_t`
 * @param[in] oversampling Samples averaged in software (`0` = 1)
 * @param[in] expiry_ms Timeout after which no sensor response sets `NaN`
 * @param[in] interval_ms Interval between measurements, at least 200ms below `expiry_ms`
 * @param[out] humidity Relative humidity in %RH
 * @param[out] temperature Temperature in °C
 * Internal:
 * @param _raw Accumulated raw samples
 * @param _buff_rx Response buffer
 * @param _expiry_tick Deadline that invalidates the readings
 * @param _interval_tick Deadline of the next measurement
 */
typedef struct {
  I2C_Master_t *i2c;
  uint8_t address;
  uint8_t mode;
  uint16_t oversampling;
  uint16_t expiry_ms;
  uint16_t interval_ms;
  float humidity;
  float temperature;
  // internal
  SHT4X_Raw_t _raw;
  uint8_t _buff_rx[6];
  uint64_t _expiry_tick;
  uint64_t _interval_tick;
} SHT4X_t;

//------------------------------------------------------------------------------------ Constructors

/**
 * @brief Declare sensor measuring with the highest repeatability.
 * @param name Variable name.
 * @param bus `I2C_Master_t` the sensor sits on.
 */
#define SHT4X_New(name, bus) \
  SHT4X_t name = { .i2c = (bus), .mode = SHT4X_Mode_High, \
    .expiry_ms = 1000, .interval_ms = 500, .humidity = NaN, .temperature = NaN }

/**
 * @brief Declare sensor measuring with the lowest repeatability, for the lowest power.
 * @param name Variable name.
 * @param bus `I2C_Master_t` the sensor sits on.
 */
#define SHT4X_NewLowPower(name, bus) \
  SHT4X_t name = { .i2c = (bus), .mode = SHT4X_Mode_Low, \
    .expiry_ms = 1000, .interval_ms = 500, .humidity = NaN, .temperature = NaN }

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Measure when the interval elapsed, otherwise return at once.
 * @param[in] sht4x Sensor instance
 * @return `OK` when done or skipped, `BUSY` when the bus is taken, `ERR` on a failed transfer.
 */
status_t SHT4X_Loop(SHT4X_t *sht4x);

/**
 * @brief Send a command that returns no data, such as a reset or a heater pulse.
 * @param[in] sht4x Sensor instance
 * @param[in] cmd Command, see `SHT4X_CMD_t`
 * @return `true` when the sensor acknowledged.
 */
bool SHT4X_Command(SHT4X_t *sht4x, SHT4X_CMD_t cmd);

float SHT4X_Temperature_C(SHT4X_t *sht4x);  // Last temperature in °C, `NaN` when expired
float SHT4X_Humidity_RH(SHT4X_t *sht4x);    // Last humidity in %RH, `NaN` when expired

#endif

// dvr/shtc3.h

#ifndef SHTC3_H_
#define SHTC3_H_

#include <stdint.h>
#include <xmath.h>
#include "i2c_master.h"

//--------------------------------------------------------------------------------------- Constants

#define SHTC3_ADDRESS 0x70

//------------------------------------------------------------------------------------------- Types

typedef enum {
  SHTC3_CMD_Sleep = 0xB098,
  SHTC3_CMD_WakuUp = 0x3517,
  SHTC3_Mode_NormalClockEnabled = 0x7CA2,
  SHTC3_Mode_NormalClockDisable = 0x7866,
  SHTC3_Mode_LowPowerClockEnabled = 0x6458,
  SHTC3_Mode_LowPowerClockDisable = 0x609C
} SHTC3_CMD_t;

typedef struct {
  int32_t humidity;     // [%RH]
  int32_t temperature;  // [°C]
} SHTC3_Raw_t;

/**
 * @brief Temperature and humidity sensor on I2C.
 * @param[in] i2c Bus the sensor sits on
 * @param[in] address Device address (`0` = `SHTC3_ADDRESS`)
 * @param[in] mode Operating mode, see `SHTC3_CMD_t`
 * @param[in] oversampling Samples averaged in software (`0` = 1)
 * @param[in] expiry_ms Timeout after which no sensor response sets `NaN`
 * @param[in] interval_ms Interval between measurements, at least 200ms below `expiry_ms`
 * @param[out] humidity Relative humidity in %RH
 * @param[out] temperature Temperature in °C
 * Internal:
 * @param _raw Accumulated raw samples
 * @param _buff_tx Command buffer
 * @param _buff_rx Response buffer
 * @param _expiry_tick Deadline that invalidates the readings
 * @param _interval_tick Deadline of the next measurement
 */
typedef struct {
  I2C_Master_t *i2c;
  uint8_t address;
  uint16_t mode;
  uint16_t oversampling;
  uint16_t expiry_ms;
  uint16_t interval_ms;
  float humidity;
  float temperature;
  // internal
  SHTC3_Raw_t _raw;
  uint8_t _buff_tx[2];
  uint8_t _buff_rx[6];
  uint64_t _expiry_tick;
  uint64_t _interval_tick;
} SHTC3_t;

//------------------------------------------------------------------------------------ Constructors

/**
 * @brief Declare sensor in normal mode.
 * @param name Variable name.
 * @param bus `I2C_Master_t` the sensor sits on.
 */
#define SHTC3_New(name, bus) \
  SHTC3_t name = { .i2c = (bus), .mode = SHTC3_Mode_NormalClockEnabled, \
    .expiry_ms = 1000, .interval_ms = 500, .humidity = NaN, .temperature = NaN }

/**
 * @brief Declare sensor in low power mode.
 * @param name Variable name.
 * @param bus `I2C_Master_t` the sensor sits on.
 */
#define SHTC3_NewLowPower(name, bus) \
  SHTC3_t name = { .i2c = (bus), .mode = SHTC3_Mode_LowPowerClockEnabled, \
    .expiry_ms = 1000, .interval_ms = 500, .humidity = NaN, .temperature = NaN }

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Measure when the interval elapsed, otherwise return at once.
 * @param[in] shtc3 Sensor instance
 * @return `OK` when done or skipped, `BUSY` when the bus is taken, `ERR` on a failed transfer.
 */
status_t SHTC3_Loop(SHTC3_t *shtc3);

float SHTC3_Temperature_C(SHTC3_t *shtc3);  // Last temperature in °C, `NaN` when expired
float SHTC3_Humidity_RH(SHTC3_t *shtc3);    // Last humidity in %RH, `NaN` when expired

#endif

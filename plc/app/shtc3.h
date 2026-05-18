// plc/app/shtc3.h

#ifndef SHTC3_H_
#define SHTC3_H_

#include <stdint.h>
#include <xmath.h>
#include "twi.h"

//-------------------------------------------------------------------------------------------------

#define SHTC3_ADRRESS 0x70

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

typedef struct {
  uint16_t mode;          // Operating mode: Normal | Low-Power, Clock Enable | Disable
  uint16_t oversampling;  // Number of samples for software oversampling
  SHTC3_Raw_t raw;        // Raw measurements before conversion
  float humidity;         // Humidity after conversion
  float temperature;      // Temperature after conversion
  uint16_t expiry_ms;     // Timeout after which no sensor response sets `NaN`
  uint16_t interval_ms;   // Interval between measurements (>= 200ms shorter than `expiry_ms`)
  uint8_t buff_tx[2];
  uint8_t buff_rx[6];
  uint64_t expiry_tick;
  uint64_t interval_tick;
} SHTC3_t;

status_t SHTC3_Loop(void);
float SHTC3_Temperature_C(void);
float SHTC3_Humidity_RH(void);

//-------------------------------------------------------------------------------------------------
#endif
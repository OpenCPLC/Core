// plc/per/max31865.h

#ifndef MAX31865_H_
#define MAX31865_H_

//-------------------------------------------------------------------------------------------------

#include "spi_master.h"
#include "log.h"
#include "xmath.h"

#define MAX31865_CFG_BIAS  (1 << 7)
#define MAX31865_CFG_AUTO  (1 << 6)
#define MAX31865_CFG_SHOT  (1 << 5)
#define MAX31865_CFG_FSCLR (1 << 1)

typedef enum {
  MAX31865_Reg_Read_Configuration = 0x00,
  MAX31865_Reg_Read_RTD_MSB = 0x01,
  MAX31865_Reg_Read_RTD_LSB = 0x02,
  MAX31865_Reg_Read_HighFaultThreshold_MSB = 0x03,
  MAX31865_Reg_Read_HighFaultThreshold_LSB = 0x04,
  MAX31865_Reg_Read_LowFaultThreshold_MSB = 0x05,
  MAX31865_Reg_Read_LowFaultThreshold_LSB = 0x06,
  MAX31865_Reg_Read_FaultStatus = 0x07,
  MAX31865_Reg_Write_Configuration = 0x80,
  MAX31865_Reg_Write_HighFaultThreshold_MSB = 0x83,
  MAX31865_Reg_Write_HighFaultThreshold_LSB = 0x84,
  MAX31865_Reg_Write_LowFaultThreshold_MSB = 0x85,
  MAX31865_Reg_Write_LowFaultThreshold_LSB = 0x86,
} MAX31865_Reg_t;

typedef enum {
  RTD_Type_None = 0,
  RTD_Type_PT100 = 100,
  RTD_Type_PT1000 = 1000
} RTD_Type_t;

typedef enum {
  RTD_Wire_2 = 0,
  RTD_Wire_3 = 1,
  RTD_Wire_4 = 0
} RTD_Wire_t;

typedef enum {
  RTD_Reject_60Hz = 0,
  RTD_Reject_50Hz = 1
} RTD_Reject_t;

typedef struct {
  const char *name;
  SPI_Master_t *spi;       // SPI interface
  GPIO_t *cs;              // CS (chip select) pin
  GPIO_t *ready;           // DATA-READY pin, signals conversion complete
  float nominal_ohms;      // Nominal sensor resistance in Ω, e.g. `RTD_Type_PT100`
  float reference_ohms;    // Reference resistor value
  RTD_Wire_t wire;         // Wiring: 2-, 3-, or 4-wire
  RTD_Reject_t reject;     // Mains noise rejection: 50Hz or 60Hz
  uint16_t oversampling;   // Number of samples for software oversampling
  uint16_t expiry_ms;      // Timeout after which no sensor response sets `NaN`
  uint16_t interval_ms;    // Interval between measurements (>= 200ms shorter than `expiry_ms`)
  uint16_t raw;
  float raw_float;
  volatile float temperature;
  uint8_t buff[3];
  uint64_t expiry_tick;
  uint64_t interval_tick;
} MAX31865_t;

void MAX31865_Init(MAX31865_t *rtd);        // Initialize the MAX31865 module
status_t MAX31865_Loop(MAX31865_t *rtd);    // Main measurement loop
float RTD_Resistance_Ohm(MAX31865_t *rtd);  // Compute RTD resistance in Ω
float RTD_Temperature_C(MAX31865_t *rtd);   // Compute temperature in °C (PT100/PT1000)

//-------------------------------------------------------------------------------------------------
#endif
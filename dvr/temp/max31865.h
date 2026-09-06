// dvr/temp/max31865.h

#ifndef MAX31865_H_
#define MAX31865_H_

#include "gpio.h"
#include "spi_master.h"
#include "xdef.h"

//------------------------------------------------------------------------------------------- Types

// Configuration register bits
#define MAX31865_CFG_BIAS  (1u << 7)
#define MAX31865_CFG_AUTO  (1u << 6)
#define MAX31865_CFG_SHOT  (1u << 5)
#define MAX31865_CFG_FSCLR (1u << 1)

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
  MAX31865_Reg_Write_LowFaultThreshold_LSB = 0x86
} MAX31865_Reg_t;

// Nominal sensor resistance [ohm]
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

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief RTD front end on the SPI bus, PT100 or PT1000.
 * @param[in] name Name shown in the log
 * @param[in] spi SPI bus
 * @param[in] cs Chip select, `NULL` = the one configured on the bus
 * @param[in] ready Data-ready pin, low when a conversion is complete
 * @param[in] nominal_ohms Nominal sensor resistance [ohm], `RTD_Type_...`; `0` = PT100
 * @param[in] reference_ohms Reference resistor [ohm], `0` = four times the nominal
 * @param[in] wire Wiring: 2, 3 or 4 wires
 * @param[in] reject Mains rejection filter, 50 or 60 Hz
 * @param[in] oversampling Conversions averaged per measurement, `0` = a single shot
 * @param[in] expiry_ms Time without a measurement after which the value turns `NaN` [ms]
 * @param[in] interval_ms Time between measurements [ms], well below `expiry_ms`
 * Internal:
 * @param _raw Last conversion, 15 bits
 * @param _raw_float Averaged conversion, `NaN` when expired
 * @param _buff Transfer buffer
 * @param _expiry_tick Deadline of the value
 * @param _interval_tick Deadline of the next measurement
 */
typedef struct {
  const char *name;
  SPI_Master_t *spi;
  GPIO_t *cs;
  GPIO_t *ready;
  float nominal_ohms;
  float reference_ohms;
  RTD_Wire_t wire;
  RTD_Reject_t reject;
  uint16_t oversampling;
  uint16_t expiry_ms;
  uint16_t interval_ms;
  // internal
  uint16_t _raw;
  float _raw_float;
  uint8_t _buff[3];
  uint64_t _expiry_tick;
  uint64_t _interval_tick;
} MAX31865_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Configure the pins and the defaults, the SPI bus is initialized by the caller.
 * @param[in,out] rtd RTD front end
 */
void MAX31865_Init(MAX31865_t *rtd);

/**
 * @brief Measure when the interval is due: configure, convert, average.
 * @param[in,out] rtd RTD front end
 * @return `OK` when done or not due, `BUSY` while the bus is taken, `ERR` on a timeout
 */
status_t MAX31865_Loop(MAX31865_t *rtd);

// Sensor resistance [ohm] and temperature [deg C], `NaN` without a valid measurement
float RTD_Resistance_Ohm(const MAX31865_t *rtd);
float RTD_Temperature_C(const MAX31865_t *rtd);

//-------------------------------------------------------------------------------------------------
#endif

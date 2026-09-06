// dvr/acc/ism330.h

#ifndef ISM330_H_
#define ISM330_H_

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "i2c_master.h"
#include "mbb.h"
#include "spi_master.h"
#include "tim.h"
#include "xdef.h"

//--------------------------------------------------------------------------------------- Constants

#define ISM330_ADDRESS 0x6A     // `SDO` low, the factory default
#define ISM330_ADDRESS_ALT 0x6B // `SDO` high
#define ISM330_WHO_AM_I 0x6B    // Identity of ISM330DHCX and ISM330DLC

// `CTRL3_C`: both bytes of a value from one sample, address advancing within a burst
#define ISM330_CTRL3_BDU (1u << 6)
#define ISM330_CTRL3_IF_INC (1u << 2)
// `CTRL9_XL`: data-enable pins as shipped, the configuration bit the datasheet asks for
#define ISM330_CTRL9_DEN 0xE0u
#define ISM330_CTRL9_DEVICE_CONF (1u << 1)
// `CTRL1_OIS`: Mode 4, the auxiliary SPI serving the samples of the primary chain
#define ISM330_OIS_MODE4_EN (1u << 4)
#define ISM330_OIS_EN_SPI2 (1u << 0)

//------------------------------------------------------------------------------------------- Types

typedef enum {
  ISM330_Reg_FUNC_CFG_ACCESS = 0x01,
  ISM330_Reg_PIN_CTRL = 0x02,
  ISM330_Reg_FIFO_CTRL1 = 0x07,
  ISM330_Reg_FIFO_CTRL2 = 0x08,
  ISM330_Reg_FIFO_CTRL3 = 0x09,
  ISM330_Reg_FIFO_CTRL4 = 0x0A,
  ISM330_Reg_COUNTER_BDR_REG1 = 0x0B,
  ISM330_Reg_COUNTER_BDR_REG2 = 0x0C,
  ISM330_Reg_INT1_CTRL = 0x0D,
  ISM330_Reg_INT2_CTRL = 0x0E,
  ISM330_Reg_WHO_AM_I = 0x0F,
  ISM330_Reg_CTRL1_XL = 0x10,
  ISM330_Reg_CTRL2_G = 0x11,
  ISM330_Reg_CTRL3_C = 0x12,
  ISM330_Reg_CTRL4_C = 0x13,
  ISM330_Reg_CTRL5_C = 0x14,
  ISM330_Reg_CTRL6_C = 0x15,
  ISM330_Reg_CTRL7_G = 0x16,
  ISM330_Reg_CTRL8_XL = 0x17,
  ISM330_Reg_CTRL9_XL = 0x18,
  ISM330_Reg_CTRL10_C = 0x19,
  ISM330_Reg_ALL_INT_SRC = 0x1A,
  ISM330_Reg_WAKE_UP_SRC = 0x1B,
  ISM330_Reg_TAP_SRC = 0x1C,
  ISM330_Reg_D6D_SRC = 0x1D,
  ISM330_Reg_STATUS_REG = 0x1E,
  ISM330_Reg_OUT_TEMP_L = 0x20,
  ISM330_Reg_OUT_TEMP_H = 0x21,
  ISM330_Reg_OUTX_L_G = 0x22,
  ISM330_Reg_OUTX_H_G = 0x23,
  ISM330_Reg_OUTY_L_G = 0x24,
  ISM330_Reg_OUTY_H_G = 0x25,
  ISM330_Reg_OUTZ_L_G = 0x26,
  ISM330_Reg_OUTZ_H_G = 0x27,
  ISM330_Reg_OUTX_L_A = 0x28,
  ISM330_Reg_OUTX_H_A = 0x29,
  ISM330_Reg_OUTY_L_A = 0x2A,
  ISM330_Reg_OUTY_H_A = 0x2B,
  ISM330_Reg_OUTZ_L_A = 0x2C,
  ISM330_Reg_OUTZ_H_A = 0x2D,
  ISM330_Reg_CTRL1_OIS = 0x70
} ISM330_Reg_t;

// `INT1_CTRL`: events routed to `INT1`
typedef enum {
  ISM330_Int_DenDrdy = 0x80,
  ISM330_Int_Counter = 0x40,
  ISM330_Int_FifoFull = 0x20,
  ISM330_Int_FifoOverrun = 0x10,
  ISM330_Int_FifoThreshold = 0x08,
  ISM330_Int_Boot = 0x04,
  ISM330_Int_Gyro = 0x02,
  ISM330_Int_Acc = 0x01
} ISM330_Int_t;

// `STATUS_REG`: a new sample waiting in each sensor
typedef enum {
  ISM330_DataReady_Acc = 0b001,
  ISM330_DataReady_Gyro = 0b010,
  ISM330_DataReady_Temp = 0b100,
  ISM330_DataReady_All = 0b111
} ISM330_DataReady_t;

// Output data rate, one code for both sensors
typedef enum {
  ISM330_Speed_PowerDown = 0b0000,
  ISM330_Speed_12Hz5 = 0b0001,
  ISM330_Speed_26Hz = 0b0010,
  ISM330_Speed_52Hz = 0b0011,
  ISM330_Speed_104Hz = 0b0100,
  ISM330_Speed_208Hz = 0b0101,
  ISM330_Speed_416Hz = 0b0110,
  ISM330_Speed_833Hz = 0b0111,
  ISM330_Speed_1666Hz = 0b1000,
  ISM330_Speed_3333Hz = 0b1001,
  ISM330_Speed_6666Hz = 0b1010,
  ISM330_Speed_1Hz6 = 0b1011 // accelerometer in low power only
} ISM330_Speed_t;

typedef enum {
  ISM330_AccScale_2g = 0b00,
  ISM330_AccScale_16g = 0b01,
  ISM330_AccScale_4g = 0b10,
  ISM330_AccScale_8g = 0b11
} ISM330_AccScale_t;

typedef enum {
  ISM330_GyroScale_250dps = 0b0000,
  ISM330_GyroScale_4000dps = 0b0001,
  ISM330_GyroScale_125dps = 0b0010,
  ISM330_GyroScale_500dps = 0b0100,
  ISM330_GyroScale_1000dps = 0b1000,
  ISM330_GyroScale_2000dps = 0b1100
} ISM330_GyroScale_t;

// Accelerometer low-pass bandwidth as a fraction of the data rate
typedef enum {
  ISM330_AccLpf_Over4 = 0b000,
  ISM330_AccLpf_Over10 = 0b001,
  ISM330_AccLpf_Over20 = 0b010,
  ISM330_AccLpf_Over45 = 0b011,
  ISM330_AccLpf_Over100 = 0b100,
  ISM330_AccLpf_Over200 = 0b101,
  ISM330_AccLpf_Over400 = 0b110,
  ISM330_AccLpf_Over800 = 0b111
} ISM330_AccLpf_t;

// What a capture records
typedef enum {
  ISM330_Mode_Acc,
  ISM330_Mode_Gyro,
  ISM330_Mode_All
} ISM330_Mode_t;

// One sample of a three-axis sensor [LSB], the record of `ISM330_Mode_Acc` and `ISM330_Mode_Gyro`
typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} ISM330_Axes_t;

// One sample of every sensor [LSB], the record of `ISM330_Mode_All`
typedef struct {
  ISM330_Axes_t acc;
  ISM330_Axes_t gyro;
  int16_t temp;
} ISM330_Sample_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief IMU on I2C, ISM330DHCX or ISM330DLC: accelerometer, gyroscope, thermometer.
 *   The configuration goes over I2C. An SPI on the auxiliary interface, Mode 4 of the
 *   datasheet, serves the samples at speed, mode 3 up to 10MHz; without it they come
 *   over I2C. A capture fills `mbb` with records at the data rate, `tim` or `INT1`
 *   pacing every sample.
 * @param[in] i2c I2C bus, the primary interface
 * @param[in] address I2C address, `0` = `ISM330_ADDRESS`
 * @param[in] spi Auxiliary SPI serving the samples, `NULL` = samples over I2C
 * @param[in] cs Chip select on SPI, `NULL` = the one configured on the bus
 * @param[in] int1 Data-ready output `INT1`, a sample waits for it `true`; `NULL` = no wait
 * @param[in] tim Timer with the sample period, run by a capture; `NULL` = none
 * @param[in] mbb Records of a capture, its `struct_size` follows `mode`; `NULL` = no capture
 * @param[in] mode What a capture records
 * @param[in] low_power Low-power mode of both sensors instead of high performance
 * @param[in] acc_speed Accelerometer data rate
 * @param[in] acc_scale Accelerometer full scale
 * @param[in] acc_lpf Accelerometer low-pass filter on
 * @param[in] acc_lpf_bw Bandwidth of that filter
 * @param[in] gyro_speed Gyroscope data rate
 * @param[in] gyro_scale Gyroscope full scale
 * @param[in] gyro_lpf Gyroscope low-pass filter on
 * @param[in] gyro_lpf_bw Bandwidth of that filter, `0` to `7`, `CTRL6_C` in the datasheet
 * @param[out] sample Last sample [LSB], `ISM330_Acceleration_g`, `ISM330_Rate_dps`
 *   and `ISM330_Temperature_C` scale its fields
 * Internal:
 * @param _buff Transfer buffer, the register address ahead of the data
 */
typedef struct {
  I2C_Master_t *i2c;
  uint8_t address;
  SPI_Master_t *spi;
  GPIO_t *cs;
  GPIO_t *int1;
  TIM_t *tim;
  MBB_t *mbb;
  ISM330_Mode_t mode;
  bool low_power;
  ISM330_Speed_t acc_speed;
  ISM330_AccScale_t acc_scale;
  bool acc_lpf;
  ISM330_AccLpf_t acc_lpf_bw;
  ISM330_Speed_t gyro_speed;
  ISM330_GyroScale_t gyro_scale;
  bool gyro_lpf;
  uint8_t gyro_lpf_bw;
  ISM330_Sample_t sample;
  // internal
  uint8_t _buff[16];
} ISM330_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Configure the pins and the pacing timer, the buses are initialized by the caller.
 * @param[in,out] ism Sensor
 */
void ISM330_Init(ISM330_t *ism);

/**
 * @brief Check the identity, write the configuration over I2C, unlock the auxiliary SPI.
 * @param[in,out] ism Sensor
 * @return `OK` when the sensor answers with its identity on every bus and takes the registers
 */
status_t ISM330_Config(ISM330_t *ism);

/**
 * @brief One sample of every sensor into `sample`, after `INT1` when the pin is wired.
 * @param[in,out] ism Sensor
 * @return `OK`, `ERR` on a timeout
 */
status_t ISM330_Read(ISM330_t *ism);

/**
 * @brief Fill `mbb` with records of `mode` at the data rate, the first sample dropped
 *   as one left from before. Four bytes stay free behind the records for a checksum.
 * @param[in,out] ism Sensor
 * @return `OK` with the buffer full, `ERR` when a sample did not come or the buffer is locked
 */
status_t ISM330_Capture(ISM330_t *ism);

/**
 * @brief Print a record of `ISM330_Mode_Acc` or `ISM330_Mode_Gyro`, the `StructPrint` of `mbb`.
 * @param[in] record `ISM330_Axes_t`
 * @return Bytes printed
 */
int32_t ISM330_PrintAxes(void *record);

/**
 * @brief Print a record of `ISM330_Mode_All`, the `StructPrint` of `mbb`.
 * @param[in] record `ISM330_Sample_t`
 * @return Bytes printed
 */
int32_t ISM330_PrintSample(void *record);

//----------------------------------------------------------------------------------------- Convert

/**
 * @brief Acceleration from a raw axis, by the configured scale.
 * @param[in] ism Sensor
 * @param[in] raw Axis value from `sample.acc`
 * @return Acceleration [g]
 */
float ISM330_Acceleration_g(const ISM330_t *ism, int16_t raw);

/**
 * @brief Angular rate from a raw axis, by the configured scale.
 * @param[in] ism Sensor
 * @param[in] raw Axis value from `sample.gyro`
 * @return Angular rate [dps]
 */
float ISM330_Rate_dps(const ISM330_t *ism, int16_t raw);

/**
 * @brief Temperature from the raw thermometer value, `256` LSB per degree around `25`.
 * @param[in] raw Value from `sample.temp`
 * @return Temperature [°C]
 */
static inline float ISM330_Temperature_C(int16_t raw) { return 25.0f + (float)raw / 256; }

//-------------------------------------------------------------------------------------------------
#endif

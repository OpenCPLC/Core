// dvr/acc/ism330.c

#include "ism330.h"

#include "dbg.h"
#include "vrts.h"

//---------------------------------------------------------------------------------------- Internal

static uint8_t address_of(const ISM330_t *ism) {
  return ism->address ? ism->address : ISM330_ADDRESS;
}

static void chip_select(ISM330_t *ism)
{
  if(ism->cs) ism->spi->cs = ism->cs;
}

// Registers from `reg` on into `_buff[1..len]`
static status_t i2c_read(ISM330_t *ism, uint8_t reg, uint8_t len)
{
  if(I2C_Master_ReadReg(ism->i2c, address_of(ism), reg, ism->_buff + 1, len)) return ERR;
  if(timeout(20 + len, WAIT_&I2C_Master_IsFree, ism->i2c)) return ERR;
  return I2C_Master_Nack(ism->i2c) ? ERR : OK;
}

// Registers from `reg` on out of `_buff[1..len]`
static status_t i2c_write(ISM330_t *ism, uint8_t reg, uint8_t len)
{
  if(I2C_Master_WriteReg(ism->i2c, address_of(ism), reg, ism->_buff + 1, len)) return ERR;
  if(timeout(20 + len, WAIT_&I2C_Master_IsFree, ism->i2c)) return ERR;
  return I2C_Master_Nack(ism->i2c) ? ERR : OK;
}

// The address byte clocks a byte in as well, so the data lands in `_buff[1..len]` here too.
static status_t spi_read(ISM330_t *ism, uint8_t reg, uint8_t len)
{
  chip_select(ism);
  if(SPI_Master_Read(ism->spi, reg | 0x80, ism->_buff, len + 1)) return ERR;
  return timeout(20 + len, WAIT_&SPI_Master_IsFree, ism->spi) ? ERR : OK;
}

static status_t spi_write(ISM330_t *ism, uint8_t reg, uint8_t len)
{
  chip_select(ism);
  ism->_buff[0] = reg;
  if(SPI_Master_Write(ism->spi, ism->_buff, len + 1)) return ERR;
  return timeout(20 + len, WAIT_&SPI_Master_IsFree, ism->spi) ? ERR : OK;
}

// Samples come over the auxiliary SPI when there is one.
static status_t read_data(ISM330_t *ism, uint8_t reg, uint8_t len) {
  return ism->spi ? spi_read(ism, reg, len) : i2c_read(ism, reg, len);
}

// The bus of the samples free for a transfer, `false` when another one holds it too long
static bool bus_free(ISM330_t *ism)
{
  if(ism->spi) return !timeout(100, WAIT_&SPI_Master_IsFree, ism->spi);
  return !timeout(100, WAIT_&I2C_Master_IsFree, ism->i2c);
}

// Little-endian value whose low byte sits at `_buff[low]`
static int16_t word(const ISM330_t *ism, uint8_t low) {
  return (int16_t)(((uint16_t)ism->_buff[low + 1] << 8) | ism->_buff[low]);
}

// Three axes out of `_buff` from `low` on
static void take_axes(const ISM330_t *ism, uint8_t low, ISM330_Axes_t *axes)
{
  axes->x = word(ism, low);
  axes->y = word(ism, low + 2);
  axes->z = word(ism, low + 4);
}

// The sensors of `mode` into `sample`: temperature, gyroscope and accelerometer
// sit back to back from `OUT_TEMP_L`.
static status_t read_sample(ISM330_t *ism, ISM330_Mode_t mode)
{
  switch(mode) {
    case ISM330_Mode_Acc:
      if(read_data(ism, ISM330_Reg_OUTX_L_A, 6)) return ERR;
      take_axes(ism, 1, &ism->sample.acc);
      return OK;
    case ISM330_Mode_Gyro:
      if(read_data(ism, ISM330_Reg_OUTX_L_G, 6)) return ERR;
      take_axes(ism, 1, &ism->sample.gyro);
      return OK;
    default:
      if(read_data(ism, ISM330_Reg_OUT_TEMP_L, 14)) return ERR;
      ism->sample.temp = word(ism, 1);
      take_axes(ism, 3, &ism->sample.gyro);
      take_axes(ism, 9, &ism->sample.acc);
      return OK;
  }
}

// The record of `mode` inside `sample` and its size
static const uint8_t *record_of(const ISM330_t *ism, uint16_t *size)
{
  switch(ism->mode) {
    case ISM330_Mode_Acc:
      *size = sizeof(ism->sample.acc);
      return (const uint8_t *)&ism->sample.acc;
    case ISM330_Mode_Gyro:
      *size = sizeof(ism->sample.gyro);
      return (const uint8_t *)&ism->sample.gyro;
    default:
      *size = sizeof(ism->sample);
      return (const uint8_t *)&ism->sample;
  }
}

// The pacers of a capture, the timer period and the data-ready pin, `false` on a timeout
static bool wait_sample(ISM330_t *ism)
{
  if(ism->tim && timeout(100, WAIT_&TIM_Event, ism->tim)) return false;
  if(ism->int1 && timeout(100, WAIT_&GPIO_In, ism->int1)) return false;
  return true;
}

//--------------------------------------------------------------------------------------------- API

void ISM330_Init(ISM330_t *ism)
{
  if(ism->cs) {
    ism->cs->mode = GPIO_Mode_Output;
    GPIO_Init(ism->cs);
  }
  if(ism->int1) {
    ism->int1->mode = GPIO_Mode_Input;
    GPIO_Init(ism->int1);
  }
  if(ism->tim) {
    // Stopped until a capture, counting update events for `TIM_Event`
    ism->tim->enable = false;
    ism->tim->enable_interrupt = true;
    TIM_Init(ism->tim);
  }
}

status_t ISM330_Config(ISM330_t *ism)
{
  if(!ism->i2c) return ERR;
  if(timeout(100, WAIT_&I2C_Master_IsFree, ism->i2c)) return ERR;
  if(i2c_read(ism, ISM330_Reg_WHO_AM_I, 1) || ism->_buff[1] != ISM330_WHO_AM_I) return ERR;
  uint8_t *ctrl = ism->_buff + 1;
  if(ism->int1) {
    // Data-ready of either sensor, so the faster one paces when the rates differ.
    ctrl[0] = ISM330_Int_Acc | ISM330_Int_Gyro;
    if(i2c_write(ism, ISM330_Reg_INT1_CTRL, 1)) return ERR;
  }
  // `CTRL1_XL` through `CTRL9_XL` in one burst, the address advancing on its own
  ctrl[0] = (ism->acc_speed << 4) | (ism->acc_scale << 2) | (ism->acc_lpf << 1);
  ctrl[1] = (ism->gyro_speed << 4) | ism->gyro_scale;
  ctrl[2] = ISM330_CTRL3_BDU | ISM330_CTRL3_IF_INC;
  ctrl[3] = ism->gyro_lpf << 1;
  ctrl[4] = 0;
  ctrl[5] = (ism->low_power << 4) | (ism->gyro_lpf_bw & 0x07);
  ctrl[6] = ism->low_power << 7;
  ctrl[7] = ism->acc_lpf_bw << 5;
  ctrl[8] = ISM330_CTRL9_DEN | ISM330_CTRL9_DEVICE_CONF;
  if(i2c_write(ism, ISM330_Reg_CTRL1_XL, 9)) return ERR;
  if(!ism->spi) return OK;
  // Mode 4: the auxiliary SPI takes only its own registers written, then serves
  // the samples of the primary chain. Its identity read proves the wiring.
  if(timeout(100, WAIT_&SPI_Master_IsFree, ism->spi)) return ERR;
  ctrl[0] = ISM330_OIS_MODE4_EN | ISM330_OIS_EN_SPI2;
  if(spi_write(ism, ISM330_Reg_CTRL1_OIS, 1)) return ERR;
  if(spi_read(ism, ISM330_Reg_WHO_AM_I, 1) || ism->_buff[1] != ISM330_WHO_AM_I) return ERR;
  return OK;
}

status_t ISM330_Read(ISM330_t *ism)
{
  if(!bus_free(ism)) return ERR;
  if(ism->int1 && timeout(100, WAIT_&GPIO_In, ism->int1)) return ERR;
  return read_sample(ism, ISM330_Mode_All);
}

status_t ISM330_Capture(ISM330_t *ism)
{
  if(!ism->mbb || !bus_free(ism)) return ERR;
  uint16_t size;
  const uint8_t *record = record_of(ism, &size);
  ism->mbb->struct_size = size;
  if(MBB_Clear(ism->mbb)) return ERR;
  if(ism->tim) {
    TIM_ResetValue(ism->tim);
    TIM_Enable(ism->tim);
  }
  bool first = true;
  status_t status = OK;
  while(MBB_StructFree(ism->mbb, sizeof(uint32_t))) {
    if(!wait_sample(ism) || read_sample(ism, ism->mode)) {
      status = ERR;
      break;
    }
    if(first) {
      first = false;
      continue;
    }
    if(MBB_StructAdd(ism->mbb, record) <= 0) {
      status = ERR;
      break;
    }
  }
  if(ism->tim) TIM_Disable(ism->tim);
  return status;
}

int32_t ISM330_PrintAxes(void *record)
{
  const ISM330_Axes_t *axes = record;
  int32_t n = 0;
  n += DBG_String("x:");
  n += DBG_Dec(axes->x);
  n += DBG_String(" y:");
  n += DBG_Dec(axes->y);
  n += DBG_String(" z:");
  n += DBG_Dec(axes->z);
  return n;
}

int32_t ISM330_PrintSample(void *record)
{
  const ISM330_Sample_t *sample = record;
  int32_t n = 0;
  n += DBG_String("ax:");
  n += DBG_Dec(sample->acc.x);
  n += DBG_String(" ay:");
  n += DBG_Dec(sample->acc.y);
  n += DBG_String(" az:");
  n += DBG_Dec(sample->acc.z);
  n += DBG_String(" gx:");
  n += DBG_Dec(sample->gyro.x);
  n += DBG_String(" gy:");
  n += DBG_Dec(sample->gyro.y);
  n += DBG_String(" gz:");
  n += DBG_Dec(sample->gyro.z);
  n += DBG_String(" temp:");
  n += DBG_Float(ISM330_Temperature_C(sample->temp), 2);
  return n;
}

//----------------------------------------------------------------------------------------- Convert

float ISM330_Acceleration_g(const ISM330_t *ism, int16_t raw)
{
  // Sensitivity [g/LSB] in the order of the `FS_XL` codes: 2g, 16g, 4g, 8g
  static const float scale[] = { 0.061e-3f, 0.488e-3f, 0.122e-3f, 0.244e-3f };
  return raw * scale[ism->acc_scale & 3];
}

float ISM330_Rate_dps(const ISM330_t *ism, int16_t raw)
{
  switch(ism->gyro_scale) {
    case ISM330_GyroScale_125dps: return raw * 4.375e-3f;
    case ISM330_GyroScale_250dps: return raw * 8.75e-3f;
    case ISM330_GyroScale_500dps: return raw * 17.5e-3f;
    case ISM330_GyroScale_1000dps: return raw * 35e-3f;
    case ISM330_GyroScale_2000dps: return raw * 70e-3f;
    default: return raw * 140e-3f;
  }
}

//-------------------------------------------------------------------------------------------------

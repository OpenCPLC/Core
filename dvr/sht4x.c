// dvr/sht4x.c

#include "sht4x.h"
#include "crc.h"
#include "vrts.h"

//--------------------------------------------------------------------------------------- Internals

static const CRC_t sht4x_crc = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0xFF,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

static uint8_t SHT4X_Address(SHT4X_t *sht4x)
{
  return sht4x->address ? sht4x->address : SHT4X_ADDRESS;
}

// Conversion time of a measurement command, rounded up to whole milliseconds
static uint16_t SHT4X_Delay_ms(uint8_t mode)
{
  switch(mode) {
    case SHT4X_Mode_Low: return 2;
    case SHT4X_Mode_Medium: return 5;
    default: return 10;
  }
}

static bool SHT4X_BusWrite(SHT4X_t *sht4x, uint8_t *data, uint16_t len)
{
  if(I2C_Master_Write(sht4x->i2c, SHT4X_Address(sht4x), data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, sht4x->i2c)) return false;
  return !I2C_Master_Nack(sht4x->i2c);
}

static bool SHT4X_BusRead(SHT4X_t *sht4x, uint8_t *data, uint16_t len)
{
  // A refused start leaves `_busy` untouched, and the wait below would read it as ours
  if(I2C_Master_Read(sht4x->i2c, SHT4X_Address(sht4x), data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, sht4x->i2c)) return false;
  return !I2C_Master_Nack(sht4x->i2c);
}

// Response holds temperature first, then humidity, each with its own CRC byte
static bool SHT4X_CheckAppend(SHT4X_t *sht4x)
{
  if((uint8_t)CRC_Run(&sht4x_crc, &sht4x->_buff_rx[0], 2) != sht4x->_buff_rx[2]) return false;
  if((uint8_t)CRC_Run(&sht4x_crc, &sht4x->_buff_rx[3], 2) != sht4x->_buff_rx[5]) return false;
  sht4x->_raw.temperature += ((int32_t)sht4x->_buff_rx[0] << 8 | sht4x->_buff_rx[1]);
  sht4x->_raw.humidity += ((int32_t)sht4x->_buff_rx[3] << 8 | sht4x->_buff_rx[4]);
  return true;
}

static void SHT4X_Calculate(SHT4X_t *sht4x)
{
  float ticks_t = (float)sht4x->_raw.temperature / sht4x->oversampling;
  float ticks_rh = (float)sht4x->_raw.humidity / sht4x->oversampling;
  sht4x->temperature = (175.0 * ticks_t / 65535) - 45.0;
  // Raw humidity reaches slightly past the physical range, so the result is clipped
  float humidity = (125.0 * ticks_rh / 65535) - 6.0;
  if(humidity < 0) humidity = 0;
  if(humidity > 100) humidity = 100;
  sht4x->humidity = humidity;
}

//--------------------------------------------------------------------------------------------- API

bool SHT4X_Command(SHT4X_t *sht4x, SHT4X_CMD_t cmd)
{
  if(!sht4x->i2c) return false;
  uint8_t value = (uint8_t)cmd;
  return SHT4X_BusWrite(sht4x, &value, 1);
}

status_t SHT4X_Loop(SHT4X_t *sht4x)
{
  if(!sht4x->i2c) return ERR;
  if(tick_away(&sht4x->_interval_tick)) return OK;
  if(tick_over(&sht4x->_expiry_tick)) {
    sht4x->humidity = NaN;
    sht4x->temperature = NaN;
  }
  if(I2C_Master_IsBusy(sht4x->i2c)) return BUSY;
  sht4x->_raw.humidity = 0;
  sht4x->_raw.temperature = 0;
  if(!sht4x->mode) sht4x->mode = SHT4X_Mode_High;
  if(!sht4x->oversampling) sht4x->oversampling = 1;
  for(uint16_t i = 0; i < sht4x->oversampling; i++) {
    uint8_t cmd = sht4x->mode;
    if(!SHT4X_BusWrite(sht4x, &cmd, 1)) return ERR;
    delay(SHT4X_Delay_ms(sht4x->mode));
    if(!SHT4X_BusRead(sht4x, sht4x->_buff_rx, 6)) return ERR;
    if(!SHT4X_CheckAppend(sht4x)) return ERR;
  }
  SHT4X_Calculate(sht4x);
  sht4x->_expiry_tick = tick_keep(sht4x->expiry_ms);
  sht4x->_interval_tick = tick_keep(sht4x->interval_ms);
  return OK;
}

float SHT4X_Temperature_C(SHT4X_t *sht4x)
{
  return sht4x->temperature;
}

float SHT4X_Humidity_RH(SHT4X_t *sht4x)
{
  return sht4x->humidity;
}

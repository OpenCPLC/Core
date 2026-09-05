// dvr/shtc3.c

#include "shtc3.h"
#include "crc.h"
#include "vrts.h"

//--------------------------------------------------------------------------------------- Internals

const CRC_t shtc3_crc = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0xFF,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

static uint8_t SHTC3_Address(SHTC3_t *shtc3)
{
  return shtc3->address ? shtc3->address : SHTC3_ADDRESS;
}

static bool SHTC3_BusWrite(SHTC3_t *shtc3, uint8_t *data, uint16_t len)
{
  if(I2C_Master_Write(shtc3->i2c, SHTC3_Address(shtc3), data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, shtc3->i2c)) return false;
  return !I2C_Master_Nack(shtc3->i2c);
}

static bool SHTC3_BusRead(SHTC3_t *shtc3, uint8_t *data, uint16_t len)
{
  // A refused start leaves `_busy` untouched, and the wait below would read it as ours
  if(I2C_Master_Read(shtc3->i2c, SHTC3_Address(shtc3), data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, shtc3->i2c)) return false;
  return !I2C_Master_Nack(shtc3->i2c);
}

static bool SHTC3_SendCommand(SHTC3_t *shtc3, SHTC3_CMD_t cmd)
{
  shtc3->_buff_tx[0] = (uint8_t)(cmd >> 8);
  shtc3->_buff_tx[1] = (uint8_t)(cmd);
  return SHTC3_BusWrite(shtc3, shtc3->_buff_tx, 2);
}

static bool SHTC3_WakeUp(SHTC3_t *shtc3)
{
  bool ok = SHTC3_SendCommand(shtc3, SHTC3_CMD_WakuUp);
  if(!ok) return false;
  delay(10);
  return true;
}

static bool SHTC3_SetMode(SHTC3_t *shtc3)
{
  return SHTC3_SendCommand(shtc3, shtc3->mode);
}

static bool SHTC3_Read(SHTC3_t *shtc3)
{
  return SHTC3_BusRead(shtc3, shtc3->_buff_rx, 6);
}

static bool SHTC3_Sleep(SHTC3_t *shtc3)
{
  bool ok = SHTC3_SendCommand(shtc3, SHTC3_CMD_Sleep);
  if(!ok) return false;
  delay(10);
  return true;
}

static bool SHTC3_CheckAppend(SHTC3_t *shtc3)
{
  if((uint8_t)CRC_Run(&shtc3_crc, &shtc3->_buff_rx[0], 2) != shtc3->_buff_rx[2]) return false;
  if((uint8_t)CRC_Run(&shtc3_crc, &shtc3->_buff_rx[3], 2) != shtc3->_buff_rx[5]) return false;
  shtc3->_raw.humidity += ((int32_t)shtc3->_buff_rx[3] << 8 | shtc3->_buff_rx[4]);
  shtc3->_raw.temperature += ((int32_t)shtc3->_buff_rx[0] << 8 | shtc3->_buff_rx[1]);
  return true;
}

static void SHTC3_Calculate(SHTC3_t *shtc3)
{
  shtc3->humidity = 100.0 * (float)shtc3->_raw.humidity / shtc3->oversampling / 65536;
  shtc3->temperature =
    (175.0 * (float)shtc3->_raw.temperature / shtc3->oversampling / 65536) - 45.0;
}

//--------------------------------------------------------------------------------------------- API

status_t SHTC3_Loop(SHTC3_t *shtc3)
{
  if(!shtc3->i2c) return ERR;
  if(tick_away(&shtc3->_interval_tick)) return OK;
  if(tick_over(&shtc3->_expiry_tick)) {
    shtc3->humidity = NaN;
    shtc3->temperature = NaN;
  }
  if(I2C_Master_IsBusy(shtc3->i2c)) return BUSY;
  shtc3->_raw.humidity = 0;
  shtc3->_raw.temperature = 0;
  if(!SHTC3_WakeUp(shtc3)) return ERR;
  if(!SHTC3_SetMode(shtc3)) return ERR;
  if(!shtc3->oversampling) shtc3->oversampling = 1;
  for(uint16_t i = 0; i < shtc3->oversampling; i++) {
    if(!SHTC3_Read(shtc3)) return ERR;
    if(!SHTC3_CheckAppend(shtc3)) return ERR;
  }
  if(!SHTC3_Sleep(shtc3)) return ERR;
  SHTC3_Calculate(shtc3);
  shtc3->_expiry_tick = tick_keep(shtc3->expiry_ms);
  shtc3->_interval_tick = tick_keep(shtc3->interval_ms);
  return OK;
}

float SHTC3_Temperature_C(SHTC3_t *shtc3)
{
  return shtc3->temperature;
}

float SHTC3_Humidity_RH(SHTC3_t *shtc3)
{
  return shtc3->humidity;
}

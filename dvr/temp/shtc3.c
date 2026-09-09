// dvr/temp/shtc3.c

#include "shtc3.h"

#include "crc.h"
#include "vrts.h"

//---------------------------------------------------------------------------------------- Internal

static const CRC_t shtc3_crc = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0xFF,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

static uint8_t address_of(SHTC3_t *shtc3)
{
  return shtc3->address ? shtc3->address : SHTC3_ADDRESS;
}

static bool bus_write(SHTC3_t *shtc3, uint8_t *data, uint16_t len)
{
  if(I2C_Master_Write(shtc3->i2c, address_of(shtc3), data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, shtc3->i2c)) return false;
  return !I2C_Master_Nack(shtc3->i2c);
}

static bool bus_read(SHTC3_t *shtc3, uint8_t *data, uint16_t len)
{
  // A refused start leaves `_busy` untouched, and the wait below would read it as ours
  if(I2C_Master_Read(shtc3->i2c, address_of(shtc3), data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, shtc3->i2c)) return false;
  return !I2C_Master_Nack(shtc3->i2c);
}

static bool send_command(SHTC3_t *shtc3, SHTC3_CMD_t cmd)
{
  shtc3->_buff_tx[0] = (uint8_t)(cmd >> 8);
  shtc3->_buff_tx[1] = (uint8_t)(cmd);
  return bus_write(shtc3, shtc3->_buff_tx, 2);
}

static bool wake_up(SHTC3_t *shtc3)
{
  bool ok = send_command(shtc3, SHTC3_CMD_WakeUp);
  if(!ok) return false;
  delay(10);
  return true;
}

static bool set_mode(SHTC3_t *shtc3)
{
  return send_command(shtc3, shtc3->mode);
}

static bool read_raw(SHTC3_t *shtc3)
{
  return bus_read(shtc3, shtc3->_buff_rx, 6);
}

static bool go_sleep(SHTC3_t *shtc3)
{
  bool ok = send_command(shtc3, SHTC3_CMD_Sleep);
  if(!ok) return false;
  delay(10);
  return true;
}

static bool check_append(SHTC3_t *shtc3)
{
  if((uint8_t)CRC_Run(&shtc3_crc, &shtc3->_buff_rx[0], 2) != shtc3->_buff_rx[2]) return false;
  if((uint8_t)CRC_Run(&shtc3_crc, &shtc3->_buff_rx[3], 2) != shtc3->_buff_rx[5]) return false;
  shtc3->_raw.humidity += ((int32_t)shtc3->_buff_rx[3] << 8 | shtc3->_buff_rx[4]);
  shtc3->_raw.temperature += ((int32_t)shtc3->_buff_rx[0] << 8 | shtc3->_buff_rx[1]);
  return true;
}

static void calculate(SHTC3_t *shtc3)
{
  shtc3->humidity = 100.0f * (float)shtc3->_raw.humidity / shtc3->oversampling / 65536;
  shtc3->temperature =
    (175.0f * (float)shtc3->_raw.temperature / shtc3->oversampling / 65536) - 45.0f;
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
  if(!wake_up(shtc3)) return ERR;
  if(!set_mode(shtc3)) return ERR;
  if(!shtc3->oversampling) shtc3->oversampling = 1;
  for(uint16_t i = 0; i < shtc3->oversampling; i++) {
    if(!read_raw(shtc3)) return ERR;
    if(!check_append(shtc3)) return ERR;
  }
  if(!go_sleep(shtc3)) return ERR;
  calculate(shtc3);
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

// hal/host/itf/i2c_master.c

#include "i2c_master.h"

#include <string.h>

I2C_TypeDef HostI2c[3];

//---------------------------------------------------------------------------------------- Internal

// Both phases go to the attached device at once, the bus busy meanwhile,
// so the transfer is over on return.
static status_t transfer(I2C_Master_t *i2c, uint8_t addr, const uint8_t *tx_data, uint16_t tx_len,
  uint8_t *rx_data, uint16_t rx_len)
{
  i2c->_busy = true;
  bool ack = i2c->_Device && i2c->_Device(i2c->_ctx, addr, tx_data, tx_len, rx_data, rx_len);
  i2c->_busy = false;
  i2c->_nack = !ack;
  return FREE;
}

//--------------------------------------------------------------------------------------------- API

void I2C_Master_Init(I2C_Master_t *i2c) { i2c->_busy = false; }
void I2C_Master_Disable(I2C_Master_t *i2c) { i2c->_busy = true; }
bool I2C_Master_IsBusy(I2C_Master_t *i2c) { return i2c->_busy; }
bool I2C_Master_IsFree(I2C_Master_t *i2c) { return !i2c->_busy; }
bool I2C_Master_Nack(I2C_Master_t *i2c) { return i2c->_nack; }

status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len > I2C_TRANSFER_MAX) return ERR;
  return transfer(i2c, addr, data, len, NULL, 0);
}

status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len > I2C_TRANSFER_MAX) return ERR;
  return transfer(i2c, addr, NULL, 0, data, len);
}

status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len >= I2C_TRANSFER_MAX) return ERR;
  uint8_t frame[I2C_TRANSFER_MAX];
  frame[0] = reg;
  memcpy(&frame[1], data, len);
  return transfer(i2c, addr, frame, len + 1, NULL, 0);
}

status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len > I2C_TRANSFER_MAX) return ERR;
  return transfer(i2c, addr, &reg, 1, data, len);
}

status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *tx_data,
  uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len)
{
  if(i2c->_busy) return BUSY;
  if(!tx_data || !tx_len || tx_len > I2C_TRANSFER_MAX) return ERR;
  if(!rx_data || !rx_len || rx_len > I2C_TRANSFER_MAX) return ERR;
  return transfer(i2c, addr, tx_data, tx_len, rx_data, rx_len);
}

//-------------------------------------------------------------------------------------------- Host

void I2C_HostAttach(I2C_Master_t *i2c, I2C_HostDevice_t Device, void *ctx)
{
  i2c->_Device = Device;
  i2c->_ctx = ctx;
}

//-------------------------------------------------------------------------------------------------

// plc/com/twi.c

#include "twi.h"

I2C_Master_t *twi_interface;

void TWI_Init(I2C_Master_t *i2c)
{
  twi_interface = i2c;
  I2C_Master_Init(twi_interface);
}

bool TWI_IsBusy(void)
{
  return I2C_Master_IsBusy(twi_interface);
}

bool TWI_IsFree(void)
{
  return I2C_Master_IsFree(twi_interface);
}

bool TWI_Read(uint8_t addr, uint8_t *data, uint16_t len)
{
  // A refused start leaves `_busy` untouched, and the wait below would read it as ours
  if(I2C_Master_Read(twi_interface, addr, data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return !I2C_Master_Nack(twi_interface);
}

bool TWI_Write(uint8_t addr, uint8_t *data, uint16_t len)
{
  if(I2C_Master_Write(twi_interface, addr, data, len)) return false;
  if(timeout(21 + len, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return !I2C_Master_Nack(twi_interface);
}

bool TWI_ReadReg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
  if(I2C_Master_ReadReg(twi_interface, addr, reg, data, len)) return false;
  if(timeout(23 + len, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return !I2C_Master_Nack(twi_interface);
}

bool TWI_WriteReg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
  if(I2C_Master_WriteReg(twi_interface, addr, reg, data, len)) return false;
  if(timeout(22 + len, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return !I2C_Master_Nack(twi_interface);
}

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

bool TWI_Read(uint8_t addr, uint8_t *ary, uint16_t n)
{
  I2C_Master_Read(twi_interface, addr, ary, n);
  if(timeout(21 + n, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return true;
}

bool TWI_Write(uint8_t addr, uint8_t *ary, uint16_t n)
{
  I2C_Master_Write(twi_interface, addr, ary, n);
  if(timeout(21 + n, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return true;
}

bool TWI_ReadReg(uint8_t addr, uint8_t reg, uint8_t *ary, uint16_t n)
{
  I2C_Master_ReadReg(twi_interface, addr, reg, ary, n);
  if(timeout(23 + n, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return true;
}

bool TWI_WriteReg(uint8_t addr, uint8_t reg, uint8_t *ary, uint16_t n)
{
  I2C_Master_WriteReg(twi_interface, addr, reg, ary, n);
  if(timeout(22 + n, WAIT_&I2C_Master_IsFree, twi_interface)) {
    return false;
  }
  return true;
}
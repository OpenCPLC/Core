// plc/com/twi.h

#ifndef TWI_H_
#define TWI_H_

#include "i2c_master.h"
#include "vrts.h"

void TWI_Init(I2C_Master_t *i2c);
bool TWI_IsBusy(void);
bool TWI_IsFree(void);
bool TWI_Read(uint8_t addr, uint8_t *data, uint16_t len);
bool TWI_Write(uint8_t addr, uint8_t *data, uint16_t len);
bool TWI_ReadReg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
bool TWI_WriteReg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

#endif

// hal/stm32wb/i2c_wb.h

#ifndef I2C_WB_H_
#define I2C_WB_H_

//-------------------------------------------------------------------------------------------------

typedef enum {
  I2C_SCL_None = 0,
  I2C1_SCL_PA9,
  I2C1_SCL_PB6,
  I2C1_SCL_PB8,
  I2C3_SCL_PA7,
  I2C3_SCL_PB10,
  I2C3_SCL_PB13
} I2C_SCL_t;

typedef enum {
  I2C_SDA_None = 0,
  I2C1_SDA_PA10,
  I2C1_SDA_PB7,
  I2C1_SDA_PB9,
  I2C3_SDA_PB4,
  I2C3_SDA_PB11,
  I2C3_SDA_PB14
} I2C_SDA_t;

//-------------------------------------------------------------------------------------------------

#endif
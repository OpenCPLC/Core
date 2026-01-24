#ifndef I2C_WB_H_
#define I2C_WB_H_

typedef enum {
  I2C1_SCL_PA9 = 1,
  I2C1_SCL_PB6 = 2,
  I2C1_SCL_PB8 = 3,
  I2C3_SCL_PA11 = 4,
  I2C3_SCL_PB10 = 5,
  I2C3_SCL_PB13 = 6,
} I2C_SCL_e;

typedef enum {
  I2C1_SDA_PA10 = 1,
  I2C1_SDA_PB7 = 2,
  I2C1_SDA_PB9 = 3,
  I2C3_SDA_PA12 = 4,
  I2C3_SDA_PB11 = 5,
  I2C3_SDA_PB14 = 6,
} I2C_SDA_e;

#endif

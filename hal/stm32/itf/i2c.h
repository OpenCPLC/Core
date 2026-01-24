#ifndef I2C_H_
#define I2C_H_

#include "gpio.h"
#include "sys.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#if(SYS_CLOCK_FREQ == 2000000)
  #define I2C_TIMING_100kHz timing = 0x00000509, .filter = 0
#elif(SYS_CLOCK_FREQ == 16000000)
  #define I2C_TIMING_100kHz timing = 0x00503D5A, .filter = 0
  #define I2C_TIMING_400kHz timing = 0x0010061A, .filter = 0
#elif(SYS_CLOCK_FREQ == 48000000)
  #define I2C_TIMING_100kHz timing = 0x00C0D6FF, .filter = 0
  #define I2C_TIMING_400kHz timing = 0x00501855, .filter = 0
  #define I2C_TIMING_1MHz timing = 0x0020091C, .filter = 0
  #define I2C_TIMING_OVERCLOCK timing = 0x00100207, .filter = 0
#elif(SYS_CLOCK_FREQ == 64000000)
  #define I2C_TIMING_100kHz timing = 0x10A13E56, .filter = 0
  #define I2C_TIMING_400kHz timing = 0x00D00E28, .filter = 0
  #define I2C_TIMING_1MHz timing = 0x00700818, .filter = 0
#elif(SYS_CLOCK_FREQ == 18432000)
  #define I2C_TIMING_100kHz timing = 0x10805E89, .filter = 0
  #define I2C_TIMING_400kHz timing = 0x00900B22, .filter = 0
#elif(SYS_CLOCK_FREQ == 59904000)
  #define I2C_TIMING_100kHz timing = 0x109034E7, .filter = 0
  #define I2C_TIMING_400kHz timing = 0x00400D14, .filter = 0
  #define I2C_TIMING_1MHz timing = 0x0030060D, .filter = 0
#endif

//-------------------------------------------------------------------------------------------------

#if defined(STM32G0)
  #include "i2c-g0.h"
#elif defined(STM32WB)
  #include "i2c-wb.h"
#endif

//-------------------------------------------------------------------------------------------------

void I2C_Reset(I2C_TypeDef *i2c_typedef);

extern const GPIO_Map_t i2c_scl_map[];
extern const GPIO_Map_t i2c_sda_map[];

//-------------------------------------------------------------------------------------------------
#endif

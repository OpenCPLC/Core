// hal/stm32/i2c.h

#ifndef I2C_H_
#define I2C_H_

#include "gpio.h"
#include "dma.h"
#include "sys.h"
#include "main.h"

//------------------------------------------------------------------------------------------------- Timing Presets

#if(SYS_CLOCK_FREQ == 2000000)
  #define I2C_TIMING_100kHz  timing = 0x00000509, .filter = 0
#elif(SYS_CLOCK_FREQ == 16000000)
  #define I2C_TIMING_100kHz  timing = 0x00503D5A, .filter = 0
  #define I2C_TIMING_400kHz  timing = 0x0010061A, .filter = 0
#elif(SYS_CLOCK_FREQ == 48000000)
  #define I2C_TIMING_100kHz  timing = 0x00C0D6FF, .filter = 0
  #define I2C_TIMING_400kHz  timing = 0x00501855, .filter = 0
  #define I2C_TIMING_1MHz    timing = 0x0020091C, .filter = 0
#elif(SYS_CLOCK_FREQ == 64000000)
  #define I2C_TIMING_100kHz  timing = 0x10A13E56, .filter = 0
  #define I2C_TIMING_400kHz  timing = 0x00D00E28, .filter = 0
  #define I2C_TIMING_1MHz    timing = 0x00700818, .filter = 0
#elif(SYS_CLOCK_FREQ == 18432000)
  #define I2C_TIMING_100kHz  timing = 0x10805E89, .filter = 0
  #define I2C_TIMING_400kHz  timing = 0x00900B22, .filter = 0
#elif(SYS_CLOCK_FREQ == 59904000)
  #define I2C_TIMING_100kHz  timing = 0x109034E7, .filter = 0
  #define I2C_TIMING_400kHz  timing = 0x00400D14, .filter = 0
  #define I2C_TIMING_1MHz    timing = 0x0030060D, .filter = 0
#endif

//------------------------------------------------------------------------------------------------- Family Include

#if defined(STM32G0)
  #include "i2c_g0.h"
#elif defined(STM32WB)
  #include "i2c_wb.h"
#endif

//------------------------------------------------------------------------------------------------- Pin Maps

extern const GPIO_Map_t I2C_SCL_MAP[];
extern const GPIO_Map_t I2C_SDA_MAP[];

//------------------------------------------------------------------------------------------------- Internal API

void I2C_Reset(I2C_TypeDef *reg);
void I2C_DmaSetTxRequest(I2C_TypeDef *reg, DMA_t *dma);
void I2C_DmaSetRxRequest(I2C_TypeDef *reg, DMA_t *dma);

//-------------------------------------------------------------------------------------------------
#endif
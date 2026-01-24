#ifndef SPI_H_
#define SPI_H_

#include "gpio.h"

//-------------------------------------------------------------------------------------------------

typedef enum {
	SPI_Prescaler_2 = 0,
	SPI_Prescaler_4 = 1,
	SPI_Prescaler_8 = 2,
	SPI_Prescaler_16 = 3,
	SPI_Prescaler_32 = 4,
	SPI_Prescaler_64 = 5,
	SPI_Prescaler_128 = 6,
	SPI_Prescaler_256 = 7,
} SPI_Prescaler_t;

#if defined(STM32G0)
  #include "spi-g0.h"
#elif defined(STM32WB)
  #include "spi-wb.h"
#endif

extern const GPIO_Map_t SPI_SCK_MAP[];
extern const GPIO_Map_t SPI_MISO_MAP[];
extern const GPIO_Map_t SPI_MOSI_MAP[];

//-------------------------------------------------------------------------------------------------
#endif

// hal/stm32wb/spi_wb.h

#ifndef SPI_WB_H_
#define SPI_WB_H_

//-------------------------------------------------------------------------------------------------

typedef enum {
  SPI_SCK_None = 0,
  SPI1_SCK_PA5,
  SPI1_SCK_PB3,
  SPI2_SCK_PB10,
  SPI2_SCK_PB13
} SPI_SCK_t;

typedef enum {
  SPI_MISO_None = 0,
  SPI1_MISO_PA6,
  SPI1_MISO_PB4,
  SPI2_MISO_PB14,
  SPI2_MISO_PC2
} SPI_MISO_t;

typedef enum {
  SPI_MOSI_None = 0,
  SPI1_MOSI_PA7,
  SPI1_MOSI_PB5,
  SPI2_MOSI_PB15,
  SPI2_MOSI_PC3
} SPI_MOSI_t;

//-------------------------------------------------------------------------------------------------

#endif
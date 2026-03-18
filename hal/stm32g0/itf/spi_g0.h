// hal/stm32g0/spi_g0.h

#ifndef SPI_G0_H_
#define SPI_G0_H_

//-------------------------------------------------------------------------------------------------

typedef enum {
  SPI_SCK_None = 0,
  SPI1_SCK_PA1,
  SPI1_SCK_PA5,
  SPI1_SCK_PB3,
  SPI1_SCK_PD8,
  SPI2_SCK_PA0,
  SPI2_SCK_PB8,
  SPI2_SCK_PB10,
  SPI2_SCK_PB13,
  SPI2_SCK_PD1
} SPI_SCK_t;

typedef enum {
  SPI_MISO_None = 0,
  SPI1_MISO_PA6,
  SPI1_MISO_PA11,
  SPI1_MISO_PB4,
  SPI1_MISO_PD5,
  SPI2_MISO_PA3,
  SPI2_MISO_PA9,
  SPI2_MISO_PB2,
  SPI2_MISO_PB6,
  SPI2_MISO_PB14,
  SPI2_MISO_PC2,
  SPI2_MISO_PD3
} SPI_MISO_t;

typedef enum {
  SPI_MOSI_None = 0,
  SPI1_MOSI_PA2,
  SPI1_MOSI_PA7,
  SPI1_MOSI_PA12,
  SPI1_MOSI_PB5,
  SPI1_MOSI_PD6,
  SPI2_MOSI_PA4,
  SPI2_MOSI_PA10,
  SPI2_MOSI_PB7,
  SPI2_MOSI_PB11,
  SPI2_MOSI_PB15,
  SPI2_MOSI_PC3,
  SPI2_MOSI_PD4
} SPI_MOSI_t;

//-------------------------------------------------------------------------------------------------

#endif
// hal/stm32wb/spi_wb.c

#include "spi.h"
#include "dma.h"

//-------------------------------------------------------------------------------------------------

const GPIO_Map_t SPI_SCK_MAP[] = {
  [SPI1_SCK_PA5]  = { .port = GPIOA, .pin = 5,  .alternate = 5 },
  [SPI1_SCK_PB3]  = { .port = GPIOB, .pin = 3,  .alternate = 5 },
  [SPI2_SCK_PB10] = { .port = GPIOB, .pin = 10, .alternate = 5 },
  [SPI2_SCK_PB13] = { .port = GPIOB, .pin = 13, .alternate = 5 }
};

const GPIO_Map_t SPI_MISO_MAP[] = {
  [SPI1_MISO_PA6]  = { .port = GPIOA, .pin = 6,  .alternate = 5 },
  [SPI1_MISO_PB4]  = { .port = GPIOB, .pin = 4,  .alternate = 5 },
  [SPI2_MISO_PB14] = { .port = GPIOB, .pin = 14, .alternate = 5 },
  [SPI2_MISO_PC2]  = { .port = GPIOC, .pin = 2,  .alternate = 5 }
};

const GPIO_Map_t SPI_MOSI_MAP[] = {
  [SPI1_MOSI_PA7]  = { .port = GPIOA, .pin = 7,  .alternate = 5 },
  [SPI1_MOSI_PB5]  = { .port = GPIOB, .pin = 5,  .alternate = 5 },
  [SPI2_MOSI_PB15] = { .port = GPIOB, .pin = 15, .alternate = 5 },
  [SPI2_MOSI_PC3]  = { .port = GPIOC, .pin = 3,  .alternate = 5 }
};

//-------------------------------------------------------------------------------------------------

void SPI_DmaSetRxRequest(SPI_TypeDef *reg, DMA_t *dma)
{
  switch((uint32_t)reg) {
    case (uint32_t)SPI1: dma->mux->CCR |= DMAMUX_REQ_SPI1_RX; break;
    case (uint32_t)SPI2: dma->mux->CCR |= DMAMUX_REQ_SPI2_RX; break;
    default: break;
  }
}

void SPI_DmaSetTxRequest(SPI_TypeDef *reg, DMA_t *dma)
{
  switch((uint32_t)reg) {
    case (uint32_t)SPI1: dma->mux->CCR |= DMAMUX_REQ_SPI1_TX; break;
    case (uint32_t)SPI2: dma->mux->CCR |= DMAMUX_REQ_SPI2_TX; break;
    default: break;
  }
}

//-------------------------------------------------------------------------------------------------
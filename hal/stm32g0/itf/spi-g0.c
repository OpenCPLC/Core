#include "spi.h"
#include "irq.h"

//-------------------------------------------------------------------------------------------------

const GPIO_Map_t SPI_SCK_MAP[] = {
  [SPI1_SCK_PA1] = { .port = GPIOA, .pin = 1, .alternate = 0 },
  [SPI1_SCK_PA5] = { .port = GPIOA, .pin = 5, .alternate = 0 },
  [SPI1_SCK_PB3] = { .port = GPIOB, .pin = 3, .alternate = 0 },
  [SPI1_SCK_PD8] = { .port = GPIOD, .pin = 8, .alternate = 1 },
  [SPI2_SCK_PA0] = { .port = GPIOA, .pin = 0, .alternate = 0 },
  [SPI2_SCK_PB8] = { .port = GPIOB, .pin = 8, .alternate = 1 },
  [SPI2_SCK_PB10] = { .port = GPIOB, .pin = 10, .alternate = 4 },
  [SPI2_SCK_PB13] = { .port = GPIOB, .pin = 13, .alternate = 0 },
  [SPI2_SCK_PD1] = { .port = GPIOD, .pin = 1, .alternate = 1 }
};

const GPIO_Map_t SPI_MISO_MAP[] = {
  [SPI1_MISO_PA6] = { .port = GPIOA, .pin = 6, .alternate = 0 },
  [SPI1_MISO_PA11] = { .port = GPIOA, .pin = 11, .alternate = 0 },
  [SPI1_MISO_PB4] = { .port = GPIOB, .pin = 4, .alternate = 0 },
  [SPI1_MISO_PD5] = { .port = GPIOD, .pin = 5, .alternate = 1 },
  [SPI2_MISO_PA3] = { .port = GPIOA, .pin = 3, .alternate = 0 },
  [SPI2_MISO_PA9] = { .port = GPIOA, .pin = 9, .alternate = 4 },
  [SPI2_MISO_PB2] = { .port = GPIOB, .pin = 2, .alternate = 1 },
  [SPI2_MISO_PB6] = { .port = GPIOB, .pin = 6, .alternate = 4 },
  [SPI2_MISO_PB14] = { .port = GPIOB, .pin = 14, .alternate = 0 },
  [SPI2_MISO_PC2] = { .port = GPIOC, .pin = 2, .alternate = 1 },
  [SPI2_MISO_PD3] = { .port = GPIOD, .pin = 3, .alternate = 1 }
};

const GPIO_Map_t SPI_MOSI_MAP[] = {
  [SPI1_MOSI_PA2] = { .port = GPIOA, .pin = 2, .alternate = 0 },
  [SPI1_MOSI_PA7] = { .port = GPIOA, .pin = 7, .alternate = 0 },
  [SPI1_MOSI_PA12] = { .port = GPIOA, .pin = 12, .alternate = 0 },
  [SPI1_MOSI_PB5] = { .port = GPIOB, .pin = 5, .alternate = 0 },
  [SPI1_MOSI_PD6] = { .port = GPIOD, .pin = 6, .alternate = 1 },
  [SPI2_MOSI_PA4] = { .port = GPIOA, .pin = 4, .alternate = 0 },
  [SPI2_MOSI_PA10] = { .port = GPIOA, .pin = 10, .alternate = 4 },
  [SPI2_MOSI_PB7] = { .port = GPIOB, .pin = 7, .alternate = 1 },
  [SPI2_MOSI_PB11] = { .port = GPIOB, .pin = 11, .alternate = 4 },
  [SPI2_MOSI_PB15] = { .port = GPIOB, .pin = 15, .alternate = 0 },
  [SPI2_MOSI_PC3] = { .port = GPIOC, .pin = 3, .alternate = 1 },
  [SPI2_MOSI_PD4] = { .port = GPIOD, .pin = 4, .alternate = 1 }
};

//-------------------------------------------------------------------------------------------------

void SPI_DmaSetRxRequest(SPI_TypeDef *reg, DMA_t *dma)
{
  (void)dma;
  switch((uint32_t)reg) {
    case (uint32_t)SPI1: dma->mux->CCR |= DMAMUX_REQ_SPI1_RX; break;
    case (uint32_t)SPI2: dma->mux->CCR |= DMAMUX_REQ_SPI2_RX; break;
    default: break;
  }
}

void SPI_DmaSetTxRequest(SPI_TypeDef *reg, DMA_t *dma)
{
  (void)dma;
  switch((uint32_t)reg) {
    case (uint32_t)SPI1: dma->mux->CCR |= DMAMUX_REQ_SPI1_TX; break;
    case (uint32_t)SPI2: dma->mux->CCR |= DMAMUX_REQ_SPI2_TX; break;
    default: break;
  }
}

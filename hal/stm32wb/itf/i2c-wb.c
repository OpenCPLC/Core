#include "i2c.h"
#include "irq.h"

//-------------------------------------------------------------------------------------------------

const GPIO_Map_t i2c_scl_map[] = {
  [I2C1_SCL_PA9] = { .port = GPIOA, .pin = 9, .alternate = 4 },
  [I2C1_SCL_PB6] = { .port = GPIOB, .pin = 6, .alternate = 4 },
  [I2C1_SCL_PB8] = { .port = GPIOB, .pin = 8, .alternate = 4 },
  [I2C3_SCL_PA11] = { .port = GPIOA, .pin = 11, .alternate = 4 },
  [I2C3_SCL_PB10] = { .port = GPIOB, .pin = 10, .alternate = 4 },
  [I2C3_SCL_PB13] = { .port = GPIOB, .pin = 13, .alternate = 4 }
};

const GPIO_Map_t i2c_sda_map[] = {
  [I2C1_SDA_PA10] = { .port = GPIOA, .pin = 10, .alternate = 4 },
  [I2C1_SDA_PB7] = { .port = GPIOB, .pin = 7, .alternate = 4 },
  [I2C1_SDA_PB9] = { .port = GPIOB, .pin = 9, .alternate = 4 },
  [I2C3_SDA_PA12] = { .port = GPIOA, .pin = 12, .alternate = 4 },
  [I2C3_SDA_PB11] = { .port = GPIOB, .pin = 11, .alternate = 4 },
  [I2C3_SDA_PB14] = { .port = GPIOB, .pin = 14, .alternate = 4 }
};

//-------------------------------------------------------------------------------------------------

#define DMAMUX_REQ_I2C1_RX 10
#define DMAMUX_REQ_I2C1_TX 11
#define DMAMUX_REQ_I2C3_RX 12
#define DMAMUX_REQ_I2C3_TX 13

void I2C_Reset(I2C_TypeDef *i2c_typedef)
{
  i2c_typedef->CR1 &= ~I2C_CR1_PE;
  switch((uint32_t)i2c_typedef) {
    case (uint32_t)I2C1:
      RCC->APB1RSTR1 |= RCC_APB1RSTR1_I2C1RST;
      RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C1RST;
      break;
    case (uint32_t)I2C3:
      RCC->APB1RSTR1 |= RCC_APB1RSTR1_I2C3RST;
      RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C3RST;
      break;
    default: break;
  }
}

void I2C_DmaSetTxRequest(I2C_TypeDef *reg, DMA_t *dma)
{
  switch((uint32_t)reg) {
    case (uint32_t)I2C1: dma->mux->CCR |= DMAMUX_REQ_I2C1_TX; break;
    case (uint32_t)I2C3: dma->mux->CCR |= DMAMUX_REQ_I2C3_TX; break;
    default: break;
  }
}

void I2C_DmaSetRxRequest(I2C_TypeDef *reg, DMA_t *dma)
{
  switch((uint32_t)reg) {
    case (uint32_t)I2C1: dma->mux->CCR |= DMAMUX_REQ_I2C1_RX; break;
    case (uint32_t)I2C3: dma->mux->CCR |= DMAMUX_REQ_I2C3_RX; break;
    default: break;
  }
}

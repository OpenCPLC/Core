// hal/stm32wb/i2c_wb.c

#include "i2c.h"

//-------------------------------------------------------------------------------------------------

const GPIO_Map_t I2C_SCL_MAP[] = {
  [I2C1_SCL_PA9]  = { .port = GPIOA, .pin = 9,  .alternate = 4 },
  [I2C1_SCL_PB6]  = { .port = GPIOB, .pin = 6,  .alternate = 4 },
  [I2C1_SCL_PB8]  = { .port = GPIOB, .pin = 8,  .alternate = 4 },
  [I2C3_SCL_PA7]  = { .port = GPIOA, .pin = 7,  .alternate = 4 },
  [I2C3_SCL_PB10] = { .port = GPIOB, .pin = 10, .alternate = 4 },
  [I2C3_SCL_PB13] = { .port = GPIOB, .pin = 13, .alternate = 4 }
};

const GPIO_Map_t I2C_SDA_MAP[] = {
  [I2C1_SDA_PA10] = { .port = GPIOA, .pin = 10, .alternate = 4 },
  [I2C1_SDA_PB7]  = { .port = GPIOB, .pin = 7,  .alternate = 4 },
  [I2C1_SDA_PB9]  = { .port = GPIOB, .pin = 9,  .alternate = 4 },
  [I2C3_SDA_PB4]  = { .port = GPIOB, .pin = 4,  .alternate = 4 },
  [I2C3_SDA_PB11] = { .port = GPIOB, .pin = 11, .alternate = 4 },
  [I2C3_SDA_PB14] = { .port = GPIOB, .pin = 14, .alternate = 4 }
};

//-------------------------------------------------------------------------------------------------

void I2C_Reset(I2C_TypeDef *reg)
{
  reg->CR1 &= ~I2C_CR1_PE;
  switch((uint32_t)reg) {
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

//-------------------------------------------------------------------------------------------------
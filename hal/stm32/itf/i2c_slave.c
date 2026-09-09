// hal/stm32/itf/i2c_slave.c

#include "i2c_slave.h"

//---------------------------------------------------------------------------------------- Handlers

// Sequence mode with a register pointer is not implemented: the interrupt is a no-op
static void sequence_handler(I2C_Slave_t *i2c)
{
  unused(i2c);
}

// Every transaction starts at register `0` and walks up one byte at a time
static void simple_handler(I2C_Slave_t *i2c)
{
  uint32_t isr = i2c->reg->ISR;
  if(isr & I2C_ISR_ADDR) {
    i2c->reg->ICR |= I2C_ICR_ADDRCF;
    if(isr & I2C_ISR_DIR) i2c->reg->CR1 |= I2C_CR1_TXIE | I2C_CR1_STOPIE | I2C_CR1_NACKIE;
    else i2c->reg->CR1 |= I2C_CR1_RXIE | I2C_CR1_STOPIE;
    i2c->_idx = 0;
  }
  else if(isr & I2C_ISR_RXNE) {
    uint8_t data = i2c->reg->RXDR;
    if(i2c->_idx >= i2c->regmap_size) return;
    if(i2c->write_mask[i2c->_idx]) {
      if(i2c->regmap[i2c->_idx] != data) {
        i2c->_updated = true;
        i2c->update_flag[i2c->_idx] = true;
      }
      i2c->regmap[i2c->_idx] = data;
    }
    i2c->_idx++;
  }
  else if(isr & I2C_ISR_TXIS) {
    // Past the map the master reads zeros
    i2c->reg->TXDR = i2c->_idx < i2c->regmap_size ? i2c->regmap[i2c->_idx++] : 0x00;
  }
  else if(isr & I2C_ISR_STOPF) {
    i2c->reg->ICR |= I2C_ICR_STOPCF;
    i2c->reg->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE);
  }
  else if(isr & I2C_ISR_NACKF) {
    i2c->reg->ICR |= I2C_ICR_NACKCF;
    i2c->reg->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE);
  }
}

//--------------------------------------------------------------------------------------------- API

void I2C_Slave_Init(I2C_Slave_t *i2c)
{
  RCC_EnableI2C(i2c->reg);
  IRQ_Handler_t handler = (IRQ_Handler_t)(i2c->sequence ? sequence_handler : simple_handler);
  IRQ_EnableI2C(i2c->reg, i2c->irq_priority, handler, NULL, i2c);
  GPIO_InitAlternate(&I2C_SCL_MAP[i2c->scl], i2c->pull_up);
  GPIO_InitAlternate(&I2C_SDA_MAP[i2c->sda], i2c->pull_up);
  i2c->reg->TIMINGR = i2c->timing;
  i2c->reg->CR1 &= ~I2C_CR1_DNF;
  i2c->reg->OAR1 = (i2c->addr << 1) | I2C_OAR1_OA1EN;
  i2c->reg->CR1 = I2C_CR1_PE | I2C_CR1_ADDRIE | (i2c->filter << I2C_CR1_DNF_Pos);
}

bool I2C_Slave_IsUpdate(I2C_Slave_t *i2c)
{
  if(!i2c->_updated) return false;
  i2c->_updated = false;
  return true;
}

//-------------------------------------------------------------------------------------------------

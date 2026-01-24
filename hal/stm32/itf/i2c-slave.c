#include "i2c-slave.h"

//-------------------------------------------------------------------------------------------------

static void I2C_SlaveSequence_InterruptEV(I2C_Slave_t *i2c)
{
  // implement sequence mode
  (void)i2c;
}

static void I2C_SlaveSimple_InterruptEV(I2C_Slave_t *i2c)
{
  uint32_t isr = i2c->reg->ISR;
  // Address match
  if(isr & I2C_ISR_ADDR) {
    i2c->reg->ICR |= I2C_ICR_ADDRCF;
    if(i2c->reg->ISR & I2C_ISR_DIR) { // Master read, slave write
      i2c->reg->CR1 |= I2C_CR1_TXIE | I2C_CR1_STOPIE | I2C_CR1_NACKIE;
    }
    else { // Master write, slave read
      i2c->reg->CR1 |= I2C_CR1_RXIE | I2C_CR1_STOPIE;
    }
    i2c->i = 0;
  }
  // RX not empty - master writing to slave
  else if(isr & I2C_ISR_RXNE) {
    uint8_t data = i2c->reg->RXDR;
    if(i2c->i >= i2c->regmap_size) return;
    if(i2c->write_mask[i2c->i]) {
      if(i2c->regmap[i2c->i] != data) {
        i2c->update_global_flag = true;
        i2c->update_flag[i2c->i] = true;
      }
      i2c->regmap[i2c->i] = data;
    }
    i2c->i++;
  }
  // TX empty - master reading from slave
  else if(isr & I2C_ISR_TXIS) {
    if(i2c->i < i2c->regmap_size) {
      i2c->reg->TXDR = i2c->regmap[i2c->i];
      i2c->i++;
    }
    else {
      i2c->reg->TXDR = 0x00;
    }
  }
  // Stop condition
  else if(isr & I2C_ISR_STOPF) {
    i2c->reg->ICR |= I2C_ICR_STOPCF;
    i2c->reg->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE);
  }
  // NACK received
  else if(isr & I2C_ISR_NACKF) {
    i2c->reg->ICR |= I2C_ICR_NACKCF;
    i2c->reg->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE);
  }
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Check if slave regmap was updated by master.
 * @param[in,out] i2c Pointer to I2C slave control structure.
 * @return `true` if regmap was updated, `false` otherwise.
 */
bool I2C_Slave_IsUpdate(I2C_Slave_t *i2c)
{
  if(i2c->update_global_flag) {
    i2c->update_global_flag = false;
    return true;
  }
  return false;
}

/**
 * @brief Initialize I2C slave peripheral.
 * @param[in,out] i2c Pointer to I2C slave control structure.
 */
void I2C_Slave_Init(I2C_Slave_t *i2c)
{
  RCC_EnableI2C(i2c->reg);
  void (*handler)(void *);
  if(i2c->sequence) handler = (void (*)(void *))&I2C_SlaveSequence_InterruptEV;
  else handler = (void (*)(void *))&I2C_SlaveSimple_InterruptEV;
  IRQ_EnableI2C(i2c->reg, i2c->irq_priority, handler, i2c);
  GPIO_InitAlternate(&i2c_scl_map[i2c->scl_pin], i2c->pull_up);
  GPIO_InitAlternate(&i2c_sda_map[i2c->sda_pin], i2c->pull_up);
  i2c->reg->TIMINGR = i2c->timing;
  i2c->reg->CR1 &= ~I2C_CR1_DNF;
  i2c->reg->OAR1 = (i2c->addr << 1) | I2C_OAR1_OA1EN;
  i2c->reg->CR1 = I2C_CR1_PE | I2C_CR1_ADDRIE | (i2c->filter << I2C_CR1_DNF_Pos);
}

//-------------------------------------------------------------------------------------------------
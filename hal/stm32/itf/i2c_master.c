// hal/stm32/i2c_master.c

#include "i2c_master.h"

//-------------------------------------------------------------------------------------------------

static inline void I2C_Master_StartRead(I2C_Master_t *i2c)
{
  i2c->_busy = false;
  I2C_Master_Read(i2c, i2c->_addr, i2c->_rx_ptr, i2c->_size);
  i2c->_size = 0;
}

static void I2C_Master_IRQHandler(I2C_Master_t *i2c)
{
  // Transfer complete - start read phase if pending
  if((i2c->reg->CR1 & I2C_CR1_TCIE) && (i2c->reg->ISR & I2C_ISR_TC)) {
    i2c->reg->CR1 &= ~I2C_CR1_TCIE;
    if(i2c->_size) I2C_Master_StartRead(i2c);
  }
  // TX buffer empty (interrupt mode)
  if(!i2c->tx_dma) {
    if((i2c->reg->CR1 & I2C_CR1_TXIE) && (i2c->reg->ISR & I2C_ISR_TXE)) {
      i2c->reg->TXDR = i2c->_tx_ptr[i2c->_tail];
      i2c->_tail++;
      if(i2c->_tail >= i2c->_head) {
        i2c->reg->CR1 &= ~I2C_CR1_TXIE;
        if(i2c->_size) I2C_Master_StartRead(i2c);
        else i2c->reg->CR1 |= I2C_CR1_STOPIE;
      }
    }
  }
  // RX buffer not empty (interrupt mode)
  if(!i2c->rx_dma) {
    if((i2c->reg->CR1 & I2C_CR1_RXIE) && (i2c->reg->ISR & I2C_ISR_RXNE)) {
      i2c->_rx_ptr[i2c->_tail] = i2c->reg->RXDR;
      i2c->_tail++;
      if(i2c->_tail >= i2c->_head) {
        i2c->reg->CR1 &= ~I2C_CR1_RXIE;
        i2c->reg->CR1 |= I2C_CR1_STOPIE;
      }
    }
  }
  // Stop condition
  if((i2c->reg->CR1 & I2C_CR1_STOPIE) && (i2c->reg->ISR & I2C_ISR_STOPF)) {
    i2c->reg->ICR |= I2C_ICR_STOPCF;
    i2c->reg->CR1 &= ~I2C_CR1_STOPIE;
    i2c->_busy = false;
    heap_free((void *)&i2c->_tx_buffer);
    i2c->_tx_buffer = NULL;
  }
  // NACK received
  if((i2c->reg->CR1 & I2C_CR1_NACKIE) && (i2c->reg->ISR & I2C_ISR_NACKF)) {
    i2c->reg->ICR |= I2C_ICR_NACKCF;
    i2c->reg->CR1 &= ~I2C_CR1_NACKIE;
    i2c->_busy = false;
  }
}

static void I2C_Master_DMA_TX_IRQHandler(I2C_Master_t *i2c)
{
  if(i2c->_tx_dma.reg->ISR & DMA_ISR_TCIF(i2c->_tx_dma.pos)) {
    i2c->_tx_dma.reg->IFCR |= DMA_ISR_TCIF(i2c->_tx_dma.pos);
    i2c->reg->CR1 |= I2C_CR1_STOPIE;
  }
}

static void I2C_Master_DMA_RX_IRQHandler(I2C_Master_t *i2c)
{
  if(i2c->_rx_dma.reg->ISR & DMA_ISR_TCIF(i2c->_rx_dma.pos)) {
    i2c->_rx_dma.reg->IFCR |= DMA_ISR_TCIF(i2c->_rx_dma.pos);
    i2c->reg->CR1 |= I2C_CR1_STOPIE;
  }
}

//-------------------------------------------------------------------------------------------------

void I2C_Master_Init(I2C_Master_t *i2c)
{
  RCC_EnableI2C(i2c->reg);
  IRQ_EnableI2C(i2c->reg, i2c->irq_priority, (IRQ_Handler_t)I2C_Master_IRQHandler, NULL, i2c);
  GPIO_InitAlternate(&I2C_SCL_MAP[i2c->scl], i2c->pull_up);
  GPIO_InitAlternate(&I2C_SDA_MAP[i2c->sda], i2c->pull_up);
  i2c->reg->TIMINGR = i2c->timing;
  i2c->reg->CR1 &= ~I2C_CR1_DNF;
  // TX DMA
  if(i2c->tx_dma) {
    DMA_SetRegisters(i2c->tx_dma, &i2c->_tx_dma);
    RCC_EnableDMA(i2c->_tx_dma.reg);
    i2c->_tx_dma.mux->CCR &= 0xFFFFFFC0;
    I2C_DmaSetTxRequest(i2c->reg, &i2c->_tx_dma);
    i2c->_tx_dma.cha->CPAR = (uint32_t)&i2c->reg->TXDR;
    i2c->_tx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE;
    IRQ_EnableDMA(i2c->tx_dma, i2c->irq_priority, (IRQ_Handler_t)I2C_Master_DMA_TX_IRQHandler, i2c);
    i2c->reg->CR1 |= I2C_CR1_TXDMAEN;
  }
  // RX DMA
  if(i2c->rx_dma) {
    DMA_SetRegisters(i2c->rx_dma, &i2c->_rx_dma);
    RCC_EnableDMA(i2c->_rx_dma.reg);
    i2c->_rx_dma.mux->CCR &= 0xFFFFFFC0;
    I2C_DmaSetRxRequest(i2c->reg, &i2c->_rx_dma);
    i2c->_rx_dma.cha->CPAR = (uint32_t)&i2c->reg->RXDR;
    i2c->_rx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_TCIE;
    IRQ_EnableDMA(i2c->rx_dma, i2c->irq_priority, (IRQ_Handler_t)I2C_Master_DMA_RX_IRQHandler, i2c);
    i2c->reg->CR1 |= I2C_CR1_RXDMAEN;
  }
  i2c->reg->CR1 |= I2C_CR1_PE | (i2c->filter << I2C_CR1_DNF_Pos);
  i2c->_busy = false;
}

void I2C_Master_Disable(I2C_Master_t *i2c)
{
  i2c->_busy = true;
  i2c->reg->CR1 &= ~I2C_CR1_PE;
  RCC_DisableI2C(i2c->reg);
}

//-------------------------------------------------------------------------------------------------

bool I2C_Master_IsBusy(I2C_Master_t *i2c)
{
  return i2c->_busy;
}

bool I2C_Master_IsFree(I2C_Master_t *i2c)
{
  return !i2c->_busy;
}

//-------------------------------------------------------------------------------------------------

status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(i2c->tx_dma) {
    i2c->_tx_dma.cha->CCR &= ~DMA_CCR_EN;
    i2c->_tx_dma.cha->CMAR = (uint32_t)data;
    i2c->_tx_dma.cha->CNDTR = len;
    i2c->_tx_dma.cha->CCR |= DMA_CCR_EN;
  }
  else {
    i2c->_tx_ptr = data;
    i2c->_tail = 1;
    i2c->_head = len;
    i2c->reg->TXDR = data[0];
    if(len == 1) i2c->reg->CR1 |= I2C_CR1_STOPIE | I2C_CR1_NACKIE;
    else i2c->reg->CR1 |= I2C_CR1_TXIE | I2C_CR1_NACKIE;
  }
  i2c->reg->CR2 = I2C_CR2_AUTOEND | (len << 16) | (addr << 1) | I2C_CR2_START;
  i2c->_busy = true;
  return FREE;
}

status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(i2c->rx_dma) {
    i2c->_rx_dma.cha->CCR &= ~DMA_CCR_EN;
    i2c->_rx_dma.cha->CMAR = (uint32_t)data;
    i2c->_rx_dma.cha->CNDTR = len;
    i2c->_rx_dma.cha->CCR |= DMA_CCR_EN;
  }
  else {
    i2c->_rx_ptr = data;
    i2c->_tail = 0;
    i2c->_head = len;
    i2c->reg->CR1 |= I2C_CR1_RXIE;
  }
  i2c->reg->CR2 = I2C_CR2_AUTOEND | (len << 16) | I2C_CR2_RD_WRN | (addr << 1) | I2C_CR2_START;
  i2c->_busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------

status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  i2c->_tx_buffer = heap_alloc(len + 1);
  if(!i2c->_tx_buffer) return ERR;
  i2c->_tx_buffer[0] = reg;
  memcpy(&i2c->_tx_buffer[1], data, len);
  return I2C_Master_Write(i2c, addr, i2c->_tx_buffer, len + 1);
}

status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  i2c->_addr = addr;
  i2c->_rx_ptr = data;
  i2c->_size = len;
  i2c->reg->CR1 |= I2C_CR1_TCIE | I2C_CR1_NACKIE;
  i2c->reg->CR2 = (1 << 16) | (addr << 1) | I2C_CR2_START;
  i2c->reg->TXDR = reg;
  i2c->_busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------

status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len)
{
  if(i2c->_busy) return BUSY;
  i2c->_addr = addr;
  i2c->_rx_ptr = rx_data;
  i2c->_size = rx_len;
  if(i2c->tx_dma) {
    i2c->_tx_dma.cha->CCR &= ~DMA_CCR_EN;
    i2c->_tx_dma.cha->CMAR = (uint32_t)tx_data;
    i2c->_tx_dma.cha->CNDTR = tx_len;
    i2c->_tx_dma.cha->CCR |= DMA_CCR_EN;
    i2c->reg->CR1 |= I2C_CR1_TCIE | I2C_CR1_NACKIE;
  }
  else {
    i2c->_tx_ptr = tx_data;
    i2c->_tail = 1;
    i2c->_head = tx_len;
    i2c->reg->TXDR = tx_data[0];
    i2c->reg->CR1 |= I2C_CR1_TXIE | I2C_CR1_TCIE | I2C_CR1_NACKIE;
  }
  i2c->reg->CR2 = (tx_len << 16) | (addr << 1) | I2C_CR2_START;
  i2c->_busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------
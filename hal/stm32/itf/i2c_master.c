// hal/stm32/itf/i2c_master.c

#include "i2c_master.h"

#include <string.h>
#include "heap.h"

//---------------------------------------------------------------------------------------- Handlers

// Write phase done, the read of `_size` bytes follows under a repeated start
static inline void start_read(I2C_Master_t *i2c)
{
  i2c->_busy = false;
  I2C_Master_Read(i2c, i2c->_addr, i2c->_rx_ptr, i2c->_size);
  i2c->_size = 0;
}

static void release_tx_buffer(I2C_Master_t *i2c)
{
  heap_free(i2c->_tx_buffer);
  i2c->_tx_buffer = NULL;
}

static void irq_handler(I2C_Master_t *i2c)
{
  uint32_t cr1 = i2c->reg->CR1;
  uint32_t isr = i2c->reg->ISR;
  // Transfer complete under a pending read: the write phase ended
  if((cr1 & I2C_CR1_TCIE) && (isr & I2C_ISR_TC)) {
    i2c->reg->CR1 &= ~I2C_CR1_TCIE;
    if(i2c->_size) start_read(i2c);
  }
  if(!i2c->tx_dma && (cr1 & I2C_CR1_TXIE) && (isr & I2C_ISR_TXE)) {
    i2c->reg->TXDR = i2c->_tx_ptr[i2c->_tail++];
    if(i2c->_tail >= i2c->_head) {
      i2c->reg->CR1 &= ~I2C_CR1_TXIE;
      if(i2c->_size) start_read(i2c);
      else i2c->reg->CR1 |= I2C_CR1_STOPIE;
    }
  }
  if(!i2c->rx_dma && (cr1 & I2C_CR1_RXIE) && (isr & I2C_ISR_RXNE)) {
    i2c->_rx_ptr[i2c->_tail++] = i2c->reg->RXDR;
    if(i2c->_tail >= i2c->_head) {
      i2c->reg->CR1 &= ~I2C_CR1_RXIE;
      i2c->reg->CR1 |= I2C_CR1_STOPIE;
    }
  }
  if((cr1 & I2C_CR1_STOPIE) && (isr & I2C_ISR_STOPF)) {
    i2c->reg->ICR |= I2C_ICR_STOPCF;
    i2c->reg->CR1 &= ~I2C_CR1_STOPIE;
    i2c->_busy = false;
    release_tx_buffer(i2c);
  }
  if((cr1 & I2C_CR1_NACKIE) && (isr & I2C_ISR_NACKF)) {
    // `AUTOEND` turns the NACK into a STOP, so `STOPF` is raised here too
    i2c->reg->ICR |= I2C_ICR_NACKCF | I2C_ICR_STOPCF;
    i2c->reg->CR1 &= ~(I2C_CR1_NACKIE | I2C_CR1_STOPIE |
      I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_TCIE);
    release_tx_buffer(i2c);
    i2c->_nack = true;
    i2c->_busy = false;
  }
}

// DMA moved the last byte, the STOP that follows ends the transfer
static void dma_handler(I2C_Master_t *i2c, DMA_t *dma)
{
  if(dma->reg->ISR & DMA_ISR_TCIF(dma->pos)) {
    dma->reg->IFCR |= DMA_ISR_TCIF(dma->pos);
    i2c->reg->CR1 |= I2C_CR1_STOPIE;
  }
}

static void dma_tx_handler(I2C_Master_t *i2c) { dma_handler(i2c, &i2c->_tx_dma); }
static void dma_rx_handler(I2C_Master_t *i2c) { dma_handler(i2c, &i2c->_rx_dma); }

//-------------------------------------------------------------------------------------------- Init

static void dma_init(I2C_Master_t *i2c, DMA_CHx_t channel, DMA_t *dma, bool tx)
{
  DMA_SetRegisters(channel, dma);
  RCC_EnableDMA(dma->reg);
  dma->mux->CCR &= ~0x3Fu;
  if(tx) {
    I2C_DmaSetTxRequest(i2c->reg, dma);
    dma->cha->CPAR = (uint32_t)&i2c->reg->TXDR;
    dma->cha->CCR |= DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE;
    IRQ_EnableDMA(channel, i2c->irq_priority, (IRQ_Handler_t)dma_tx_handler, i2c);
    i2c->reg->CR1 |= I2C_CR1_TXDMAEN;
  }
  else {
    I2C_DmaSetRxRequest(i2c->reg, dma);
    dma->cha->CPAR = (uint32_t)&i2c->reg->RXDR;
    dma->cha->CCR |= DMA_CCR_MINC | DMA_CCR_TCIE;
    IRQ_EnableDMA(channel, i2c->irq_priority, (IRQ_Handler_t)dma_rx_handler, i2c);
    i2c->reg->CR1 |= I2C_CR1_RXDMAEN;
  }
}

void I2C_Master_Init(I2C_Master_t *i2c)
{
  RCC_EnableI2C(i2c->reg);
  IRQ_EnableI2C(i2c->reg, i2c->irq_priority, (IRQ_Handler_t)irq_handler, NULL, i2c);
  GPIO_InitAlternate(&I2C_SCL_MAP[i2c->scl], i2c->pull_up);
  GPIO_InitAlternate(&I2C_SDA_MAP[i2c->sda], i2c->pull_up);
  i2c->reg->TIMINGR = i2c->timing;
  i2c->reg->CR1 &= ~I2C_CR1_DNF;
  if(i2c->tx_dma) dma_init(i2c, i2c->tx_dma, &i2c->_tx_dma, true);
  if(i2c->rx_dma) dma_init(i2c, i2c->rx_dma, &i2c->_rx_dma, false);
  i2c->reg->CR1 |= I2C_CR1_PE | (i2c->filter << I2C_CR1_DNF_Pos);
  i2c->_busy = false;
}

void I2C_Master_Disable(I2C_Master_t *i2c)
{
  i2c->_busy = true;
  i2c->reg->CR1 &= ~I2C_CR1_PE;
  RCC_DisableI2C(i2c->reg);
}

bool I2C_Master_IsBusy(I2C_Master_t *i2c) { return i2c->_busy; }
bool I2C_Master_IsFree(I2C_Master_t *i2c) { return !i2c->_busy; }
bool I2C_Master_Nack(I2C_Master_t *i2c) { return i2c->_nack; }

//---------------------------------------------------------------------------------------- Transfer

// Arm a DMA channel on `data`
static void dma_arm(DMA_t *dma, uint8_t *data, uint16_t len)
{
  dma->cha->CCR &= ~DMA_CCR_EN;
  dma->cha->CMAR = (uint32_t)data;
  dma->cha->CNDTR = len;
  dma->cha->CCR |= DMA_CCR_EN;
}

// Queue a write of `len` bytes: DMA, or the first byte now and the rest from the interrupt
static void tx_arm(I2C_Master_t *i2c, uint8_t *data, uint16_t len)
{
  if(i2c->tx_dma) {
    dma_arm(&i2c->_tx_dma, data, len);
    return;
  }
  i2c->_tx_ptr = data;
  i2c->_tail = 1;
  i2c->_head = len;
  i2c->reg->TXDR = data[0];
  if(len > 1) i2c->reg->CR1 |= I2C_CR1_TXIE;
}

status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len > I2C_TRANSFER_MAX) return ERR;
  i2c->_nack = false;
  tx_arm(i2c, data, len);
  if(!i2c->tx_dma && len == 1) i2c->reg->CR1 |= I2C_CR1_STOPIE;
  // An absent device produces an address NACK and nothing else
  i2c->reg->CR1 |= I2C_CR1_NACKIE;
  i2c->reg->CR2 = I2C_CR2_AUTOEND | (len << 16) | (addr << 1) | I2C_CR2_START;
  i2c->_busy = true;
  return FREE;
}

status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len > I2C_TRANSFER_MAX) return ERR;
  i2c->_nack = false;
  if(i2c->rx_dma) dma_arm(&i2c->_rx_dma, data, len);
  else {
    i2c->_rx_ptr = data;
    i2c->_tail = 0;
    i2c->_head = len;
    i2c->reg->CR1 |= I2C_CR1_RXIE;
  }
  i2c->reg->CR1 |= I2C_CR1_NACKIE;
  i2c->reg->CR2 = I2C_CR2_AUTOEND | (len << 16) | I2C_CR2_RD_WRN | (addr << 1) | I2C_CR2_START;
  i2c->_busy = true;
  return FREE;
}

status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len >= I2C_TRANSFER_MAX) return ERR;
  i2c->_tx_buffer = heap_alloc(len + 1);
  if(!i2c->_tx_buffer) return ERR;
  i2c->_tx_buffer[0] = reg;
  memcpy(&i2c->_tx_buffer[1], data, len);
  return I2C_Master_Write(i2c, addr, i2c->_tx_buffer, len + 1);
}

status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len)
{
  if(i2c->_busy) return BUSY;
  if(!data || !len || len > I2C_TRANSFER_MAX) return ERR;
  i2c->_nack = false;
  i2c->_addr = addr;
  i2c->_rx_ptr = data;
  i2c->_size = len;
  i2c->reg->CR1 |= I2C_CR1_TCIE | I2C_CR1_NACKIE;
  i2c->reg->CR2 = (1 << 16) | (addr << 1) | I2C_CR2_START;
  i2c->reg->TXDR = reg;
  i2c->_busy = true;
  return FREE;
}

status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr,
  uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len)
{
  if(i2c->_busy) return BUSY;
  if(!tx_data || !tx_len || tx_len > I2C_TRANSFER_MAX) return ERR;
  if(!rx_data || !rx_len || rx_len > I2C_TRANSFER_MAX) return ERR;
  i2c->_nack = false;
  i2c->_addr = addr;
  i2c->_rx_ptr = rx_data;
  i2c->_size = rx_len;
  tx_arm(i2c, tx_data, tx_len);
  i2c->reg->CR1 |= I2C_CR1_TCIE | I2C_CR1_NACKIE;
  i2c->reg->CR2 = (tx_len << 16) | (addr << 1) | I2C_CR2_START;
  i2c->_busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------

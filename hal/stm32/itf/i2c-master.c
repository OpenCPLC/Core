#include "i2c-master.h"

extern void I2C_DmaSetTxRequest(I2C_TypeDef *reg, DMA_t *dma);
extern void I2C_DmaSetRxRequest(I2C_TypeDef *reg, DMA_t *dma);

//-------------------------------------------------------------------------------------------------

static inline void I2C_Master_ReadEV(I2C_Master_t *i2c)
{
  i2c->busy = false;
  I2C_Master_Read(i2c, i2c->address, i2c->rx_location, i2c->size);
  i2c->size = 0;
}

static void I2C_Master_InterruptEV(I2C_Master_t *i2c)
{
  // Transfer complete - start read phase if pending
  if((i2c->reg->CR1 & I2C_CR1_TCIE) && (i2c->reg->ISR & I2C_ISR_TC)) {
    i2c->reg->CR1 &= ~I2C_CR1_TCIE;
    if(i2c->size) I2C_Master_ReadEV(i2c);
  }
  // TX buffer empty - send next byte (interrupt mode)
  if(!i2c->tx_dma_nbr) {
    if((i2c->reg->CR1 & I2C_CR1_TXIE) && (i2c->reg->ISR & I2C_ISR_TXE)) {
      i2c->reg->TXDR = i2c->tx_location[i2c->tail];
      i2c->tail++;
      if(i2c->tail >= i2c->head) {
        i2c->reg->CR1 &= ~I2C_CR1_TXIE;
        if(i2c->size) I2C_Master_ReadEV(i2c);
        else i2c->reg->CR1 |= I2C_CR1_STOPIE;
      }
    }
  }
  // RX buffer not empty - read byte (interrupt mode)
  if(!i2c->rx_dma_nbr) {
    if((i2c->reg->CR1 & I2C_CR1_RXIE) && (i2c->reg->ISR & I2C_ISR_RXNE)) {
      i2c->rx_location[i2c->tail] = i2c->reg->RXDR;
      i2c->tail++;
      if(i2c->tail >= i2c->head) {
        i2c->reg->CR1 &= ~I2C_CR1_RXIE;
        i2c->reg->CR1 |= I2C_CR1_STOPIE;
      }
    }
  }
  // Stop condition detected
  if((i2c->reg->CR1 & I2C_CR1_STOPIE) && (i2c->reg->ISR & I2C_ISR_STOPF)) {
    i2c->reg->ICR |= I2C_ICR_STOPCF;
    i2c->reg->CR1 &= ~I2C_CR1_STOPIE;
    i2c->busy = false;
    heap_free((void *)&i2c->tx_buffer);
    i2c->tx_buffer = NULL;
  }
  // NACK received
  if((i2c->reg->CR1 & I2C_CR1_NACKIE) && (i2c->reg->ISR & I2C_ISR_NACKF)) {
    i2c->reg->ICR |= I2C_ICR_NACKCF;
    i2c->reg->CR1 &= ~I2C_CR1_NACKIE;
    i2c->busy = false;
  }
}

static void I2C_Master_InterruptDMA_TX(I2C_Master_t *i2c)
{
  if(i2c->tx_dma.reg->ISR & DMA_ISR_TCIF(i2c->tx_dma.pos)) {
    i2c->tx_dma.reg->IFCR |= DMA_ISR_TCIF(i2c->tx_dma.pos);
    i2c->reg->CR1 |= I2C_CR1_STOPIE;
  }
}

static void I2C_Master_InterruptDMA_RX(I2C_Master_t *i2c)
{
  if(i2c->rx_dma.reg->ISR & DMA_ISR_TCIF(i2c->rx_dma.pos)) {
    i2c->rx_dma.reg->IFCR |= DMA_ISR_TCIF(i2c->rx_dma.pos);
    i2c->reg->CR1 |= I2C_CR1_STOPIE;
  }
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize I2C master peripheral.
 * Configures GPIO, clock, interrupts and optional DMA.
 * @param[in,out] i2c Pointer to I2C master control structure.
 */
void I2C_Master_Init(I2C_Master_t *i2c)
{
  RCC_EnableI2C(i2c->reg);
  IRQ_EnableI2C(i2c->reg, i2c->irq_priority, (void (*)(void *))&I2C_Master_InterruptEV, i2c);
  GPIO_InitAlternate(&i2c_scl_map[i2c->scl_pin], i2c->pull_up);
  GPIO_InitAlternate(&i2c_sda_map[i2c->sda_pin], i2c->pull_up);
  i2c->reg->TIMINGR = i2c->timing;
  i2c->reg->CR1 &= ~I2C_CR1_DNF;
  // TX DMA
  if(i2c->tx_dma_nbr) {
    DMA_SetRegisters(i2c->tx_dma_nbr, &i2c->tx_dma);
    RCC_EnableDMA(i2c->tx_dma.reg);
    i2c->tx_dma.mux->CCR &= 0xFFFFFFC0;
    I2C_DmaSetTxRequest(i2c->reg, &i2c->tx_dma);
    i2c->tx_dma.cha->CPAR = (uint32_t)&(i2c->reg->TXDR);
    i2c->tx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE;
    IRQ_EnableDMA(i2c->tx_dma_nbr, i2c->irq_priority, (void (*)(void *))&I2C_Master_InterruptDMA_TX, i2c);
    i2c->reg->CR1 |= I2C_CR1_TXDMAEN;
  }
  // RX DMA
  if(i2c->rx_dma_nbr) {
    DMA_SetRegisters(i2c->rx_dma_nbr, &i2c->rx_dma);
    RCC_EnableDMA(i2c->rx_dma.reg);
    i2c->rx_dma.mux->CCR &= 0xFFFFFFC0;
    I2C_DmaSetRxRequest(i2c->reg, &i2c->rx_dma);
    i2c->rx_dma.cha->CPAR = (uint32_t)&(i2c->reg->RXDR);
    i2c->rx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_TCIE;
    IRQ_EnableDMA(i2c->rx_dma_nbr, i2c->irq_priority, (void (*)(void *))&I2C_Master_InterruptDMA_RX, i2c);
    i2c->reg->CR1 |= I2C_CR1_RXDMAEN;
  }
  i2c->reg->CR1 |= I2C_CR1_PE | (i2c->filter << I2C_CR1_DNF_Pos);
  i2c->busy = false;
}

/**
 * @brief Disable I2C master peripheral.
 * @param[in,out] i2c Pointer to I2C master control structure.
 */
void I2C_Master_Disable(I2C_Master_t *i2c)
{
  i2c->busy = true;
  i2c->reg->CR1 &= ~I2C_CR1_PE;
  RCC_DisableI2C(i2c->reg);
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Check if I2C is busy.
 * @param[in] i2c Pointer to I2C master control structure.
 * @return `true` if busy, `false` if free.
 */
bool I2C_Master_IsBusy(I2C_Master_t *i2c)
{
  return i2c->busy;
}

/**
 * @brief Check if I2C is free.
 * @param[in] i2c Pointer to I2C master control structure.
 * @return `true` if free, `false` if busy.
 */
bool I2C_Master_IsFree(I2C_Master_t *i2c)
{
  return !i2c->busy;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Write data to I2C device.
 * @param[in,out] i2c Pointer to I2C master control structure.
 * @param[in] addr 7-bit device address.
 * @param[in] ary Pointer to data buffer.
 * @param[in] n Number of bytes to write.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *ary, uint16_t n)
{
  if(i2c->busy) return BUSY;
  if(i2c->tx_dma_nbr) {
    i2c->tx_dma.cha->CCR &= ~DMA_CCR_EN;
    i2c->tx_dma.cha->CMAR = (uint32_t)ary;
    i2c->tx_dma.cha->CNDTR = n;
    i2c->tx_dma.cha->CCR |= DMA_CCR_EN;
  }
  else {
    i2c->tx_location = ary;
    i2c->tail = 1;
    i2c->head = n;
    i2c->reg->TXDR = ary[0];
    if(n == 1) i2c->reg->CR1 |= I2C_CR1_STOPIE | I2C_CR1_NACKIE;
    else i2c->reg->CR1 |= I2C_CR1_TXIE | I2C_CR1_NACKIE;
  }
  i2c->reg->CR2 = I2C_CR2_AUTOEND | (n << 16) | (addr << 1) | I2C_CR2_START;
  i2c->busy = true;
  return FREE;
}

/**
 * @brief Read data from I2C device.
 * @param[in,out] i2c Pointer to I2C master control structure.
 * @param[in] addr 7-bit device address.
 * @param[out] ary Pointer to receive buffer.
 * @param[in] n Number of bytes to read.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *ary, uint16_t n)
{
  if(i2c->busy) return BUSY;
  if(i2c->rx_dma_nbr) {
    i2c->rx_dma.cha->CCR &= ~DMA_CCR_EN;
    i2c->rx_dma.cha->CMAR = (uint32_t)ary;
    i2c->rx_dma.cha->CNDTR = n;
    i2c->rx_dma.cha->CCR |= DMA_CCR_EN;
  }
  else {
    i2c->rx_location = ary;
    i2c->tail = 0;
    i2c->head = n;
    i2c->reg->CR1 |= I2C_CR1_RXIE;
  }
  i2c->reg->CR2 = I2C_CR2_AUTOEND | (n << 16) | I2C_CR2_RD_WRN | (addr << 1) | I2C_CR2_START;
  i2c->busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Write data to I2C device register.
 * Sends register address followed by data bytes.
 * @param[in,out] i2c Pointer to I2C master control structure.
 * @param[in] addr 7-bit device address.
 * @param[in] reg Register address.
 * @param[in] ary Pointer to data buffer.
 * @param[in] n Number of data bytes to write.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *ary, uint16_t n)
{
  if(i2c->busy) return BUSY;
  i2c->tx_buffer = heap_alloc(n + 1);
  if(!i2c->tx_buffer) return ERR;
  i2c->tx_buffer[0] = reg;
  memcpy(&i2c->tx_buffer[1], ary, n);
  return I2C_Master_Write(i2c, addr, i2c->tx_buffer, n + 1);
}

/**
 * @brief Read data from I2C device register.
 * Sends register address, then restarts and reads data.
 * @param[in,out] i2c Pointer to I2C master control structure.
 * @param[in] addr 7-bit device address.
 * @param[in] reg Register address.
 * @param[out] ary Pointer to receive buffer.
 * @param[in] n Number of bytes to read.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *ary, uint16_t n)
{
  if(i2c->busy) return BUSY;
  i2c->address = addr;
  i2c->rx_location = ary;
  i2c->size = n;
  i2c->reg->CR1 |= I2C_CR1_TCIE | I2C_CR1_NACKIE;
  i2c->reg->CR2 = (1 << 16) | (addr << 1) | I2C_CR2_START;
  i2c->reg->TXDR = reg;
  i2c->busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Combined write-then-read I2C transaction.
 * Writes data, then restarts and reads response.
 * @param[in,out] i2c Pointer to I2C master control structure.
 * @param[in] addr 7-bit device address.
 * @param[in] write_ary Pointer to write data buffer.
 * @param[in] write_n Number of bytes to write.
 * @param[out] read_ary Pointer to receive buffer.
 * @param[in] read_n Number of bytes to read.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *write_ary, uint16_t write_n, uint8_t *read_ary, uint16_t read_n)
{
  if(i2c->busy) return BUSY;
  i2c->address = addr;
  i2c->rx_location = read_ary;
  i2c->size = read_n;
  if(i2c->tx_dma_nbr) {
    i2c->tx_dma.cha->CCR &= ~DMA_CCR_EN;
    i2c->tx_dma.cha->CMAR = (uint32_t)write_ary;
    i2c->tx_dma.cha->CNDTR = write_n;
    i2c->tx_dma.cha->CCR |= DMA_CCR_EN;
    i2c->reg->CR1 |= I2C_CR1_TCIE | I2C_CR1_NACKIE;
  }
  else {
    i2c->tx_location = write_ary;
    i2c->tail = 1;
    i2c->head = write_n;
    i2c->reg->TXDR = write_ary[0];
    i2c->reg->CR1 |= I2C_CR1_TXIE | I2C_CR1_TCIE | I2C_CR1_NACKIE;
  }
  i2c->reg->CR2 = (write_n << 16) | (addr << 1) | I2C_CR2_START;
  i2c->busy = true;
  return FREE;
}

//-------------------------------------------------------------------------------------------------

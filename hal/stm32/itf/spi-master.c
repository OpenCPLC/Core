#include "spi-master.h"

extern void SPI_DmaSetRxRequest(SPI_TypeDef *reg, DMA_t *dma);
extern void SPI_DmaSetTxRequest(SPI_TypeDef *reg, DMA_t *dma);

//-------------------------------------------------------------------------------------------------

static void SPI_Master_InterruptDMA(SPI_Master_t *spi)
{
  if(spi->rx_dma.reg->ISR & DMA_ISR_TCIF(spi->rx_dma.pos)) {
    spi->rx_dma.reg->IFCR |= DMA_ISR_TCIF(spi->rx_dma.pos);
    if(spi->cs_gpio) GPIO_Rst(spi->cs_gpio);
    spi->busy_flag = false;
    if(!spi->mosi_pin) spi->reg->CR1 &= ~SPI_CR1_SPE;
  }
}

/**
 * @brief Initialize SPI master peripheral with DMA.
 * @param[in,out] spi Pointer to SPI master control structure.
 */
void SPI_Master_Init(SPI_Master_t *spi)
{
  DMA_SetRegisters(spi->rx_dma_nbr, &spi->rx_dma);
  DMA_SetRegisters(spi->tx_dma_nbr, &spi->tx_dma);
  RCC_EnableDMA(spi->rx_dma.reg);
  RCC_EnableDMA(spi->tx_dma.reg);
  GPIO_InitAlternate(&SPI_SCK_MAP[spi->sck_pin], false);
  if(spi->miso_pin) {
    spi->rx_dma.mux->CCR &= 0xFFFFFFC0;
    SPI_DmaSetRxRequest(spi->reg, &spi->rx_dma);
    IRQ_EnableDMA(spi->rx_dma_nbr, spi->irq_priority, (void (*)(void *))&SPI_Master_InterruptDMA, spi);
    GPIO_InitAlternate(&SPI_MISO_MAP[spi->miso_pin], false);
    spi->rx_dma.cha->CPAR = (uint32_t)&(spi->reg->DR);
    spi->rx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_TCIE;
  }
  if(spi->mosi_pin) {
    spi->tx_dma.mux->CCR &= 0xFFFFFFC0;
    SPI_DmaSetTxRequest(spi->reg, &spi->tx_dma);
    GPIO_InitAlternate(&SPI_MOSI_MAP[spi->mosi_pin], false);
    spi->tx_dma.cha->CPAR = (uint32_t)&(spi->reg->DR);
    spi->tx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_DIR;
  }
  uint32_t cr2 = SPI_CR2_RXDMAEN | SPI_CR2_FRXTH | SPI_CR2_SSOE | 0x00000700;
  uint32_t cr1 = SPI_CR1_MSTR | (spi->lsb << 7) | (spi->prescaler << 3) | SPI_CR1_SPE | (spi->cpol << 1) | spi->cpha;
  if(spi->cs_gpio) {
    spi->cs_gpio->mode = GPIO_Mode_Output;
    GPIO_Init(spi->cs_gpio);
  }
  else {
    cr2 |= SPI_CR1_SSM | SPI_CR1_SSI;
  }
  if(spi->mosi_pin) cr2 |= SPI_CR2_TXDMAEN;
  else cr1 |= SPI_CR1_RXONLY;
  RCC_EnableSPI(spi->reg);
  spi->reg->CR2 = cr2;
  spi->reg->CR1 = cr1;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Check if SPI is busy.
 * @param[in] spi Pointer to SPI master control structure.
 * @return `true` if busy, `false` if free.
 */
bool SPI_Master_IsBusy(SPI_Master_t *spi)
{
  return spi->busy_flag;
}

/**
 * @brief Check if SPI is free.
 * @param[in] spi Pointer to SPI master control structure.
 * @return `true` if free, `false` if busy.
 */
bool SPI_Master_IsFree(SPI_Master_t *spi)
{
  return !spi->busy_flag;
}

static void SPI_Master_Start(SPI_Master_t *spi)
{
  spi->tx_dma.cha->CCR &= ~DMA_CCR_EN;
  spi->rx_dma.cha->CCR &= ~DMA_CCR_EN;
}

static void SPI_Master_End(SPI_Master_t *spi, uint16_t n)
{
  spi->tx_dma.cha->CNDTR = n;
  spi->rx_dma.cha->CNDTR = n;
  if(spi->cs_gpio) {
    GPIO_Set(spi->cs_gpio);
    SPI_Delay(spi->cs_delay);
  }
  spi->rx_dma.cha->CCR |= DMA_CCR_EN;
  spi->tx_dma.cha->CCR |= DMA_CCR_EN;
  spi->busy_flag = true;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Full-duplex SPI transfer.
 * @param[in,out] spi Pointer to SPI master control structure.
 * @param[out] rx_buff Pointer to receive buffer.
 * @param[in] tx_buff Pointer to transmit buffer.
 * @param[in] n Number of bytes to transfer.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t SPI_Master_Run(SPI_Master_t *spi, uint8_t *rx_buff, uint8_t *tx_buff, uint16_t n)
{
  if(spi->busy_flag) return BUSY;
  SPI_Master_Start(spi);
  spi->tx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->rx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->tx_dma.cha->CMAR = (uint32_t)tx_buff;
  spi->rx_dma.cha->CMAR = (uint32_t)rx_buff;
  SPI_Master_End(spi, n);
  return FREE;
}

/**
 * @brief Start RX-only SPI transfer (no MOSI).
 * @param[in,out] spi Pointer to SPI master control structure.
 * @param[out] rx_buff Pointer to receive buffer.
 * @param[in] n Number of bytes to read.
 */
void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_buff, uint16_t n)
{
  spi->rx_dma.cha->CCR &= ~DMA_CCR_EN;
  spi->rx_dma.cha->CMAR = (uint32_t)rx_buff;
  spi->rx_dma.cha->CNDTR = n;
  spi->rx_dma.cha->CCR |= DMA_CCR_EN;
  spi->busy_flag = true;
  spi->reg->CR1 |= SPI_CR1_SPE;
}

/**
 * @brief Read from SPI device sending constant address byte.
 * @param[in,out] spi Pointer to SPI master control structure.
 * @param[in] addr Constant byte to send (e.g. register address).
 * @param[out] rx_buff Pointer to receive buffer.
 * @param[in] n Number of bytes to read.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t addr, uint8_t *rx_buff, uint16_t n)
{
  if(spi->busy_flag) return BUSY;
  spi->const_reg = addr;
  if(!spi->mosi_pin) {
    SPI_Master_OnlyRead(spi, rx_buff, n);
    return FREE;
  }
  SPI_Master_Start(spi);
  spi->tx_dma.cha->CCR &= ~DMA_CCR_MINC;
  spi->rx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->tx_dma.cha->CMAR = (uint32_t)&(spi->const_reg);
  spi->rx_dma.cha->CMAR = (uint32_t)rx_buff;
  SPI_Master_End(spi, n);
  return FREE;
}

/**
 * @brief Write to SPI device.
 * @param[in,out] spi Pointer to SPI master control structure.
 * @param[in] tx_buff Pointer to transmit buffer.
 * @param[in] n Number of bytes to write.
 * @return `FREE` if started, `BUSY` if transfer in progress.
 */
status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_buff, uint16_t n)
{
  if(spi->busy_flag) return BUSY;
  SPI_Master_Start(spi);
  spi->tx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->rx_dma.cha->CCR &= ~DMA_CCR_MINC;
  spi->tx_dma.cha->CMAR = (uint32_t)tx_buff;
  spi->rx_dma.cha->CMAR = (uint32_t)&(spi->const_reg);
  SPI_Master_End(spi, n);
  return FREE;
}

//-------------------------------------------------------------------------------------------------
#if(SPI_SOFTWARE_ENABLE)

/**
 * @brief Initialize software (bit-bang) SPI.
 * @param[in,out] spi Pointer to software SPI control structure.
 */
void SPI_Software_Init(SPI_Software_t *spi)
{
  spi->cs->mode = GPIO_Mode_Output;
  spi->sck->mode = GPIO_Mode_Output;
  spi->miso->mode = GPIO_Mode_Input;
  spi->mosi->mode = GPIO_Mode_Output;
  GPIO_InitList(spi->cs, spi->sck, spi->miso, spi->mosi, NULL);
  #if(SPI_SOFTWARE_CPOL)
  GPIO_Set(spi->sck);
  #else
  GPIO_Rst(spi->sck);
  #endif
  GPIO_Rst(spi->cs);
}

static void SPI_Software_Write(GPIO_t *mosi, uint8_t *byte_tx)
{
  #if(SPI_SOFTWARE_LSB)
  if(*byte_tx & 1) GPIO_Set(mosi);
  else GPIO_Rst(mosi);
  *byte_tx >>= 1;
  #else
  if(*byte_tx & 0x80) GPIO_Set(mosi);
  else GPIO_Rst(mosi);
  *byte_tx <<= 1;
  #endif
}

static void SPI_Software_Read(GPIO_t *miso, uint8_t *byte_rx)
{
  #if(SPI_SOFTWARE_LSB)
  *byte_rx >>= 1;
  if(GPIO_In(miso)) *byte_rx |= 0x80;
  #else
  *byte_rx <<= 1;
  if(GPIO_In(miso)) *byte_rx |= 1;
  #endif
}

/**
 * @brief Full-duplex software SPI transfer.
 * @param[in,out] spi Pointer to software SPI control structure.
 * @param[out] rx_buff Pointer to receive buffer.
 * @param[in] tx_buff Pointer to transmit buffer.
 * @param[in] n Number of bytes to transfer.
 */
void SPI_Software_Run(SPI_Software_t *spi, uint8_t *rx_buff, uint8_t *tx_buff, uint16_t n)
{
  #if(SPI_SOFTWARE_CPOL)
  GPIO_Set(spi->sck);
  #else
  GPIO_Rst(spi->sck);
  #endif
  GPIO_Set(spi->cs);
  for(uint32_t i = 0; i < spi->delay; i++) { __NOP(); }
  for(uint16_t i = 0; i < n; i++) {
    uint8_t byte_tx = tx_buff[i];
    uint8_t byte_rx = 0;
    for(uint8_t j = 0; j < 8; j++) {
      GPIO_Tgl(spi->sck); __NOP();
      #if(!SPI_SOFTWARE_CPHA)
      SPI_Software_Write(spi->mosi, &byte_tx);
      SPI_Software_Read(spi->miso, &byte_rx);
      #endif
      GPIO_Tgl(spi->sck); __NOP();
      #if(SPI_SOFTWARE_CPHA)
      SPI_Software_Write(spi->mosi, &byte_tx);
      SPI_Software_Read(spi->miso, &byte_rx);
      #endif
    }
    rx_buff[i] = byte_rx;
  }
  #if(SPI_SOFTWARE_CPOL)
  GPIO_Rst(spi->sck);
  #else
  GPIO_Set(spi->sck);
  #endif
  GPIO_Rst(spi->cs);
}

#endif
//-------------------------------------------------------------------------------------------------

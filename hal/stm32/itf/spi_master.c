// hal/stm32/spi_master.c

#include "spi_master.h"

//-------------------------------------------------------------------------------------------------

static void SPI_Master_DMA_IRQHandler(SPI_Master_t *spi)
{
  if(spi->_rx_dma.reg->ISR & DMA_ISR_TCIF(spi->_rx_dma.pos)) {
    spi->_rx_dma.reg->IFCR |= DMA_ISR_TCIF(spi->_rx_dma.pos);
    if(spi->cs) GPIO_Rst(spi->cs);
    spi->_busy = false;
    if(!spi->mosi) spi->reg->CR1 &= ~SPI_CR1_SPE;
  }
}

void SPI_Master_Init(SPI_Master_t *spi)
{
  DMA_SetRegisters(spi->rx_dma, &spi->_rx_dma);
  DMA_SetRegisters(spi->tx_dma, &spi->_tx_dma);
  RCC_EnableDMA(spi->_rx_dma.reg);
  RCC_EnableDMA(spi->_tx_dma.reg);
  GPIO_InitAlternate(&SPI_SCK_MAP[spi->sck], false);
  if(spi->miso) {
    spi->_rx_dma.mux->CCR &= 0xFFFFFFC0;
    SPI_DmaSetRxRequest(spi->reg, &spi->_rx_dma);
    IRQ_EnableDMA(spi->rx_dma, spi->irq_priority, (IRQ_Handler_t)SPI_Master_DMA_IRQHandler, spi);
    GPIO_InitAlternate(&SPI_MISO_MAP[spi->miso], false);
    spi->_rx_dma.cha->CPAR = (uint32_t)&spi->reg->DR;
    spi->_rx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_TCIE;
  }
  if(spi->mosi) {
    spi->_tx_dma.mux->CCR &= 0xFFFFFFC0;
    SPI_DmaSetTxRequest(spi->reg, &spi->_tx_dma);
    GPIO_InitAlternate(&SPI_MOSI_MAP[spi->mosi], false);
    spi->_tx_dma.cha->CPAR = (uint32_t)&spi->reg->DR;
    spi->_tx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_DIR;
  }
  uint32_t cr2 = SPI_CR2_RXDMAEN | SPI_CR2_FRXTH | SPI_CR2_SSOE | 0x00000700;
  uint32_t cr1 = SPI_CR1_MSTR | (spi->lsb << 7) | (spi->prescaler << 3) | SPI_CR1_SPE | (spi->cpol << 1) | spi->cpha;
  if(spi->cs) {
    spi->cs->mode = GPIO_Mode_Output;
    GPIO_Init(spi->cs);
  }
  else {
    cr2 |= SPI_CR1_SSM | SPI_CR1_SSI;
  }
  if(spi->mosi) cr2 |= SPI_CR2_TXDMAEN;
  else cr1 |= SPI_CR1_RXONLY;
  RCC_EnableSPI(spi->reg);
  spi->reg->CR2 = cr2;
  spi->reg->CR1 = cr1;
}

//-------------------------------------------------------------------------------------------------

bool SPI_Master_IsBusy(SPI_Master_t *spi)
{
  return spi->_busy;
}

bool SPI_Master_IsFree(SPI_Master_t *spi)
{
  return !spi->_busy;
}

//-------------------------------------------------------------------------------------------------

static void SPI_Master_Start(SPI_Master_t *spi)
{
  spi->_tx_dma.cha->CCR &= ~DMA_CCR_EN;
  spi->_rx_dma.cha->CCR &= ~DMA_CCR_EN;
}

static void SPI_Master_End(SPI_Master_t *spi, uint16_t len)
{
  spi->_tx_dma.cha->CNDTR = len;
  spi->_rx_dma.cha->CNDTR = len;
  if(spi->cs) {
    GPIO_Set(spi->cs);
    SPI_Delay(spi->cs_delay);
  }
  spi->_rx_dma.cha->CCR |= DMA_CCR_EN;
  spi->_tx_dma.cha->CCR |= DMA_CCR_EN;
  spi->_busy = true;
}

//-------------------------------------------------------------------------------------------------

status_t SPI_Master_Transfer(SPI_Master_t *spi, uint8_t *rx_data, uint8_t *tx_data, uint16_t len)
{
  if(spi->_busy) return BUSY;
  SPI_Master_Start(spi);
  spi->_tx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->_rx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->_tx_dma.cha->CMAR = (uint32_t)tx_data;
  spi->_rx_dma.cha->CMAR = (uint32_t)rx_data;
  SPI_Master_End(spi, len);
  return FREE;
}

void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_data, uint16_t len)
{
  spi->_rx_dma.cha->CCR &= ~DMA_CCR_EN;
  spi->_rx_dma.cha->CMAR = (uint32_t)rx_data;
  spi->_rx_dma.cha->CNDTR = len;
  spi->_rx_dma.cha->CCR |= DMA_CCR_EN;
  spi->_busy = true;
  spi->reg->CR1 |= SPI_CR1_SPE;
}

status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t cmd, uint8_t *rx_data, uint16_t len)
{
  if(spi->_busy) return BUSY;
  spi->_const_byte = cmd;
  if(!spi->mosi) {
    SPI_Master_OnlyRead(spi, rx_data, len);
    return FREE;
  }
  SPI_Master_Start(spi);
  spi->_tx_dma.cha->CCR &= ~DMA_CCR_MINC;
  spi->_rx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->_tx_dma.cha->CMAR = (uint32_t)&spi->_const_byte;
  spi->_rx_dma.cha->CMAR = (uint32_t)rx_data;
  SPI_Master_End(spi, len);
  return FREE;
}

status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_data, uint16_t len)
{
  if(spi->_busy) return BUSY;
  SPI_Master_Start(spi);
  spi->_tx_dma.cha->CCR |= DMA_CCR_MINC;
  spi->_rx_dma.cha->CCR &= ~DMA_CCR_MINC;
  spi->_tx_dma.cha->CMAR = (uint32_t)tx_data;
  spi->_rx_dma.cha->CMAR = (uint32_t)&spi->_const_byte;
  SPI_Master_End(spi, len);
  return FREE;
}

//-------------------------------------------------------------------------------------------------

#if(SPI_SOFTWARE_ENABLE)

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

static void SPI_Software_WriteBit(GPIO_t *mosi, uint8_t *byte)
{
  #if(SPI_SOFTWARE_LSB)
    if(*byte & 1) GPIO_Set(mosi);
    else GPIO_Rst(mosi);
    *byte >>= 1;
  #else
    if(*byte & 0x80) GPIO_Set(mosi);
    else GPIO_Rst(mosi);
    *byte <<= 1;
  #endif
}

static void SPI_Software_ReadBit(GPIO_t *miso, uint8_t *byte)
{
  #if(SPI_SOFTWARE_LSB)
    *byte >>= 1;
    if(GPIO_In(miso)) *byte |= 0x80;
  #else
    *byte <<= 1;
    if(GPIO_In(miso)) *byte |= 1;
  #endif
}

void SPI_Software_Transfer(SPI_Software_t *spi, uint8_t *rx_data, uint8_t *tx_data, uint16_t len)
{
  #if(SPI_SOFTWARE_CPOL)
    GPIO_Set(spi->sck);
  #else
    GPIO_Rst(spi->sck);
  #endif
  GPIO_Set(spi->cs);
  for(uint32_t i = 0; i < spi->delay; i++) __NOP();
  for(uint16_t i = 0; i < len; i++) {
    uint8_t tx_byte = tx_data[i];
    uint8_t rx_byte = 0;
    for(uint8_t j = 0; j < 8; j++) {
      GPIO_Tgl(spi->sck);
      __NOP();
      #if(!SPI_SOFTWARE_CPHA)
        SPI_Software_WriteBit(spi->mosi, &tx_byte);
        SPI_Software_ReadBit(spi->miso, &rx_byte);
      #endif
      GPIO_Tgl(spi->sck);
      __NOP();
      #if(SPI_SOFTWARE_CPHA)
        SPI_Software_WriteBit(spi->mosi, &tx_byte);
        SPI_Software_ReadBit(spi->miso, &rx_byte);
      #endif
    }
    rx_data[i] = rx_byte;
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
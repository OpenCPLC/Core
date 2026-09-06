// hal/stm32/itf/spi_master.c

#include "spi_master.h"

#include "vrts.h"

//-------------------------------------------------------------------------------------------- Init

// RX transfer complete ends every transfer: the last byte in is the last byte out
static void dma_handler(SPI_Master_t *spi)
{
  if(spi->_rx_dma.reg->ISR & DMA_ISR_TCIF(spi->_rx_dma.pos)) {
    spi->_rx_dma.reg->IFCR |= DMA_ISR_TCIF(spi->_rx_dma.pos);
    if(spi->cs) GPIO_Rst(spi->cs);
    spi->_busy = false;
    // `RXONLY` keeps clocking while enabled, so the peripheral is stopped between reads
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
    spi->_rx_dma.mux->CCR &= ~0x3Fu;
    SPI_DmaSetRxRequest(spi->reg, &spi->_rx_dma);
    IRQ_EnableDMA(spi->rx_dma, spi->irq_priority, (IRQ_Handler_t)dma_handler, spi);
    GPIO_InitAlternate(&SPI_MISO_MAP[spi->miso], false);
    spi->_rx_dma.cha->CPAR = (uint32_t)&spi->reg->DR;
    spi->_rx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_TCIE;
  }
  if(spi->mosi) {
    spi->_tx_dma.mux->CCR &= ~0x3Fu;
    SPI_DmaSetTxRequest(spi->reg, &spi->_tx_dma);
    GPIO_InitAlternate(&SPI_MOSI_MAP[spi->mosi], false);
    spi->_tx_dma.cha->CPAR = (uint32_t)&spi->reg->DR;
    spi->_tx_dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_DIR;
  }
  // 8-bit frames (`DS` = 7), RX threshold at one byte, NSS output
  uint32_t cr2 = SPI_CR2_RXDMAEN | SPI_CR2_FRXTH | SPI_CR2_SSOE | (7u << SPI_CR2_DS_Pos);
  uint32_t cr1 = SPI_CR1_MSTR | (spi->lsb << SPI_CR1_LSBFIRST_Pos) |
    (spi->prescaler << SPI_CR1_BR_Pos) | SPI_CR1_SPE |
    (spi->cpol << SPI_CR1_CPOL_Pos) | (spi->cpha << SPI_CR1_CPHA_Pos);
  // Without a chip select GPIO the hardware NSS output (`SSOE`) drives the line
  if(spi->cs) {
    spi->cs->mode = GPIO_Mode_Output;
    GPIO_Init(spi->cs);
  }
  if(spi->mosi) cr2 |= SPI_CR2_TXDMAEN;
  else cr1 |= SPI_CR1_RXONLY;
  RCC_EnableSPI(spi->reg);
  spi->reg->CR2 = cr2;
  spi->reg->CR1 = cr1;
}

bool SPI_Master_IsBusy(SPI_Master_t *spi) { return spi->_busy; }
bool SPI_Master_IsFree(SPI_Master_t *spi) { return !spi->_busy; }

//---------------------------------------------------------------------------------------- Transfer

// Both channels stopped, then pointed at their buffers with or without increment
static void dma_setup(SPI_Master_t *spi, uint8_t *tx, bool tx_inc, uint8_t *rx, bool rx_inc)
{
  spi->_tx_dma.cha->CCR &= ~DMA_CCR_EN;
  spi->_rx_dma.cha->CCR &= ~DMA_CCR_EN;
  if(tx_inc) spi->_tx_dma.cha->CCR |= DMA_CCR_MINC;
  else spi->_tx_dma.cha->CCR &= ~DMA_CCR_MINC;
  if(rx_inc) spi->_rx_dma.cha->CCR |= DMA_CCR_MINC;
  else spi->_rx_dma.cha->CCR &= ~DMA_CCR_MINC;
  spi->_tx_dma.cha->CMAR = (uint32_t)tx;
  spi->_rx_dma.cha->CMAR = (uint32_t)rx;
}

// Chip select, then both channels running: RX first, the TX request starts the clock
static void dma_start(SPI_Master_t *spi, uint16_t len)
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

status_t SPI_Master_Transfer(SPI_Master_t *spi, uint8_t *rx_data, uint8_t *tx_data,
  uint16_t len)
{
  if(spi->_busy) return BUSY;
  dma_setup(spi, tx_data, true, rx_data, true);
  dma_start(spi, len);
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
  dma_setup(spi, &spi->_const_byte, false, rx_data, true);
  dma_start(spi, len);
  return FREE;
}

status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_data, uint16_t len)
{
  if(spi->_busy) return BUSY;
  dma_setup(spi, tx_data, true, &spi->_const_byte, false);
  dma_start(spi, len);
  return FREE;
}

//---------------------------------------------------------------------------------------- Software
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

static void write_bit(GPIO_t *mosi, uint8_t *byte)
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

static void read_bit(GPIO_t *miso, uint8_t *byte)
{
  #if(SPI_SOFTWARE_LSB)
  *byte >>= 1;
  if(GPIO_In(miso)) *byte |= 0x80;
  #else
  *byte <<= 1;
  if(GPIO_In(miso)) *byte |= 1;
  #endif
}

void SPI_Software_Transfer(SPI_Software_t *spi, uint8_t *rx_data, uint8_t *tx_data,
  uint16_t len)
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
      write_bit(spi->mosi, &tx_byte);
      read_bit(spi->miso, &rx_byte);
      #endif
      GPIO_Tgl(spi->sck);
      __NOP();
      #if(SPI_SOFTWARE_CPHA)
      write_bit(spi->mosi, &tx_byte);
      read_bit(spi->miso, &rx_byte);
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

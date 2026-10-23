// hal/host/itf/spi_master.c

#include "spi_master.h"

#include <string.h>

#include "vrts.h"

SPI_TypeDef HostSpi[3];

//---------------------------------------------------------------------------------------- Internal

// Bytes answered by the attached device, the bus busy meanwhile; an empty bus reads `0xFF`
static void answer(SPI_Master_t *spi, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len)
{
  spi->_busy = true;
  if(spi->_Device) spi->_Device(spi->_ctx, tx_data, rx_data, len);
  else if(rx_data) memset(rx_data, 0xFF, len);
  spi->_busy = false;
}

// Chip select set and released around the bytes, as the chip drives it
static void transfer(SPI_Master_t *spi, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len)
{
  if(spi->cs) {
    GPIO_Set(spi->cs);
    SPI_Delay(spi->cs_delay);
  }
  answer(spi, tx_data, rx_data, len);
  if(spi->cs) GPIO_Rst(spi->cs);
}

//--------------------------------------------------------------------------------------------- API

void SPI_Master_Init(SPI_Master_t *spi)
{
  if(!spi->cs) return;
  spi->cs->mode = GPIO_Mode_Output;
  GPIO_Init(spi->cs);
}

bool SPI_Master_IsBusy(SPI_Master_t *spi) { return spi->_busy; }
bool SPI_Master_IsFree(SPI_Master_t *spi) { return !spi->_busy; }

status_t SPI_Master_Transfer(SPI_Master_t *spi, uint8_t *rx_data, uint8_t *tx_data,
  uint16_t len)
{
  if(spi->_busy) return BUSY;
  transfer(spi, tx_data, rx_data, len);
  return FREE;
}

status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t cmd, uint8_t *rx_data, uint16_t len)
{
  if(spi->_busy) return BUSY;
  if(!spi->mosi) {
    SPI_Master_OnlyRead(spi, rx_data, len);
    return FREE;
  }
  // `cmd` goes out with every byte, from the buffer the bytes come back into
  memset(rx_data, cmd, len);
  transfer(spi, rx_data, rx_data, len);
  return FREE;
}

status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_data, uint16_t len)
{
  if(spi->_busy) return BUSY;
  transfer(spi, tx_data, NULL, len);
  return FREE;
}

// As on the chip: chip select left as it is for the bytes, released once they are in
void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_data, uint16_t len)
{
  answer(spi, NULL, rx_data, len);
  if(spi->cs) GPIO_Rst(spi->cs);
}

//-------------------------------------------------------------------------------------------- Host

void SPI_HostAttach(SPI_Master_t *spi, SPI_HostDevice_t Device, void *ctx)
{
  spi->_Device = Device;
  spi->_ctx = ctx;
}

//-------------------------------------------------------------------------------------------------

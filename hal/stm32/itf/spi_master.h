// hal/stm32/itf/spi_master.h

#ifndef SPI_MASTER_H_
#define SPI_MASTER_H_

#include "irq.h"
#include "dma.h"
#include "spi.h"
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef SPI_SOFTWARE_ENABLE
  // Bit-bang SPI on plain GPIO, `SPI_Software_t`
  #define SPI_SOFTWARE_ENABLE 0
#endif

#ifndef SPI_Delay
  // Wait between chip select and the first clock, `cs_delay` argument
  #define SPI_Delay(x) delay(x)
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief SPI master, every transfer runs on DMA.
 * @param[in] reg SPI peripheral registers
 * @param[in] tx_dma TX DMA channel
 * @param[in] rx_dma RX DMA channel
 * @param[in] irq_priority Interrupt priority for DMA
 * @param[in] sck SCK pin mapping
 * @param[in] miso MISO pin mapping, `SPI_MISO_None` = transmit only
 * @param[in] mosi MOSI pin mapping, `SPI_MOSI_None` = receive only
 * @param[in] cs Chip select GPIO, `NULL` = hardware NSS
 * @param[in] cs_delay Wait after chip select, `SPI_Delay` units
 * @param[in] prescaler SPI clock prescaler
 * @param[in] lsb LSB-first mode, `false` = MSB first
 * @param[in] cpol Clock polarity
 * @param[in] cpha Clock phase
 * Internal:
 * @param _tx_dma TX DMA register set
 * @param _rx_dma RX DMA register set
 * @param _const_byte Byte repeated on MOSI during a read, sink of RX during a write
 * @param _busy Transfer in progress flag
 */
typedef struct {
  SPI_TypeDef *reg;
  DMA_CHx_t tx_dma;
  DMA_CHx_t rx_dma;
  IRQ_Priority_t irq_priority;
  SPI_SCK_t sck;
  SPI_MISO_t miso;
  SPI_MOSI_t mosi;
  GPIO_t *cs;
  uint32_t cs_delay;
  SPI_Prescaler_t prescaler;
  bool lsb;
  bool cpol;
  bool cpha;
  // internal
  DMA_t _tx_dma;
  DMA_t _rx_dma;
  uint8_t _const_byte;
  volatile bool _busy;
} SPI_Master_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize the peripheral, pins and DMA.
 * @param[in,out] spi Pointer to SPI master structure
 */
void SPI_Master_Init(SPI_Master_t *spi);

bool SPI_Master_IsBusy(SPI_Master_t *spi);
bool SPI_Master_IsFree(SPI_Master_t *spi);

/**
 * @brief Full-duplex transfer, both buffers stay valid until it ends.
 * @param[in,out] spi Pointer to SPI master structure
 * @param[out] rx_data Receive buffer
 * @param[in] tx_data Bytes to send
 * @param[in] len Number of bytes
 * @return `FREE` if started, `BUSY` if a transfer is in progress
 */
status_t SPI_Master_Transfer(SPI_Master_t *spi, uint8_t *rx_data, uint8_t *tx_data,
  uint16_t len);

/**
 * @brief Read `len` bytes while `cmd` is repeated on MOSI, a register address or a filler.
 * @param[in,out] spi Pointer to SPI master structure
 * @param[in] cmd Byte repeated for every received byte
 * @param[out] rx_data Receive buffer
 * @param[in] len Number of bytes
 * @return `FREE` if started, `BUSY` if a transfer is in progress
 */
status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t cmd, uint8_t *rx_data, uint16_t len);

/**
 * @brief Write `len` bytes, the received ones are dropped.
 * @param[in,out] spi Pointer to SPI master structure
 * @param[in] tx_data Bytes to send
 * @param[in] len Number of bytes
 * @return `FREE` if started, `BUSY` if a transfer is in progress
 */
status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_data, uint16_t len);

/**
 * @brief Receive-only transfer, the peripheral clocks in `RXONLY` mode without MOSI.
 * @param[in,out] spi Pointer to SPI master structure
 * @param[out] rx_data Receive buffer
 * @param[in] len Number of bytes
 */
void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_data, uint16_t len);

//---------------------------------------------------------------------------------------- Software
#if(SPI_SOFTWARE_ENABLE)

#ifndef SPI_SOFTWARE_LSB
  #define SPI_SOFTWARE_LSB 0
#endif
#ifndef SPI_SOFTWARE_CPOL
  #define SPI_SOFTWARE_CPOL 0
#endif
#ifndef SPI_SOFTWARE_CPHA
  #define SPI_SOFTWARE_CPHA 1
#endif

/**
 * @brief Bit-bang SPI master on four GPIO lines, blocking.
 * @param[in] cs Chip select
 * @param[in] sck Clock
 * @param[in] miso Input
 * @param[in] mosi Output
 * @param[in] delay Wait after chip select [NOP cycles]
 */
typedef struct {
  GPIO_t *cs;
  GPIO_t *sck;
  GPIO_t *miso;
  GPIO_t *mosi;
  uint32_t delay;
} SPI_Software_t;

/**
 * @brief Initialize the four lines and park the bus idle.
 * @param[in,out] spi Pointer to software SPI structure
 */
void SPI_Software_Init(SPI_Software_t *spi);

/**
 * @brief Full-duplex transfer, done when the call returns.
 * @param[in,out] spi Pointer to software SPI structure
 * @param[out] rx_data Receive buffer
 * @param[in] tx_data Bytes to send
 * @param[in] len Number of bytes
 */
void SPI_Software_Transfer(SPI_Software_t *spi, uint8_t *rx_data, uint8_t *tx_data,
  uint16_t len);

#endif
//-------------------------------------------------------------------------------------------------
#endif

// hal/stm32/spi_master.h

#ifndef SPI_MASTER_H_
#define SPI_MASTER_H_

#include "irq.h"
#include "dma.h"
#include "spi.h"
#include "xdef.h"
#include "main.h"

#ifndef SPI_SOFTWARE_ENABLE
  #define SPI_SOFTWARE_ENABLE 0
#endif

#ifndef SPI_Delay
  #define SPI_Delay(x) delay(x)
#endif

//-------------------------------------------------------------------------------------------------

/**
 * @brief SPI master control structure with DMA.
 * @param[in] reg Pointer to SPI peripheral registers
 * @param[in] tx_dma TX DMA channel number
 * @param[in] rx_dma RX DMA channel number
 * @param[in] irq_priority Interrupt priority for DMA
 * @param[in] sck SCK pin mapping enum value
 * @param[in] miso MISO pin mapping (`SPI_MISO_None` = TX-only)
 * @param[in] mosi MOSI pin mapping (`SPI_MOSI_None` = RX-only)
 * @param[in] cs Pointer to CS GPIO (`NULL` = hardware NSS)
 * @param[in] cs_delay CS setup delay in ticks
 * @param[in] prescaler SPI clock prescaler
 * @param[in] lsb LSB-first mode (`false` = MSB first)
 * @param[in] cpol Clock polarity
 * @param[in] cpha Clock phase
 * Internal:
 * @param _tx_dma TX DMA registers structure
 * @param _rx_dma RX DMA registers structure
 * @param _const_byte Constant byte for read operations
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

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize SPI master peripheral with DMA.
 * @param[in,out] spi Pointer to SPI master structure
 */
void SPI_Master_Init(SPI_Master_t *spi);

/**
 * @brief Check if SPI transfer is in progress.
 * @param[in] spi Pointer to SPI master structure
 * @return `true` if busy
 */
bool SPI_Master_IsBusy(SPI_Master_t *spi);

/**
 * @brief Check if SPI is ready for new transfer.
 * @param[in] spi Pointer to SPI master structure
 * @return `true` if free
 */
bool SPI_Master_IsFree(SPI_Master_t *spi);

/**
 * @brief Full-duplex SPI transfer.
 * @param[in,out] spi Pointer to SPI master structure
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] tx_data Pointer to transmit buffer
 * @param[in] len Number of bytes to transfer
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t SPI_Master_Transfer(SPI_Master_t *spi, uint8_t *rx_data, uint8_t *tx_data, uint16_t len);

/**
 * @brief Read from SPI device (sends constant `cmd` byte).
 * @param[in,out] spi Pointer to SPI master structure
 * @param[in] cmd Constant byte to send (e.g. register address)
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] len Number of bytes to read
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t cmd, uint8_t *rx_data, uint16_t len);

/**
 * @brief Write to SPI device.
 * @param[in,out] spi Pointer to SPI master structure
 * @param[in] tx_data Pointer to transmit buffer
 * @param[in] len Number of bytes to write
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_data, uint16_t len);

/**
 * @brief Start RX-only transfer (for `RXONLY` mode without MOSI).
 * @param[in,out] spi Pointer to SPI master structure
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] len Number of bytes to read
 */
void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_data, uint16_t len);

//-------------------------------------------------------------------------------------------------
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
 * @brief Software (bit-bang) SPI control structure.
 * @param[in] cs Pointer to CS GPIO
 * @param[in] sck Pointer to SCK GPIO
 * @param[in] miso Pointer to MISO GPIO
 * @param[in] mosi Pointer to MOSI GPIO
 * @param[in] delay Bit delay in NOP cycles
 */
typedef struct {
  GPIO_t *cs;
  GPIO_t *sck;
  GPIO_t *miso;
  GPIO_t *mosi;
  uint32_t delay;
} SPI_Software_t;

/**
 * @brief Initialize software SPI.
 * @param[in,out] spi Pointer to software SPI structure
 */
void SPI_Software_Init(SPI_Software_t *spi);

/**
 * @brief Full-duplex software SPI transfer.
 * @param[in,out] spi Pointer to software SPI structure
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] tx_data Pointer to transmit buffer
 * @param[in] len Number of bytes to transfer
 */
void SPI_Software_Transfer(SPI_Software_t *spi, uint8_t *rx_data, uint8_t *tx_data, uint16_t len);

#endif
//-------------------------------------------------------------------------------------------------
#endif
// hal/host/itf/spi_master.h

// Contract of the SPI master a host build compiles against, with the bus modelled in memory:
// a transfer ends inside the call that starts it, answered by the device a test attaches,
// with chip select driven as the chip drives it.
// The fields of the chip are kept, so a board file builds as it is;
// the model reads `mosi`, `cs` and `cs_delay` of them.

#ifndef SPI_MASTER_H_
#define SPI_MASTER_H_

#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "gpio.h"
#include "irq.h"
#include "dma.h"
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef SPI_Delay
  // Wait between chip select and the first clock, `cs_delay` argument
  #define SPI_Delay(x) delay(x)
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  SPI_Prescaler_2   = 0,
  SPI_Prescaler_4   = 1,
  SPI_Prescaler_8   = 2,
  SPI_Prescaler_16  = 3,
  SPI_Prescaler_32  = 4,
  SPI_Prescaler_64  = 5,
  SPI_Prescaler_128 = 6,
  SPI_Prescaler_256 = 7
} SPI_Prescaler_t;

/**
 * @brief Devices on the bus, one handler for every transfer.
 *   Byte `i` of `tx_data` goes out before byte `i` of `rx_data` comes in,
 *   so the two may be one buffer.
 * @param[in] ctx Context given to `SPI_HostAttach`
 * @param[in] tx_data Bytes on MOSI, `NULL` when the master drives none
 * @param[out] rx_data Room for the bytes on MISO, `NULL` when the master drops them
 * @param[in] len Number of bytes
 */
typedef void (*SPI_HostDevice_t)(void *ctx, const uint8_t *tx_data, uint8_t *rx_data,
  uint16_t len);

//---------------------------------------------------------------------------------- Family include

// Pin names of the family of the chip the build stands for
#if defined(STM32WB55xx)
  #include "../../stm32wb/itf/spi_wb.h"
#else
  #include "../../stm32g0/itf/spi_g0.h"
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief SPI master over a modelled bus.
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
 * @param _busy Transfer under way, the device answering it
 * @param _Device Devices of the bus, `NULL` = none, every byte read as `0xFF`
 * @param _ctx Context of `_Device`
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
  bool _busy;
  SPI_HostDevice_t _Device;
  void *_ctx;
} SPI_Master_t;

//--------------------------------------------------------------------------------------------- API

void SPI_Master_Init(SPI_Master_t *spi);
bool SPI_Master_IsBusy(SPI_Master_t *spi);
bool SPI_Master_IsFree(SPI_Master_t *spi);

status_t SPI_Master_Transfer(SPI_Master_t *spi, uint8_t *rx_data, uint8_t *tx_data,
  uint16_t len);
status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t cmd, uint8_t *rx_data, uint16_t len);
status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_data, uint16_t len);
void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_data, uint16_t len);

//-------------------------------------------------------------------------------------------- Host

/**
 * @brief Put devices on the bus.
 * @param[in,out] spi Bus
 * @param[in] Device Handler answering every transfer, `NULL` empties the bus
 * @param[in] ctx Context passed to `Device`
 */
void SPI_HostAttach(SPI_Master_t *spi, SPI_HostDevice_t Device, void *ctx);

//-------------------------------------------------------------------------------------------------
#endif

// hal/host/itf/i2c_master.h

// Contract of the I2C master a host build compiles against, with the bus modelled in memory:
// a transfer ends inside the call that starts it, answered by the device a test attaches.
// The fields and presets of the chip are kept, so a board file builds as it is;
// the model reads none of them.

#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_

#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "irq.h"
#include "dma.h"
#include "xdef.h"
#include "main.h"

//--------------------------------------------------------------------------------------- Constants

// Longest single transfer, as on the chip
#define I2C_TRANSFER_MAX 255

//----------------------------------------------------------------------------------------- Presets

// Speeds of the chip presets, every one of them: a modelled bus has no clock to time
#define I2C_TIMING_100kHz timing = 0, .filter = 0
#define I2C_TIMING_400kHz timing = 0, .filter = 0
#define I2C_TIMING_1MHz   timing = 0, .filter = 0

//------------------------------------------------------------------------------------------- Types

/**
 * @brief Devices on the bus, one handler answering every address.
 *   A write-read brings both phases in one call, joined by the repeated start.
 * @param[in] ctx Context given to `I2C_HostAttach`
 * @param[in] addr 7-bit device address
 * @param[in] tx_data Bytes written, `NULL` for a read
 * @param[in] tx_len Number of bytes written
 * @param[out] rx_data Room for the bytes read, `NULL` for a write
 * @param[in] rx_len Number of bytes read
 * @return `true` when the device acknowledged every byte, `false` for a NACK
 */
typedef bool (*I2C_HostDevice_t)(void *ctx, uint8_t addr, const uint8_t *tx_data, uint16_t tx_len,
  uint8_t *rx_data, uint16_t rx_len);

//---------------------------------------------------------------------------------- Family include

// Pin names of the family of the chip the build stands for
#if defined(STM32WB55xx)
  #include "../../stm32wb/itf/i2c_wb.h"
#else
  #include "../../stm32g0/itf/i2c_g0.h"
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief I2C master over a modelled bus.
 * @param[in] reg I2C peripheral registers
 * @param[in] scl SCL pin mapping
 * @param[in] sda SDA pin mapping
 * @param[in] pull_up Enable internal pull-up resistors
 * @param[in] irq_priority Interrupt priority for I2C and DMA
 * @param[in] timing `TIMINGR` value, see the `I2C_TIMING_...` presets
 * @param[in] filter Digital noise filter coefficient, `0..15`
 * @param[in] tx_dma TX DMA channel, `DMA_None` = interrupt mode
 * @param[in] rx_dma RX DMA channel, `DMA_None` = interrupt mode
 * Internal:
 * @param _busy Transfer under way, or transfers refused after `I2C_Master_Disable`
 * @param _nack Last transfer was refused by the device
 * @param _Device Devices of the bus, `NULL` = none, every address refused
 * @param _ctx Context of `_Device`
 */
typedef struct {
  I2C_TypeDef *reg;
  I2C_SCL_t scl;
  I2C_SDA_t sda;
  bool pull_up;
  IRQ_Priority_t irq_priority;
  uint32_t timing;
  uint8_t filter;
  DMA_CHx_t tx_dma;
  DMA_CHx_t rx_dma;
  // internal
  bool _busy;
  bool _nack;
  I2C_HostDevice_t _Device;
  void *_ctx;
} I2C_Master_t;

//--------------------------------------------------------------------------------------------- API

void I2C_Master_Init(I2C_Master_t *i2c);
void I2C_Master_Disable(I2C_Master_t *i2c);
bool I2C_Master_IsBusy(I2C_Master_t *i2c);
bool I2C_Master_IsFree(I2C_Master_t *i2c);
bool I2C_Master_Nack(I2C_Master_t *i2c);

status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);
status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);
status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len);
status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len);
status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *tx_data,
  uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len);

//-------------------------------------------------------------------------------------------- Host

/**
 * @brief Put devices on the bus.
 * @param[in,out] i2c Bus
 * @param[in] Device Handler answering every transfer, `NULL` empties the bus
 * @param[in] ctx Context passed to `Device`
 */
void I2C_HostAttach(I2C_Master_t *i2c, I2C_HostDevice_t Device, void *ctx);

//-------------------------------------------------------------------------------------------------
#endif

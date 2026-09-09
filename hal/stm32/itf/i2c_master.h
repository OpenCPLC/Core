// hal/stm32/itf/i2c_master.h

#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_

#include "irq.h"
#include "dma.h"
#include "pwr.h"
#include "i2c.h"
#include "xdef.h"
#include "main.h"
#include <string.h>

//--------------------------------------------------------------------------------------- Constants

// Longest single transfer. `NBYTES` in `CR2` is 8 bits wide and this driver does not
// chain frames with `RELOAD`, so a larger length would spill into neighbouring fields
#define I2C_TRANSFER_MAX 255

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief I2C master, interrupt or DMA driven.
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
 * @param _tx_dma TX DMA register set
 * @param _rx_dma RX DMA register set
 * @param _busy Transfer in progress flag
 * @param _nack Last transfer was refused by the device
 * @param _tx_buffer Heap copy of register plus data behind `I2C_Master_WriteReg`
 * @param _tx_ptr TX data, interrupt mode
 * @param _rx_ptr RX data
 * @param _tail Byte index of the transfer, interrupt mode
 * @param _head Byte count of the transfer, interrupt mode
 * @param _addr Device address kept for the read phase
 * @param _size Read length pending after the write phase
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
  DMA_t _tx_dma;
  DMA_t _rx_dma;
  volatile bool _busy;
  volatile bool _nack;
  uint8_t *_tx_buffer;
  volatile uint8_t *_tx_ptr;
  uint8_t *_rx_ptr;
  volatile uint16_t _tail;
  uint16_t _head;
  uint8_t _addr;
  uint16_t _size;
} I2C_Master_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize the peripheral, pins, DMA and interrupts.
 * @param[in,out] i2c Pointer to I2C master structure
 */
void I2C_Master_Init(I2C_Master_t *i2c);

/**
 * @brief Disable the peripheral and gate its clock, transfers are refused from then on.
 * @param[in,out] i2c Pointer to I2C master structure
 */
void I2C_Master_Disable(I2C_Master_t *i2c);

bool I2C_Master_IsBusy(I2C_Master_t *i2c);
bool I2C_Master_IsFree(I2C_Master_t *i2c);

/**
 * @brief Whether the last transfer ended with a NACK from the device.
 *   `NACKF` on this peripheral means a NACK received by the master, so it marks
 *   an absent device or a byte the device refused. The NACK a master-receiver sends
 *   after its own last byte is a normal end of frame and never raises this.
 * @param[in] i2c Pointer to I2C master structure
 * @return `true` when the device refused the address or a data byte
 */
bool I2C_Master_Nack(I2C_Master_t *i2c);

/**
 * @brief Start a write, `data` must stay valid until the transfer ends.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] data Bytes to send
 * @param[in] len Number of bytes, up to `I2C_TRANSFER_MAX`
 * @return `FREE` if started, `BUSY` if a transfer is in progress, `ERR` on bad arguments
 */
status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Start a read into `data`.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[out] data Receive buffer
 * @param[in] len Number of bytes, up to `I2C_TRANSFER_MAX`
 * @return `FREE` if started, `BUSY` if a transfer is in progress, `ERR` on bad arguments
 */
status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Write a device register: the register address followed by `data`,
 *   copied to a heap block released when the transfer ends.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] reg Register address
 * @param[in] data Bytes to send after the register address
 * @param[in] len Number of bytes, below `I2C_TRANSFER_MAX`
 * @return `FREE` if started, `BUSY` if a transfer is in progress, `ERR` on bad arguments
 */
status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len);

/**
 * @brief Read a device register: the register address, a repeated start, then the read.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] reg Register address
 * @param[out] data Receive buffer
 * @param[in] len Number of bytes, up to `I2C_TRANSFER_MAX`
 * @return `FREE` if started, `BUSY` if a transfer is in progress, `ERR` on bad arguments
 */
status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg,
  uint8_t *data, uint16_t len);

/**
 * @brief Write then read in one transaction, joined by a repeated start.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] tx_data Bytes to send
 * @param[in] tx_len Number of bytes to send
 * @param[out] rx_data Receive buffer
 * @param[in] rx_len Number of bytes to read
 * @return `FREE` if started, `BUSY` if a transfer is in progress, `ERR` on bad arguments
 */
status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *tx_data,
  uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len);

//-------------------------------------------------------------------------------------------------
#endif

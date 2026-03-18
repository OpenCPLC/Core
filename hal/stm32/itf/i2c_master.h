// hal/stm32/i2c_master.h

#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_

#include <string.h>
#include "irq.h"
#include "dma.h"
#include "pwr.h"
#include "i2c.h"
#include "heap.h"
#include "xdef.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief I2C master control structure.
 * @param[in] reg Pointer to I2C peripheral registers
 * @param[in] scl SCL pin mapping enum value
 * @param[in] sda SDA pin mapping enum value
 * @param[in] pull_up Enable internal pull-up resistors
 * @param[in] irq_priority Interrupt priority for I2C and DMA
 * @param[in] timing I2C `TIMINGR` register value
 * @param[in] filter Digital noise filter coefficient (0-15)
 * @param[in] tx_dma TX DMA channel (`DMA_None` = interrupt mode)
 * @param[in] rx_dma RX DMA channel (`DMA_None` = interrupt mode)
 * Internal:
 * @param _tx_dma TX DMA registers structure
 * @param _rx_dma RX DMA registers structure
 * @param _busy Transfer in progress flag
 * @param _tx_buffer Allocated TX buffer for `WriteReg`
 * @param _tx_ptr Current TX pointer (interrupt mode)
 * @param _rx_ptr Current RX pointer
 * @param _tail Current byte index
 * @param _head Total byte count
 * @param _addr Stored device address for read phase
 * @param _size Pending read size
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
  uint8_t *_tx_buffer;
  volatile uint8_t *_tx_ptr;
  uint8_t *_rx_ptr;
  volatile uint16_t _tail;
  uint16_t _head;
  uint8_t _addr;
  uint16_t _size;
} I2C_Master_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize I2C master peripheral.
 * @param[in,out] i2c Pointer to I2C master structure
 */
void I2C_Master_Init(I2C_Master_t *i2c);

/**
 * @brief Disable I2C master peripheral.
 * @param[in,out] i2c Pointer to I2C master structure
 */
void I2C_Master_Disable(I2C_Master_t *i2c);

/**
 * @brief Check if I2C transfer is in progress.
 * @param[in] i2c Pointer to I2C master structure
 * @return `true` if busy
 */
bool I2C_Master_IsBusy(I2C_Master_t *i2c);

/**
 * @brief Check if I2C is ready for new transfer.
 * @param[in] i2c Pointer to I2C master structure
 * @return `true` if free
 */
bool I2C_Master_IsFree(I2C_Master_t *i2c);

/**
 * @brief Write data to I2C device.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to write
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Read data from I2C device.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[out] data Pointer to receive buffer
 * @param[in] len Number of bytes to read
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Write to device register (sends `reg` address + `data`).
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] reg Register address
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of data bytes
 * @return `FREE` if started, `BUSY`/`ERR` on failure
 */
status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

/**
 * @brief Read from device register (sends `reg` address, restarts, reads).
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] reg Register address
 * @param[out] data Pointer to receive buffer
 * @param[in] len Number of bytes to read
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

/**
 * @brief Combined write-then-read transaction.
 * @param[in,out] i2c Pointer to I2C master structure
 * @param[in] addr 7-bit device address
 * @param[in] tx_data Pointer to write data
 * @param[in] tx_len Number of bytes to write
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] rx_len Number of bytes to read
 * @return `FREE` if started, `BUSY` if transfer in progress
 */
status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len);

//-------------------------------------------------------------------------------------------------
#endif
// dvr/mem/m24x.h

#ifndef M24X_H_
#define M24X_H_

#include <stdint.h>
#include "gpio.h"
#include "i2c_master.h"

//--------------------------------------------------------------------------------------- Constants

#define M24X_ADDRESS 0x50  // chip enable pins low, block 0
#define M24X_WRITE_ms 10   // write cycle of a page, the longest of the family, M24M02

//----------------------------------------------------------------------------------------- Presets

// Size and page of the parts of ST; the parts of other makers give theirs in the same two fields
#define M24X_C01 size = 128,    .page_size = 16
#define M24X_C02 size = 256,    .page_size = 16
#define M24X_C04 size = 512,    .page_size = 16
#define M24X_C08 size = 1024,   .page_size = 16
#define M24X_C16 size = 2048,   .page_size = 16
#define M24X_C32 size = 4096,   .page_size = 32
#define M24X_C64 size = 8192,   .page_size = 32
#define M24X_128 size = 16384,  .page_size = 64
#define M24X_256 size = 32768,  .page_size = 64
#define M24X_512 size = 65536,  .page_size = 128
#define M24X_M01 size = 131072, .page_size = 256
#define M24X_M02 size = 262144, .page_size = 256

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Serial EEPROM of the M24 family on I2C, from M24C01 to M24M02.
 *   The address goes in one byte up to 2kB and in two past it,
 *   the bits above them in the device address, a block each.
 *   A write goes page by page, each part programmed before the next is sent,
 *   and only with `wc` set: the memory refuses data while `~WC` is high.
 *   One thread at a time, a write yields between its parts.
 * @param[in] i2c Bus the memory sits on
 * @param[in] size Memory [B], see the `M24X_...` presets
 * @param[in] page_size Page [B], a write wraps around inside it
 * @param[in] address Device address of block 0, chip enable pins in it (`0` = `M24X_ADDRESS`)
 * @param[in] wc `~WC` line, set while writing, `.reverse` for active low, `NULL` when tied low
 */
typedef struct {
  I2C_Master_t *i2c;
  uint32_t size;
  uint16_t page_size;
  uint8_t address;
  GPIO_t *wc;
} M24X_t;

//--------------------------------------------------------------------------------------------- API

// Take `~WC` as an output, writes off
void M24X_Init(M24X_t *m24x);

/**
 * @brief Read bytes from `addr` on.
 * @param[in] m24x Memory
 * @param[in] addr First byte
 * @param[out] data Receive buffer
 * @param[in] len Number of bytes, `addr + len` up to `size`
 * @return `OK`, `ERR` on bad arguments, a NACK or a timeout
 */
status_t M24X_Read(M24X_t *m24x, uint32_t addr, void *data, uint16_t len);

/**
 * @brief Write bytes from `addr` on, page by page, with `~WC` set from the first to the last.
 * @param[in] m24x Memory
 * @param[in] addr First byte
 * @param[in] data Bytes to write
 * @param[in] len Number of bytes, `addr + len` up to `size`
 * @return `OK` once the last part is programmed, `ERR` on bad arguments, a NACK or a timeout
 */
status_t M24X_Write(M24X_t *m24x, uint32_t addr, const void *data, uint16_t len);

//-------------------------------------------------------------------------------------------------
#endif

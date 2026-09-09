// hal/stm32/itf/i2c_slave.h

#ifndef I2C_SLAVE_H_
#define I2C_SLAVE_H_

#include "irq.h"
#include "i2c.h"
#include "xdef.h"
#include "main.h"
#include <string.h>

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief I2C slave exposing a byte register map.
 *   Every transaction starts at register `0`: the master writes or reads bytes upward,
 *   `write_mask` guards each register and `update_flag` marks the ones a write changed.
 * @param[in] reg I2C peripheral registers
 * @param[in] scl SCL pin mapping
 * @param[in] sda SDA pin mapping
 * @param[in] addr 7-bit slave address
 * @param[in,out] regmap Register map
 * @param[in] write_mask Per-register write permission, `true` = writable
 * @param[out] update_flag Per-register flag set when the master changed the value
 * @param[in] regmap_size Map size [B]
 * @param[in] sequence Sequence mode with a register pointer, not implemented
 * @param[in] pull_up Enable internal pull-up resistors
 * @param[in] irq_priority Interrupt priority
 * @param[in] timing `TIMINGR` value, see the `I2C_TIMING_...` presets
 * @param[in] filter Digital noise filter coefficient, `0..15`
 * Internal:
 * @param _updated Any register changed since `I2C_Slave_IsUpdate`
 * @param _idx Register of the byte in flight
 */
typedef struct {
  I2C_TypeDef *reg;
  I2C_SCL_t scl;
  I2C_SDA_t sda;
  uint8_t addr;
  uint8_t *regmap;
  bool *write_mask;
  bool *update_flag;
  uint16_t regmap_size;
  bool sequence;
  bool pull_up;
  IRQ_Priority_t irq_priority;
  uint32_t timing;
  uint8_t filter;
  // internal
  volatile bool _updated;
  volatile uint16_t _idx;
} I2C_Slave_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize the peripheral, pins and interrupt.
 * @param[in,out] i2c Pointer to I2C slave structure
 */
void I2C_Slave_Init(I2C_Slave_t *i2c);

/**
 * @brief Check if the master changed a register, the flag clears on read.
 * @param[in,out] i2c Pointer to I2C slave structure
 * @return `true` if any register was updated
 */
bool I2C_Slave_IsUpdate(I2C_Slave_t *i2c);

//-------------------------------------------------------------------------------------------------
#endif

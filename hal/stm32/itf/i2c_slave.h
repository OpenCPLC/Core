// hal/stm32/i2c_slave.h

#ifndef I2C_SLAVE_H_
#define I2C_SLAVE_H_

#include <string.h>
#include "irq.h"
#include "i2c.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief I2C slave control structure with register map interface.
 * @param[in] reg Pointer to I2C peripheral registers
 * @param[in] scl SCL pin mapping enum value
 * @param[in] sda SDA pin mapping enum value
 * @param[in] addr 7-bit slave address
 * @param[in,out] regmap Pointer to register map buffer
 * @param[in] write_mask Pointer to write permission mask (`true` = writable)
 * @param[out] update_flag Pointer to per-register update flags
 * @param[in] regmap_size Size of register map in bytes
 * @param[in] sequence Enable sequence mode (multi-byte with auto-increment)
 * @param[in] pull_up Enable internal pull-up resistors
 * @param[in] irq_priority Interrupt priority
 * @param[in] timing I2C `TIMINGR` register value
 * @param[in] filter Digital noise filter coefficient (0-15)
 * Internal:
 * @param _updated Global update flag
 * @param _idx Current register index
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

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize I2C slave peripheral.
 * @param[in,out] i2c Pointer to I2C slave structure
 */
void I2C_Slave_Init(I2C_Slave_t *i2c);

/**
 * @brief Check if register map was updated by master. Clears flag after reading.
 * @param[in,out] i2c Pointer to I2C slave structure
 * @return `true` if any register was updated
 */
bool I2C_Slave_IsUpdate(I2C_Slave_t *i2c);

//-------------------------------------------------------------------------------------------------

#endif
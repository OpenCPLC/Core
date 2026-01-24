#ifndef I2C_SLAVE_H_
#define I2C_SLAVE_H_

#include <string.h>
#include "irq.h"
#include "i2c.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief I2C slave control structure with regmap interface.
 * @param reg Pointer to I2C register base. [user]
 * @param scl_pin SCL pin mapping. [user]
 * @param sda_pin SDA pin mapping. [user]
 * @param addr 7-bit slave address. [user]
 * @param regmap Pointer to register map buffer. [user]
 * @param write_mask Pointer to write permission mask array. [user]
 * @param update_flag Pointer to per-register update flag array. [user]
 * @param regmap_size Size of register map in bytes. [user]
 * @param sequence Enable sequence mode (multi-byte transactions). [user]
 * @param pull_up Enable internal pull-up resistors. [user]
 * @param irq_priority Priority for I2C IRQ. [user]
 * @param timing I2C timing register value. [user]
 * @param filter Digital noise filter coefficient (0..15). [user]
 * @param update_global_flag Global update occurred flag. [internal]
 * @param i Current register index. [internal]
 */
typedef struct {
  I2C_TypeDef *reg;
  I2C_SCL_e scl_pin;
  I2C_SDA_e sda_pin;
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
  bool update_global_flag;
  uint16_t i;
} I2C_Slave_t;

//-------------------------------------------------------------------------------------------------

void I2C_Slave_Init(I2C_Slave_t *i2c);
bool I2C_Slave_IsUpdate(I2C_Slave_t *i2c);

//-------------------------------------------------------------------------------------------------

#endif
#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_

#include <string.h>
#include "irq.h"
#include "i2c.h"
#include "heap.h"
#include "xdef.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief I2C master control structure.
 * @param reg Pointer to I2C register base. [user]
 * @param scl_pin SCL pin mapping. [user]
 * @param sda_pin SDA pin mapping. [user]
 * @param pull_up Enable internal pull-up resistors. [user]
 * @param irq_priority Priority for I2C and DMA IRQ. [user]
 * @param timing I2C timing register value. [user]
 * @param filter Digital noise filter coefficient (0..15). [user]
 * @param tx_dma_nbr TX DMA channel number (0 = no DMA). [user]
 * @param rx_dma_nbr RX DMA channel number (0 = no DMA). [user]
 * @param tx_dma TX DMA control structure. [internal]
 * @param rx_dma RX DMA control structure. [internal]
 * @param busy Transfer in progress flag. [internal]
 * @param tx_buffer Allocated TX buffer pointer. [internal]
 * @param tx_location Current TX pointer for interrupt mode. [internal]
 * @param tail Current index for interrupt mode. [internal]
 * @param head Total count for interrupt mode. [internal]
 * @param address Target device address for read phase. [internal]
 * @param rx_location Pointer to RX data destination. [internal]
 * @param size Remaining bytes for read phase. [internal]
 */
typedef struct {
  I2C_TypeDef *reg;
  I2C_SCL_e scl_pin;
  I2C_SDA_e sda_pin;
  bool pull_up;
  IRQ_Priority_t irq_priority;
  uint32_t timing;
  uint8_t filter;
  DMA_Nbr_t tx_dma_nbr;
  DMA_Nbr_t rx_dma_nbr;
  DMA_t tx_dma;
  DMA_t rx_dma;
  volatile bool busy;
  uint8_t *tx_buffer;
  volatile uint8_t *tx_location;
  volatile uint16_t tail;
  uint16_t head;
  uint8_t address;
  uint8_t *rx_location;
  uint16_t size;
} I2C_Master_t;

//-------------------------------------------------------------------------------------------------

void I2C_Master_Init(I2C_Master_t *i2c);
void I2C_Master_Disable(I2C_Master_t *i2c);
bool I2C_Master_IsBusy(I2C_Master_t *i2c);
bool I2C_Master_IsFree(I2C_Master_t *i2c);

status_t I2C_Master_Write(I2C_Master_t *i2c, uint8_t addr, uint8_t *ary, uint16_t n);
status_t I2C_Master_Read(I2C_Master_t *i2c, uint8_t addr, uint8_t *ary, uint16_t n);
status_t I2C_Master_WriteReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *ary, uint16_t n);
status_t I2C_Master_ReadReg(I2C_Master_t *i2c, uint8_t addr, uint8_t reg, uint8_t *ary, uint16_t n);
status_t I2C_Master_WriteRead(I2C_Master_t *i2c, uint8_t addr, uint8_t *write_ary, uint16_t write_n, uint8_t *read_ary, uint16_t read_n);

//-------------------------------------------------------------------------------------------------

#endif
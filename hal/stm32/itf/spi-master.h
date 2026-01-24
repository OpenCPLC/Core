#ifndef SPI_MASTER_H_
#define SPI_MASTER_H_

#include "irq.h"
#include "spi.h"
#include "xdef.h"
#include "main.h"

#ifndef SPI_SOFTWARE_ENABLE
  #define SPI_SOFTWARE_ENABLE 0
#endif

#ifndef SPI_Delay
  #define SPI_Delay(value) delay(value)
#endif

//-------------------------------------------------------------------------------------------------

/**
 * @brief SPI master control structure with DMA.
 * @param reg Pointer to SPI register base. [user]
 * @param tx_dma_nbr TX DMA channel number. [user]
 * @param rx_dma_nbr RX DMA channel number. [user]
 * @param irq_priority Priority for SPI and DMA IRQ. [user]
 * @param sck_pin SCK pin mapping. [user]
 * @param miso_pin MISO pin mapping (0 for TX-only). [user]
 * @param mosi_pin MOSI pin mapping (0 for RX-only). [user]
 * @param cs_gpio Pointer to CS GPIO (NULL for hardware NSS). [user]
 * @param cs_delay CS setup delay in ticks. [user]
 * @param prescaler SPI clock prescaler. [user]
 * @param lsb LSB-first mode enable. [user]
 * @param cpol Clock polarity. [user]
 * @param cpha Clock phase. [user]
 * @param tx_dma TX DMA control structure. [internal]
 * @param rx_dma RX DMA control structure. [internal]
 * @param const_reg Constant byte for read operations. [internal]
 * @param busy_flag Transfer in progress flag. [internal]
 */
typedef struct {
  SPI_TypeDef *reg;
  DMA_Nbr_t tx_dma_nbr;
  DMA_Nbr_t rx_dma_nbr;
  IRQ_Priority_t irq_priority;
  SPI_SCK_t sck_pin;
  SPI_MISO_t miso_pin;
  SPI_MOSI_t mosi_pin;
  GPIO_t *cs_gpio;
  uint32_t cs_delay;
  SPI_Prescaler_t prescaler;
  bool lsb;
  bool cpol;
  bool cpha;
  DMA_t tx_dma;
  DMA_t rx_dma;
  uint8_t const_reg;
  volatile bool busy_flag;
} SPI_Master_t;

//-------------------------------------------------------------------------------------------------

void SPI_Master_Init(SPI_Master_t *spi);
bool SPI_Master_IsBusy(SPI_Master_t *spi);
bool SPI_Master_IsFree(SPI_Master_t *spi);

status_t SPI_Master_Run(SPI_Master_t *spi, uint8_t *rx_buff, uint8_t *tx_buff, uint16_t n);
status_t SPI_Master_Read(SPI_Master_t *spi, uint8_t addr, uint8_t *rx_buff, uint16_t n);
status_t SPI_Master_Write(SPI_Master_t *spi, uint8_t *tx_buff, uint16_t n);
void SPI_Master_OnlyRead(SPI_Master_t *spi, uint8_t *rx_buff, uint16_t n);

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

typedef struct {
  GPIO_t *cs;
  GPIO_t *sck;
  GPIO_t *miso;
  GPIO_t *mosi;
  uint32_t delay;
} SPI_Software_t;

void SPI_Software_Init(SPI_Software_t *spi);
void SPI_Software_Run(SPI_Software_t *spi, uint8_t *rx_buff, uint8_t *tx_buff, uint16_t n);

#endif

//-------------------------------------------------------------------------------------------------

#endif
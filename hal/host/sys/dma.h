// hal/host/sys/dma.h

// Contract of the DMA controller a host build compiles against: the types, enums, fields
// and prototypes firmware is written to. Implementations are inert, a host has no DMA.

#ifndef DMA_H_
#define DMA_H_

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

//------------------------------------------------------------------------------------------- Types

// Channel a peripheral is wired to. `DMA_None` leaves the peripheral unwired
typedef enum {
  DMA_None = 0,
  DMA_CH1 = 1,
  DMA_CH2 = 2,
  DMA_CH3 = 3,
  DMA_CH4 = 4,
  DMA_CH5 = 5,
  DMA_CH6 = 6,
  DMA_CH7 = 7
} DMA_CHx_t;

/**
 * @brief Resolved channel of a peripheral.
 * @param[in] reg Controller the channel belongs to
 * @param[in] cha Channel registers
 * @param[in] mux Request multiplexer of the channel
 * @param[in] pos Channel index inside the controller
 */
typedef struct {
  DMA_TypeDef *reg;
  DMA_Channel_TypeDef *cha;
  DMAMUX_Channel_TypeDef *mux;
  uint8_t pos;
} DMA_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Resolve a channel selector to its register set.
 * @param[in] channel Channel selector
 * @param[out] dma Descriptor to fill
 */
void DMA_SetRegisters(DMA_CHx_t channel, DMA_t *dma);

// Drop the transfer flags of a channel
void DMA_ClearFlags(DMA_t *dma);

//-------------------------------------------------------------------------------------------------
#endif

// hal/stm32/sys/dma.h

#ifndef DMA_H_
#define DMA_H_

#include <stdint.h>

//---------------------------------------------------------------------------------- Family include

#if defined(STM32G0)
  #include "stm32g0xx.h"
  #include "dma_g0.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
  #include "dma_wb.h"
#endif

//------------------------------------------------------------------------------------------- Types

// Register set of one channel, resolved from its number by `DMA_SetRegisters`
typedef struct {
  DMA_TypeDef *reg;
  DMA_Channel_TypeDef *cha;
  DMAMUX_Channel_TypeDef *mux;
  uint8_t pos;
} DMA_t;

//------------------------------------------------------------------------------------------- Flags

// Interrupt flags of channel `pos`, four bits per channel

#define DMA_ISR_GIF(pos)  (DMA_ISR_GIF1  << ((pos) * 4))
#define DMA_ISR_TCIF(pos) (DMA_ISR_TCIF1 << ((pos) * 4))
#define DMA_ISR_HTIF(pos) (DMA_ISR_HTIF1 << ((pos) * 4))
#define DMA_ISR_TEIF(pos) (DMA_ISR_TEIF1 << ((pos) * 4))

//--------------------------------------------------------------------------------------------- API

// Resolve channel `nbr` into `dma`
void DMA_SetRegisters(DMA_CHx_t nbr, DMA_t *dma);

// Drop every interrupt flag of the channel
void DMA_ClearFlags(DMA_t *dma);

//-------------------------------------------------------------------------------------------------
#endif

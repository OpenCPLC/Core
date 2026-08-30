// hal/host/sys/dma.c

#include "dma.h"
#include "xdef.h"

//-------------------------------------------------------------------------------------------------

/**
 * Nothing moves data on its own off-target. A descriptor is filled with the null register
 * set and the flag drop is accepted, so code that wires a peripheral to a channel builds
 * and runs; the destination buffer is written by whoever stands in for the transfer.
 */

void DMA_SetRegisters(DMA_t *dma, DMA_CHx_t channel)
{
  if(!dma) return;
  dma->reg = NULL;
  dma->cha = NULL;
  dma->mux = NULL;
  dma->pos = (uint8_t)channel;
}

void DMA_ClearFlags(DMA_t *dma)
{
  unused(dma);
}

// hal/stm32wb/sys/dma_wb.c

#include "dma.h"

//------------------------------------------------------------------------------------------------- DMA helpers

void DMA_SetRegisters(DMA_CHx_t nbr, DMA_t *dma)
{
  if(nbr >= 1 && nbr <= 7) {
    dma->reg = DMA1;
    dma->cha = (DMA_Channel_TypeDef *)((uint32_t)DMA1_Channel1 + (nbr - 1) * 0x14);
    dma->mux = (DMAMUX_Channel_TypeDef *)((uint32_t)DMAMUX1_Channel0 + (nbr - 1) * 4);
    dma->pos = nbr - 1;
  }
  else if(nbr >= 8 && nbr <= 14) {
    dma->reg = DMA2;
    dma->cha = (DMA_Channel_TypeDef *)((uint32_t)DMA2_Channel1 + (nbr - 8) * 0x14);
    dma->mux = (DMAMUX_Channel_TypeDef *)((uint32_t)DMAMUX1_Channel7 + (nbr - 8) * 4);
    dma->pos = nbr - 8;
  }
}

void DMA_ClearFlags(DMA_t *dma)
{
  dma->reg->IFCR = 0x0F << (dma->pos * 4);
}

//-------------------------------------------------------------------------------------------------

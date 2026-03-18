// hal/stm32wb/adc_wb.c

#include "adc.h"
#include "dma.h"

//--------------------------------------------------------------------------------------- Const

const uint16_t ADC_PRESCALER_TAB[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t ADC_SAMPLING_TIME_TAB[] = { 15, 19, 25, 37, 60, 105, 260, 653 };
const uint16_t ADC_OVERSAMPLING_RATIO_TAB[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

//--------------------------------------------------------------------------------------- Internal

static ADC_Common_TypeDef *ADC_GetCommon(ADC_t *adc)
{
  (void)adc;
  return ADC1_COMMON;
}

static void ADC_SetSequence(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  if(count > 16) count = 16;
  adc->reg->SQR1 = 0;
  adc->reg->SQR2 = 0;
  adc->reg->SQR3 = 0;
  adc->reg->SQR4 = 0;
  if(!count) return;
  adc->reg->SQR1 = ((uint32_t)(count - 1) << ADC_SQR1_L_Pos)
    | ((uint32_t)cha[0] << ADC_SQR1_SQ1_Pos);
  if(--count == 0) return;
  adc->reg->SQR1 |= (uint32_t)cha[1] << ADC_SQR1_SQ2_Pos; if(--count == 0) return;
  adc->reg->SQR1 |= (uint32_t)cha[2] << ADC_SQR1_SQ3_Pos; if(--count == 0) return;
  adc->reg->SQR1 |= (uint32_t)cha[3] << ADC_SQR1_SQ4_Pos; if(--count == 0) return;
  adc->reg->SQR2 |= (uint32_t)cha[4] << ADC_SQR2_SQ5_Pos; if(--count == 0) return;
  adc->reg->SQR2 |= (uint32_t)cha[5] << ADC_SQR2_SQ6_Pos; if(--count == 0) return;
  adc->reg->SQR2 |= (uint32_t)cha[6] << ADC_SQR2_SQ7_Pos; if(--count == 0) return;
  adc->reg->SQR2 |= (uint32_t)cha[7] << ADC_SQR2_SQ8_Pos; if(--count == 0) return;
  adc->reg->SQR2 |= (uint32_t)cha[8] << ADC_SQR2_SQ9_Pos; if(--count == 0) return;
  adc->reg->SQR3 |= (uint32_t)cha[9] << ADC_SQR3_SQ10_Pos; if(--count == 0) return;
  adc->reg->SQR3 |= (uint32_t)cha[10] << ADC_SQR3_SQ11_Pos; if(--count == 0) return;
  adc->reg->SQR3 |= (uint32_t)cha[11] << ADC_SQR3_SQ12_Pos; if(--count == 0) return;
  adc->reg->SQR3 |= (uint32_t)cha[12] << ADC_SQR3_SQ13_Pos; if(--count == 0) return;
  adc->reg->SQR3 |= (uint32_t)cha[13] << ADC_SQR3_SQ14_Pos; if(--count == 0) return;
  adc->reg->SQR4 |= (uint32_t)cha[14] << ADC_SQR4_SQ15_Pos; if(--count == 0) return;
  adc->reg->SQR4 |= (uint32_t)cha[15] << ADC_SQR4_SQ16_Pos;
}

static void ADC_SetSamplingTime(ADC_t *adc, uint8_t *cha, uint8_t count, ADC_SamplingTime_t st)
{
  uint32_t smpr1 = 0, smpr2 = 0;
  while(count--) {
    uint8_t ch = *cha++;
    if(ch <= 9) smpr1 |= ((uint32_t)st << (3u * ch));
    else if(ch <= 18) smpr2 |= ((uint32_t)st << (3u * (ch - 10u)));
  }
  adc->reg->SMPR1 = smpr1;
  adc->reg->SMPR2 = smpr2;
}

static void ADC_SetOversampling(ADC_t *adc, ADC_Oversampling_t *ovs)
{
  adc->reg->CFGR2 =
    (ovs->shift << ADC_CFGR2_OVSS_Pos) |
    (ovs->ratio << ADC_CFGR2_OVSR_Pos) |
    (ovs->enable ? ADC_CFGR2_ROVSE : 0);
}

static void ADC_SetPrescaler(ADC_t *adc, ADC_Prescaler_t prescaler)
{
  if(adc->prescaler != prescaler) {
    ADC_Disable(adc);
    adc->prescaler = prescaler;
    ADC_Common_TypeDef *common = ADC_GetCommon(adc);
    common->CCR = (common->CCR & ~ADC_CCR_PRESC_Msk) | (prescaler << ADC_CCR_PRESC_Pos);
    ADC_Enable(adc);
  }
}

//------------------------------------------------------------------------------------- Handler

static void ADC_IRQHandler(ADC_t *adc)
{
  if(adc->reg->ISR & ADC_ISR_OVR) {
    adc->reg->ISR = ADC_ISR_OVR;
    adc->_overrun++;
    ADC_Stop(adc);
  }
  else if(adc->reg->ISR & ADC_ISR_EOC) {
    adc->reg->ISR = ADC_ISR_EOC;
    adc->measure.output[adc->measure._active++] = adc->reg->DR;
    if(adc->measure._active >= adc->measure.chan_count) ADC_Stop(adc);
  }
}

#if(ADC_RECORD)
static void ADC_DMA_IRQHandler(ADC_t *adc)
{
  uint32_t isr = adc->record._dma.reg->ISR;
  uint8_t pos = adc->record._dma.pos;
  if(isr & DMA_ISR_HTIF(pos)) {
    adc->record._dma.reg->IFCR = DMA_ISR_HTIF(pos);
    if(adc->record.HalfCallback) {
      adc->record.HalfCallback(adc->record.callback_arg);
    }
  }
  if(isr & DMA_ISR_TCIF(pos)) {
    adc->record._dma.reg->IFCR = DMA_ISR_TCIF(pos);
    if(adc->record.continuous_mode) {
      if(adc->record.CompleteCallback) {
        adc->record.CompleteCallback(adc->record.callback_arg);
      }
    }
    else {
      ADC_Stop(adc);
    }
  }
}
#endif

//---------------------------------------------------------------------------------------- GPIO

static void ADC_InitGPIO(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  ADC_Common_TypeDef *common = ADC_GetCommon(adc);
  while(count--) {
    uint8_t ch = *cha++;
    switch(ch) {
      case ADC_IN_VREFEN: common->CCR |= ADC_CCR_VREFEN; break;
      case ADC_IN_TSEN:   common->CCR |= ADC_CCR_TSEN; break;
      case ADC_IN_VBATEN: common->CCR |= ADC_CCR_VBATEN; break;
      case ADC_IN_PC0: case ADC_IN_PC1: case ADC_IN_PC2: case ADC_IN_PC3:
        RCC_EnableGPIO(GPIOC);
        GPIOC->MODER |= (3u << (2u * (ch - ADC_IN_PC0)));
        break;
      case ADC_IN_PA0: case ADC_IN_PA1: case ADC_IN_PA2: case ADC_IN_PA3:
      case ADC_IN_PA4: case ADC_IN_PA5: case ADC_IN_PA6: case ADC_IN_PA7:
        RCC_EnableGPIO(GPIOA);
        GPIOA->MODER |= (3u << (2u * (ch - ADC_IN_PA0)));
        break;
      case ADC_IN_PC4: case ADC_IN_PC5:
        RCC_EnableGPIO(GPIOC);
        GPIOC->MODER |= (3u << (2u * (ch - ADC_IN_PC4 + 4)));
        break;
      case ADC_IN_PA8: case ADC_IN_PA9:
        RCC_EnableGPIO(GPIOA);
        GPIOA->MODER |= (3u << (2u * (ch - ADC_IN_PA8 + 8)));
        break;
    }
  }
}

//------------------------------------------------------------------------------ Enable/Disable

void ADC_Enable(ADC_t *adc)
{
  adc->reg->ISR = ADC_ISR_ADRDY;
  adc->reg->CR |= ADC_CR_ADEN;
  while(!(adc->reg->ISR & ADC_ISR_ADRDY)) __NOP();
}

void ADC_Disable(ADC_t *adc)
{
  if(adc->reg->CR & ADC_CR_ADSTART) {
    adc->reg->CR |= ADC_CR_ADSTP;
    while(adc->reg->CR & ADC_CR_ADSTP) let();
  }
  if(adc->reg->CR & ADC_CR_ADEN) {
    adc->reg->CR |= ADC_CR_ADDIS;
    while(adc->reg->CR & ADC_CR_ADEN) let();
  }
}

//---------------------------------------------------------------------------------------- Stop

void ADC_Stop(ADC_t *adc)
{
  adc->reg->CR |= ADC_CR_ADSTP;
  while(adc->reg->CR & ADC_CR_ADSTP) __NOP();
  switch(adc->_busy) {
    case ADC_State_Measure:
      adc->reg->IER &= ~ADC_IER_EOCIE;
      break;
    #if(ADC_RECORD)
    case ADC_State_Record:
      adc->record._dma.cha->CCR &= ~DMA_CCR_EN;
      break;
    #endif
    default: break;
  }
  adc->_busy = ADC_State_Free;
}

//------------------------------------------------------------------------------------- Measure

status_t ADC_Measure(ADC_t *adc)
{
  if(adc->_busy) return BUSY;
  adc->_busy = ADC_State_Measure;
  adc->measure._active = 0;
  ADC_SetPrescaler(adc, adc->measure.prescaler);
  ADC_SetOversampling(adc, &adc->measure.oversampling);
  ADC_SetSamplingTime(adc, adc->measure.chan, adc->measure.chan_count, adc->measure.sampling_time);
  ADC_SetSequence(adc, adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
    adc->reg->CFGR &= ~ADC_CFGR_EXTEN;
    adc->reg->CFGR |= ADC_CFGR_CONT;
  #endif
  adc->reg->IER |= ADC_IER_EOCIE;
  adc->reg->CR |= ADC_CR_ADSTART;
  return OK;
}

//-------------------------------------------------------------------------------------- Record

#if(ADC_RECORD)

status_t ADC_Record(ADC_t *adc)
{
  if(adc->_busy) return BUSY;
  adc->_busy = ADC_State_Record;
  ADC_SetPrescaler(adc, adc->record.prescaler);
  ADC_SetOversampling(adc, &adc->record.oversampling);
  ADC_SetSamplingTime(adc, adc->record.chan, adc->record.chan_count, adc->record.sampling_time);
  ADC_SetSequence(adc, adc->record.chan, adc->record.chan_count);
  adc->record._dma.cha->CCR &= ~DMA_CCR_EN;
  adc->record._dma.cha->CMAR = (uint32_t)adc->record.buff;
  adc->record._dma.cha->CNDTR = adc->record.buff_len;
  uint32_t cfgr_rst = ADC_CFGR_EXTSEL_Msk;
  uint32_t cfgr_set;
  if(adc->record.ext_trig) {
    cfgr_set = ADC_CFGR_EXTEN_0 | (adc->record.ext_select << ADC_CFGR_EXTSEL_Pos);
    cfgr_rst |= ADC_CFGR_CONT;
  }
  else {
    cfgr_set = ADC_CFGR_CONT;
    cfgr_rst |= ADC_CFGR_EXTEN;
  }
  adc->reg->CFGR = (adc->reg->CFGR & ~cfgr_rst) | cfgr_set;
  if(adc->record.continuous_mode) {
    adc->record._dma.cha->CCR |= DMA_CCR_CIRC;
    if(adc->record.HalfCallback) {
      adc->record._dma.cha->CCR |= DMA_CCR_HTIE;
    }
    else {
      adc->record._dma.cha->CCR &= ~DMA_CCR_HTIE;
    }
    if(adc->record.CompleteCallback) {
      adc->record._dma.cha->CCR |= DMA_CCR_TCIE;
    }
    else {
      adc->record._dma.cha->CCR &= ~DMA_CCR_TCIE;
    }
  }
  else {
    adc->record._dma.cha->CCR &= ~DMA_CCR_CIRC;
    adc->record._dma.cha->CCR &= ~DMA_CCR_HTIE;
    adc->record._dma.cha->CCR |= DMA_CCR_TCIE;
  }
  adc->record._dma.cha->CCR |= DMA_CCR_EN;
  adc->reg->CR |= ADC_CR_ADSTART;
  return OK;
}

#endif

//---------------------------------------------------------------------------------------- Init

void ADC_Init(ADC_t *adc)
{
  if(!adc->reg) adc->reg = ADC1;
  ADC_Disable(adc);
  ADC_Common_TypeDef *common = ADC_GetCommon(adc);
  RCC->CCIPR &= ~RCC_CCIPR_ADCSEL_Msk;
  if(adc->use_hsi) {
    RCC->CCIPR |= RCC_CCIPR_ADCSEL_1;
  }
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  common->CCR = (common->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
  adc->reg->CR &= ~ADC_CR_DEEPPWD;
  adc->reg->CR |= ADC_CR_ADVREGEN;
  for(uint32_t i = 0; i < SystemCoreClock / 500000; i++) let();
  adc->reg->CR &= ~ADC_CR_ADCALDIF;
  adc->reg->CR |= ADC_CR_ADCAL;
  while(adc->reg->CR & ADC_CR_ADCAL) let();
  #if(ADC_RECORD)
  if(adc->record.chan) {
    DMA_SetRegisters(adc->record.dma, &adc->record._dma);
    RCC_EnableDMA(adc->record._dma.reg);
    adc->reg->CFGR &= ~ADC_CFGR_DMAEN;
    adc->record._dma.mux->CCR = (adc->record._dma.mux->CCR & ~0x3Fu) | DMAMUX_REQ_ADC;
    adc->record._dma.cha->CPAR = (uint32_t)&adc->reg->DR;
    adc->record._dma.cha->CCR = DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    adc->reg->CFGR |= ADC_CFGR_DMAEN | ADC_CFGR_DMACFG;
    IRQ_EnableDMA(adc->record.dma, adc->irq_priority, (IRQ_Handler_t)ADC_DMA_IRQHandler, adc);
  }
  #endif
  ADC_InitGPIO(adc, adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
  if(adc->record.chan) {
    ADC_InitGPIO(adc, adc->record.chan, adc->record.chan_count);
  }
  #endif
  adc->reg->IER |= ADC_IER_OVRIE;
  IRQ_EnableADC(adc->irq_priority, (IRQ_Handler_t)ADC_IRQHandler, adc);
  #if(!ADC_RECORD)
    adc->reg->CFGR &= ~ADC_CFGR_EXTEN;
    adc->reg->CFGR |= ADC_CFGR_CONT;
  #endif
  ADC_Enable(adc);
}

//---------------------------------------------------------------------------------------------
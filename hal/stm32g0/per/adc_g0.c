// hal/stm32g0/adc_g0.c

#include "adc.h"
#include "dma.h"

//--------------------------------------------------------------------------------------- Const

const uint16_t ADC_PRESCALER_TAB[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t ADC_SAMPLING_TIME_TAB[] = { 14, 16, 20, 25, 32, 52, 92, 173 };
const uint16_t ADC_OVERSAMPLING_RATIO_TAB[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

//----------------------------------------------------------------------------------------- Set

static void ADC_SetChannels(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  uint32_t chselr = 0;
  while(count--) {
    chselr |= (1u << *cha++);
  }
  adc->reg->CHSELR = chselr;
}

static void ADC_SetOversampling(ADC_t *adc, ADC_Oversampling_t *ovs)
{
  adc->reg->CFGR2 =
    (ovs->shift << ADC_CFGR2_OVSS_Pos) |
    (ovs->ratio << ADC_CFGR2_OVSR_Pos) |
    (ovs->enable ? ADC_CFGR2_OVSE : 0);
}

static void ADC_SetPrescaler(ADC_t *adc, ADC_Prescaler_t prescaler)
{
  if(adc->prescaler != prescaler) {
    ADC_Disable(adc);
    adc->prescaler = prescaler;
    ADC->CCR = (ADC->CCR & ~ADC_CCR_PRESC_Msk) | (prescaler << ADC_CCR_PRESC_Pos);
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

static void ADC_InitGPIO(uint8_t *cha, uint8_t count)
{
  RCC_EnableGPIO(GPIOA);
  RCC_EnableGPIO(GPIOB);
  while(count--) {
    uint8_t ch = *cha++;
    if(ch <= 7) GPIOA->MODER |= (3u << (2u * ch));
    else if(ch <= 10) GPIOB->MODER |= (3u << (2u * (ch - 8)));
    else if(ch == 11) GPIOB->MODER |= (3u << (2u * 10));
    else if(ch == 12) ADC->CCR |= ADC_CCR_TSEN;
    else if(ch == 13) ADC->CCR |= ADC_CCR_VREFEN;
    else if(ch == 14) ADC->CCR |= ADC_CCR_VBATEN;
    else if(ch <= 16) GPIOB->MODER |= (3u << (2u * (ch - 4)));
    else if(ch <= 18) {
      RCC_EnableGPIO(GPIOC);
      GPIOC->MODER |= (3u << (2u * (ch - 13)));
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
  adc->reg->SMPR = adc->measure.sampling_time;
  ADC_SetChannels(adc, adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
    adc->reg->CFGR1 &= ~ADC_CFGR1_EXTEN;
    adc->reg->CFGR1 |= ADC_CFGR1_CONT;
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
  adc->reg->SMPR = adc->record.sampling_time;
  ADC_SetChannels(adc, adc->record.chan, adc->record.chan_count);
  adc->record._dma.cha->CCR &= ~DMA_CCR_EN;
  adc->record._dma.cha->CMAR = (uint32_t)adc->record.buff;
  adc->record._dma.cha->CNDTR = adc->record.buff_len;
  uint32_t cfgr_rst = ADC_CFGR1_EXTSEL_Msk;
  uint32_t cfgr_set;
  if(adc->record.ext_trig) {
    cfgr_set = ADC_CFGR1_EXTEN_0 | (adc->record.ext_select << ADC_CFGR1_EXTSEL_Pos);
    cfgr_rst |= ADC_CFGR1_CONT;
  }
  else {
    cfgr_set = ADC_CFGR1_CONT;
    cfgr_rst |= ADC_CFGR1_EXTEN;
  }
  adc->reg->CFGR1 = (adc->reg->CFGR1 & ~cfgr_rst) | cfgr_set;
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
  RCC->CCIPR &= ~RCC_CCIPR_ADCSEL_Msk;
  if(adc->use_hsi) {
    RCC->CCIPR |= RCC_CCIPR_ADCSEL_1;
  }
  RCC->APBENR2 |= RCC_APBENR2_ADCEN;
  ADC->CCR = (ADC->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
  adc->reg->CR |= ADC_CR_ADVREGEN;
  for(uint32_t i = 0; i < SystemCoreClock / 500000; i++) let();
  adc->reg->CR |= ADC_CR_ADCAL;
  while(!(adc->reg->ISR & ADC_ISR_EOCAL)) let();
  adc->reg->ISR = ADC_ISR_EOCAL;
  #if(ADC_RECORD)
  if(adc->record.chan) {
    DMA_SetRegisters(adc->record.dma, &adc->record._dma);
    RCC_EnableDMA(adc->record._dma.reg);
    adc->reg->CFGR1 &= ~ADC_CFGR1_DMAEN;
    adc->record._dma.mux->CCR = (adc->record._dma.mux->CCR & ~0x3Fu) | DMAMUX_REQ_ADC;
    adc->record._dma.cha->CPAR = (uint32_t)&adc->reg->DR;
    adc->record._dma.cha->CCR = DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    adc->reg->CFGR1 |= ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;
    IRQ_EnableDMA(adc->record.dma, adc->irq_priority, (IRQ_Handler_t)ADC_DMA_IRQHandler, adc);
  }
  #endif
  ADC_InitGPIO(adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
  if(adc->record.chan) {
    ADC_InitGPIO(adc->record.chan, adc->record.chan_count);
  }
  #endif
  adc->reg->IER |= ADC_IER_OVRIE;
  IRQ_EnableADC(adc->irq_priority, (IRQ_Handler_t)ADC_IRQHandler, adc);
  #if(!ADC_RECORD)
    adc->reg->CFGR1 &= ~ADC_CFGR1_EXTEN;
    adc->reg->CFGR1 |= ADC_CFGR1_CONT;
  #endif
  ADC_Enable(adc);
}

//---------------------------------------------------------------------------------------------
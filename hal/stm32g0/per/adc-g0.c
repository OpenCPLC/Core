#include "adc.h"

const uint16_t AdcPrescalerTab[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t AdcSamplingTimeTab[] = { 14, 16, 20, 25, 32, 52, 92, 173 };
const uint16_t AdcOversamplingRatioTab[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

//-------------------------------------------------------------------------------------------------

static void ADC_SetChannels(uint8_t *cha, uint8_t count)
{
  uint32_t chselr = 0;
  while(count) {
    chselr |= (1 << *cha);
    cha++; count--;
  }
  ADC1->CHSELR = chselr;
}

uint8_t ADC_Measure(ADC_t *adc)
{
  if(adc->busy) return BUSY;
  adc->busy = ADC_State_Measure;
  adc->measure.active = 0;
  if(adc->prescaler != adc->measure.prescaler) {
    ADC_Disable();
    adc->prescaler = adc->measure.prescaler;
    ADC->CCR = (ADC->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
    ADC_Enable();
  }
  ADC1->CFGR2 =
    (adc->measure.oversampling.shift << ADC_CFGR2_OVSS_Pos) |
    (adc->measure.oversampling.ratio << ADC_CFGR2_OVSR_Pos) |
    adc->measure.oversampling.enable << ADC_CFGR2_OVSE_Pos;
  ADC1->SMPR = adc->measure.sampling_time;
  ADC_SetChannels(adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
    ADC1->CFGR1 &= ~(ADC_CFGR1_EXTEN);
    ADC1->CFGR1 |= ADC_CFGR1_CONT;
  #endif
  ADC1->IER |= ADC_IER_EOCIE;
  ADC1->CR |= ADC_CR_ADSTART;
  return OK;
}

#if(ADC_RECORD)

uint8_t ADC_Record(ADC_t *adc)
{
  if(adc->busy) return BUSY;
  adc->busy = ADC_State_Record;
  if(adc->prescaler != adc->record.prescaler) {
    ADC_Disable();
    adc->prescaler = adc->record.prescaler;
    ADC->CCR = (ADC->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
    ADC_Enable();
  }
  ADC1->CFGR2 =
    (adc->record.oversampling.shift << ADC_CFGR2_OVSS_Pos) |
    (adc->record.oversampling.ratio << ADC_CFGR2_OVSR_Pos) |
    adc->record.oversampling.enable << ADC_CFGR2_OVSE_Pos;
  ADC1->SMPR = adc->record.sampling_time;
  ADC_SetChannels(adc->record.chan, adc->record.chan_count);
  adc->record.dma.cha->CMAR = (uint32_t)(adc->record.buff);
  adc->record.dma.cha->CNDTR = (uint32_t)(adc->record.buff_len);
  if(adc->record.tim) {
    TIM_Enable(adc->record.tim);
    ADC1->CFGR1 |= ADC_CFGR1_EXTEN;
    ADC1->CFGR1 &= ~ADC_CFGR1_CONT;
  }
  else {
    ADC1->CFGR1 &= ~(ADC_CFGR1_EXTEN);
    ADC1->CFGR1 |= ADC_CFGR1_CONT;
  }
  if(adc->record.continuous_mode) {
    adc->record.dma.cha->CCR |= DMA_CCR_CIRC;
    adc->record.dma.cha->CCR &= ~DMA_CCR_TCIE;
  }
  else {
    adc->record.dma.cha->CCR &= ~DMA_CCR_CIRC;
    adc->record.dma.cha->CCR |= DMA_CCR_TCIE;
  }
  adc->record.dma.cha->CCR |= DMA_CCR_EN;
  ADC1->CR |= ADC_CR_ADSTART;
  return OK;
}

/**
 * @brief Copy last samples from ADC DMA buffer
 * Read current DMA position, find latest `count` samples, handle wrap if needed, and copy them to `buffer`.
 * Function required in `continuous_mode`, where measurements are performed all the time.
 * @param[in] adc  ADC controller structure
 * @param[out] buffer Output buffer
 * @param[in] count Number of samples to copy. Must be smaller than the DMA buffer size.
 * @param[in] sort `false`: raw interleaved copy; `true`: deinterleave into channel blocks
 * @return `OK` on success, `ERR` if `count` is invalid
 */
status_t ADC_LastMeasurements(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort)
{
  if(!adc || !buffer) return ERR;
  DMA_Channel_TypeDef *cha = adc->record.dma.cha;
  uint16_t *src = adc->record.buff;
  uint16_t len = adc->record.buff_len;
  if(!src || !len || !count || count > len) return ERR;
  volatile uint16_t cnt1 = cha->CNDTR;
  volatile uint16_t cnt2 = cha->CNDTR;
  uint16_t cnt = (cnt2 < cnt1) ? cnt2 : cnt1;
  uint16_t write_idx = (uint16_t)((len - cnt) % len);
  uint16_t start_idx = (uint16_t)((write_idx + len - count) % len);
  if(!sort) {
    if(start_idx + count <= len) memcpy(buffer, &src[start_idx], (size_t)count * sizeof(uint16_t));
    else {
      uint16_t first = (uint16_t)(len - start_idx);
      memcpy(buffer, &src[start_idx], (size_t)first * sizeof(uint16_t));
      memcpy(buffer + first, &src[0], (size_t)(count - first) * sizeof(uint16_t));
    }
    return OK;
  }
  uint16_t n = adc->record.chan_count;
  if(n == 0) return ERR;
  if((uint16_t)(len % n) != 0u) return ERR;
  uint16_t samples = (uint16_t)(count / n);
  if(samples == 0) return ERR;
  uint16_t used = (uint16_t)(samples * n);
  start_idx = (uint16_t)((write_idx + len - used) % len);
  uint16_t ch0 = (uint16_t)(start_idx % n);
  for(uint16_t i = 0u; i < used; ++i) {
    uint16_t src_idx = (uint16_t)(start_idx + i);
    if(src_idx >= len) src_idx = (uint16_t)(src_idx - len);
    uint16_t pos = (uint16_t)(i % n);
    uint16_t t = (uint16_t)(i / n);
    uint16_t ch = (uint16_t)((ch0 + pos) % n);
    buffer[(uint16_t)(ch * samples + t)] = src[src_idx];
  }
  return OK;
}

float ADC_RecordSampleTime_s(ADC_t *adc)
{
  float freq = (adc->freq_16Mhz ? 16000000.0f : (float)SystemCoreClock) / AdcPrescalerTab[adc->record.prescaler];
  float time = (float)adc->record.chan_count * (float)AdcSamplingTimeTab[adc->record.sampling_time] / freq;
  if(adc->record.oversampling.enable) time *= (float)AdcOversamplingRatioTab[adc->record.oversampling.ratio];
  return time;
}

#endif

//-------------------------------------------------------------------------------------------------

void ADC_Stop(ADC_t *adc)
{
  ADC1->CR |= ADC_CR_ADSTP;
  while(ADC1->CR & ADC_CR_ADSTP) __NOP();
  switch(adc->busy) {
    case ADC_State_Measure: ADC1->IER &= ~ADC_IER_EOCIE; break;
    #if(ADC_RECORD)
      case ADC_State_Record:
        if(adc->record.tim) TIM_Disable(adc->record.tim);
        adc->record.dma.cha->CCR &= ~DMA_CCR_EN;
        break;
    #endif
  }
  adc->busy = ADC_State_Free;
}

bool ADC_IsBusy(ADC_t *adc)
{
  if(adc->busy) return true;
  return false;
}

bool ADC_IsFree(ADC_t *adc)
{
  if(adc->busy) return false;
  return true;
}

void ADC_Wait(ADC_t *adc)
{
  while(ADC_IsBusy(adc)) let();
}

void ADC_Enable(void)
{
  do {
    ADC1->CR |= ADC_CR_ADEN;
  } while((ADC1->ISR & ADC_ISR_ADRDY) == 0);
}

void ADC_Disable(void)
{
  if(ADC1->CR & ADC_CR_ADSTART) ADC1->CR |= ADC_CR_ADSTP;
  while((ADC1->CR & ADC_CR_ADSTP)) let();
  ADC1->CR |= ADC_CR_ADDIS;
  while(ADC1->CR & ADC_CR_ADEN) let();
}

//-------------------------------------------------------------------------------------------------


static void ADC_InterruptEV(ADC_t *adc)
{
  if(ADC1->ISR & ADC_ISR_OVR) {
    ADC1->ISR |= ADC_ISR_OVR;
    adc->overrun++;
    ADC_Stop(adc);
  }
  else if(ADC1->ISR & ADC_ISR_EOC) {
    ADC1->ISR |= ADC_ISR_EOC;
    adc->measure.output[adc->measure.active] = ADC1->DR;
    adc->measure.active++;
    if(adc->measure.active >= adc->measure.chan_count) ADC_Stop(adc);
  }
}

#if(ADC_RECORD)
static void ADC_InterruptDMA(ADC_t *adc)
{
  if(adc->record.dma.reg->ISR & DMA_ISR_TCIF(adc->record.dma.pos)) {
    adc->record.dma.reg->IFCR |= DMA_ISR_TCIF(adc->record.dma.pos);
    ADC_Stop(adc);
  }
}
#endif

//-------------------------------------------------------------------------------------------------

static void ADC_InitGPIO(uint32_t *chselr, uint8_t *cha, uint8_t count)
{
  while(count) {
    *chselr |= (1 << *cha);
    if(*cha < 8) GPIOA->MODER |= (3 << (2 * *cha));
    else if(*cha < 11) GPIOB->MODER |= (3 << (2 * (*cha - 8)));
    else if(*cha == 11) GPIOB->MODER |= (3 << (2 * 10));
    else if(*cha == 12) ADC->CCR |= ADC_CCR_TSEN;
    else if(*cha == 13) ADC->CCR |= ADC_CCR_VREFEN;
    else if(*cha == 14) ADC->CCR |= ADC_CCR_VBATEN;
    else if(*cha < 17) GPIOB->MODER |= (3 << (2 * (*cha - 4)));
    else if(*cha < 19) GPIOC->MODER |= (3 << (2 * (*cha - 13)));
    cha++; count--;
  }
}

void ADC_Init(ADC_t *adc)
{
  ADC_Disable();
  RCC->CCIPR &= ~RCC_CCIPR_ADCSEL_Msk;
  if(adc->freq_16Mhz) {
    RCC->CCIPR |= RCC_CCIPR_ADCSEL_1; // HSI16
  }
  RCC->APBENR2 |= RCC_APBENR2_ADCEN;
  ADC->CCR = (ADC->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
  ADC1->CR |= ADC_CR_ADVREGEN;
  for(uint32_t i = 0; i < SystemCoreClock / 500000; i++) let();
  ADC1->CR |= ADC_CR_ADCAL;
  while(!(ADC1->ISR & ADC_ISR_EOCAL)) let();
  ADC1->ISR |= ADC_ISR_EOCAL;
  #if(ADC_RECORD)
    DMA_SetRegisters(adc->record.dma_nbr, &adc->record.dma);
    RCC_EnableDMA(adc->record.dma.reg);
    ADC1->CFGR1 &= ~ADC_CFGR1_DMAEN;
    adc->record.dma.mux->CCR &= 0xFFFFFFC0;
    adc->record.dma.mux->CCR |= DMAMUX_REQ_ADC;
    adc->record.dma.cha->CPAR = (uint32_t)(&(ADC1->DR));
    adc->record.dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    ADC1->CFGR1 |= ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;
    IRQ_EnableDMA(adc->record.dma_nbr, adc->int_prioryty, (void (*)(void *))&ADC_InterruptDMA, adc);
  #endif
  uint32_t chselr = 0;
  ADC_InitGPIO(&chselr, adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
    if(adc->record.chan) ADC_InitGPIO(&chselr, adc->record.chan, adc->record.chan_count);
  #endif
  ADC1->CHSELR = chselr;
  ADC1->IER |= ADC_IER_OVRIE;
  IRQ_EnableADC(adc->int_prioryty, (void (*)(void *))&ADC_InterruptEV, adc);
  #if(ADC_RECORD)
    if(adc->record.tim) {
      switch((uint32_t)adc->record.tim->reg) {
        case (uint32_t)TIM1: ADC1->CFGR1 |= (0 << ADC_CFGR1_EXTSEL_Pos); break;
        case (uint32_t)TIM3: ADC1->CFGR1 |= (3 << ADC_CFGR1_EXTSEL_Pos); break;
        case (uint32_t)TIM6: ADC1->CFGR1 |= (5 << ADC_CFGR1_EXTSEL_Pos); break;
        case (uint32_t)TIM15: ADC1->CFGR1 |= (4 << ADC_CFGR1_EXTSEL_Pos); break;
      }
      adc->record.tim->irq_priority = adc->int_prioryty;
      adc->record.tim->one_pulse_mode = false;
      adc->record.tim->enable = false;
      adc->record.tim->enable_interrupt = false;
      TIM_Init(adc->record.tim);
      TIM_MasterMode(adc->record.tim, TIM_MasterMode_Update);
    }
  #else
    ADC1->CFGR1 &= ~ADC_CFGR1_EXTEN;
    ADC1->CFGR1 |= ADC_CFGR1_CONT;
  #endif
  ADC_Enable();
}

//-------------------------------------------------------------------------------------------------

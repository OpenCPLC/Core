#include "adc.h"

const uint16_t AdcPrescalerTab[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t AdcSamplingTimeTab[] = { 2, 6, 12, 24, 47, 92, 247, 640 };
const uint16_t AdcOversamplingRatioTab[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

#define DMAMUX_REQ_ADC 5

//-------------------------------------------------------------------------------------------------

static void ADC_SetChannels(uint8_t *cha, uint8_t count)
{
  uint8_t seq_count = count;
  if(seq_count > 16) seq_count = 16;
  ADC1->SQR1 = 0;
  ADC1->SQR2 = 0;
  ADC1->SQR3 = 0;
  ADC1->SQR4 = 0;
  if(!seq_count) return;
  ADC1->SQR1 |= (uint32_t)(seq_count - 1) << ADC_SQR1_L_Pos;
  ADC1->SQR1 |= (uint32_t)cha[0] << ADC_SQR1_SQ1_Pos;
  if(!--seq_count) return;
  ADC1->SQR1 |= (uint32_t)cha[1] << ADC_SQR1_SQ2_Pos;
  if(!--seq_count) return;
  ADC1->SQR1 |= (uint32_t)cha[2] << ADC_SQR1_SQ3_Pos;
  if(!--seq_count) return;
  ADC1->SQR1 |= (uint32_t)cha[3] << ADC_SQR1_SQ4_Pos;
  if(!--seq_count) return;
  ADC1->SQR2 |= (uint32_t)cha[4] << ADC_SQR2_SQ5_Pos;
  if(!--seq_count) return;
  ADC1->SQR2 |= (uint32_t)cha[5] << ADC_SQR2_SQ6_Pos;
  if(!--seq_count) return;
  ADC1->SQR2 |= (uint32_t)cha[6] << ADC_SQR2_SQ7_Pos;
  if(!--seq_count) return;
  ADC1->SQR2 |= (uint32_t)cha[7] << ADC_SQR2_SQ8_Pos;
  if(!--seq_count) return;
  ADC1->SQR2 |= (uint32_t)cha[8] << ADC_SQR2_SQ9_Pos;
  if(!--seq_count) return;
  ADC1->SQR3 |= (uint32_t)cha[9] << ADC_SQR3_SQ10_Pos;
  if(!--seq_count) return;
  ADC1->SQR3 |= (uint32_t)cha[10] << ADC_SQR3_SQ11_Pos;
  if(!--seq_count) return;
  ADC1->SQR3 |= (uint32_t)cha[11] << ADC_SQR3_SQ12_Pos;
  if(!--seq_count) return;
  ADC1->SQR3 |= (uint32_t)cha[12] << ADC_SQR3_SQ13_Pos;
  if(!--seq_count) return;
  ADC1->SQR3 |= (uint32_t)cha[13] << ADC_SQR3_SQ14_Pos;
  if(!--seq_count) return;
  ADC1->SQR4 |= (uint32_t)cha[14] << ADC_SQR4_SQ15_Pos;
  if(!--seq_count) return;
  ADC1->SQR4 |= (uint32_t)cha[15] << ADC_SQR4_SQ16_Pos;
}

static void ADC_SetSamplingTime(uint8_t *cha, uint8_t count, ADC_SamplingTime_t sampling_time)
{
  uint32_t smpr1 = 0;
  uint32_t smpr2 = 0;
  while(count) {
    uint8_t ch = *cha;
    if(ch <= 9) smpr1 |= ((uint32_t)sampling_time & 0x7u) << (3u * ch);
    else if(ch <= 18) smpr2 |= ((uint32_t)sampling_time & 0x7u) << (3u * (ch - 10u));
    cha++; count--;
  }
  ADC1->SMPR1 = smpr1;
  ADC1->SMPR2 = smpr2;
}

uint8_t ADC_Measure(ADC_t *adc)
{
  if(adc->busy) return BUSY;
  adc->busy = ADC_State_Measure;
  adc->measure.active = 0;
  if(adc->prescaler != adc->measure.prescaler) {
    ADC_Disable();
    adc->prescaler = adc->measure.prescaler;
    ADC1_COMMON->CCR = (ADC1_COMMON->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
    ADC_Enable();
  }
  ADC1->CFGR2 =
    (adc->measure.oversampling.shift << ADC_CFGR2_OVSS_Pos) |
    (adc->measure.oversampling.ratio << ADC_CFGR2_OVSR_Pos) |
    (adc->measure.oversampling.enable ? ADC_CFGR2_ROVSE : 0);
  ADC_SetSamplingTime(adc->measure.chan, adc->measure.chan_count, adc->measure.sampling_time);
  ADC_SetChannels(adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
    ADC1->CFGR &= ~(ADC_CFGR_EXTEN);
    ADC1->CFGR |= ADC_CFGR_CONT;
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
    ADC1_COMMON->CCR = (ADC1_COMMON->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
    ADC_Enable();
  }
  ADC1->CFGR2 =
    (adc->record.oversampling.shift << ADC_CFGR2_OVSS_Pos) |
    (adc->record.oversampling.ratio << ADC_CFGR2_OVSR_Pos) |
    (adc->record.oversampling.enable ? ADC_CFGR2_ROVSE : 0);
  ADC_SetSamplingTime(adc->record.chan, adc->record.chan_count, adc->record.sampling_time);
  ADC_SetChannels(adc->record.chan, adc->record.chan_count);
  adc->record.dma.cha->CMAR = (uint32_t)(adc->record.buff);
  adc->record.dma.cha->CNDTR = (uint32_t)(adc->record.buff_len);
  if(adc->record.tim) {
    TIM_Enable(adc->record.tim);
    ADC1->CFGR |= ADC_CFGR_EXTEN;
    ADC1->CFGR &= ~ADC_CFGR_CONT;
  }
  else {
    ADC1->CFGR &= ~(ADC_CFGR_EXTEN);
    ADC1->CFGR |= ADC_CFGR_CONT;
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

static void ADC_InitGPIO(uint8_t *cha, uint8_t count)
{
  while(count) {
    switch(*cha) {
      case ADC_IN_PA0: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 0u)); break;
      case ADC_IN_PA1: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 1u)); break;
      case ADC_IN_PA2: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 2u)); break;
      case ADC_IN_PA3: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 3u)); break;
      case ADC_IN_PA4: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 4u)); break;
      case ADC_IN_PA5: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 5u)); break;
      case ADC_IN_PA6: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 6u)); break;
      case ADC_IN_PA7: RCC_EnableGPIO(GPIOA); GPIOA->MODER |= (3u << (2u * 7u)); break;
      case ADC_IN_PB0: RCC_EnableGPIO(GPIOB); GPIOB->MODER |= (3u << (2u * 0u)); break;
      case ADC_IN_PB1: RCC_EnableGPIO(GPIOB); GPIOB->MODER |= (3u << (2u * 1u)); break;
      case ADC_IN_PC0: RCC_EnableGPIO(GPIOC); GPIOC->MODER |= (3u << (2u * 0u)); break;
      case ADC_IN_PC1: RCC_EnableGPIO(GPIOC); GPIOC->MODER |= (3u << (2u * 1u)); break;
      case ADC_IN_PC2: RCC_EnableGPIO(GPIOC); GPIOC->MODER |= (3u << (2u * 2u)); break;
      case ADC_IN_PC3: RCC_EnableGPIO(GPIOC); GPIOC->MODER |= (3u << (2u * 3u)); break;
      case ADC_IN_PC4: RCC_EnableGPIO(GPIOC); GPIOC->MODER |= (3u << (2u * 4u)); break;
      case ADC_IN_PC5: RCC_EnableGPIO(GPIOC); GPIOC->MODER |= (3u << (2u * 5u)); break;
      case ADC_IN_VREFEN: ADC1_COMMON->CCR |= ADC_CCR_VREFEN; break;
      case ADC_IN_TSEN: ADC1_COMMON->CCR |= ADC_CCR_TSEN; break;
      case ADC_IN_VBATEN: ADC1_COMMON->CCR |= ADC_CCR_VBATEN; break;
    }
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
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  ADC1_COMMON->CCR = (ADC1_COMMON->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
  ADC1->CR |= ADC_CR_ADVREGEN;
  for(uint32_t i = 0; i < SystemCoreClock / 500000; i++) let();
  ADC1->CR |= ADC_CR_ADCAL;
  while(ADC1->CR & ADC_CR_ADCAL) let();
  #if(ADC_RECORD)
    DMA_SetRegisters(adc->record.dma_nbr, &adc->record.dma);
    RCC_EnableDMA(adc->record.dma.reg);
    ADC1->CFGR &= ~ADC_CFGR_DMAEN;
    adc->record.dma.mux->CCR &= 0xFFFFFFC0;
    adc->record.dma.mux->CCR |= DMAMUX_REQ_ADC;
    adc->record.dma.cha->CPAR = (uint32_t)(&(ADC1->DR));
    adc->record.dma.cha->CCR |= DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    ADC1->CFGR |= ADC_CFGR_DMAEN | ADC_CFGR_DMACFG;
    IRQ_EnableDMA(adc->record.dma_nbr, adc->int_prioryty, (void (*)(void *))&ADC_InterruptDMA, adc);
  #endif
  ADC_InitGPIO(adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
    if(adc->record.chan) ADC_InitGPIO(adc->record.chan, adc->record.chan_count);
  #endif
  ADC1->IER |= ADC_IER_OVRIE;
  IRQ_EnableADC(adc->int_prioryty, (void (*)(void *))&ADC_InterruptEV, adc);
  #if(ADC_RECORD)
    if(adc->record.tim) {
      ADC1->CFGR &= ~ADC_CFGR_EXTSEL_Msk;
      switch((uint32_t)adc->record.tim->reg) {
        case (uint32_t)TIM1: ADC1->CFGR |= (0 << ADC_CFGR_EXTSEL_Pos); break;
        case (uint32_t)TIM3: ADC1->CFGR |= (3 << ADC_CFGR_EXTSEL_Pos); break;
        case (uint32_t)TIM6: ADC1->CFGR |= (5 << ADC_CFGR_EXTSEL_Pos); break;
        case (uint32_t)TIM15: ADC1->CFGR |= (4 << ADC_CFGR_EXTSEL_Pos); break;
      }
      adc->record.tim->irq_priority = adc->int_prioryty;
      adc->record.tim->one_pulse_mode = false;
      adc->record.tim->enable = false;
      adc->record.tim->enable_interrupt = false;
      TIM_Init(adc->record.tim);
      TIM_MasterMode(adc->record.tim, TIM_MasterMode_Update);
    }
  #else
    ADC1->CFGR &= ~ADC_CFGR_EXTEN;
    ADC1->CFGR |= ADC_CFGR_CONT;
  #endif
  ADC_Enable();
}

//-------------------------------------------------------------------------------------------------

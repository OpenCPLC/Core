// hal/stm32g0/per/adc_g0.c

#include "adc.h"
#include "dma.h"

//------------------------------------------------------------------------------------------- Const

const uint16_t ADC_PRESCALER_TAB[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };

uint32_t ADC_Frequency_Hz(ADC_t *adc)
{
  uint32_t base;
  switch(adc->clock) {
    case ADC_Clock_SYSCLK: base = SystemCoreClock; break;
    case ADC_Clock_PLLP: return 0; // the framework does not run the PLL
    default: base = 16000000u; break; // HSI16
  }
  return base / ADC_PRESCALER_TAB[adc->prescaler];
}
const uint16_t ADC_SAMPLING_TIME_TAB[] = { 14, 16, 20, 25, 32, 52, 92, 173 };
const uint16_t ADC_OVERSAMPLING_RATIO_TAB[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

//------------------------------------------------------------------------------------------- Setup

// Up to 8 channels (numbers 0..14) go through the configurable sequencer,
// which preserves the order of the `cha` list;
// larger sets fall back to the bitmask scan, ascending by channel number.
// `pad` appends sacrificial repeats of the last channel
// (the errata workaround behind `adc_scan_len`), possible only in sequencer mode.
// With the ADC enabled, every step of the channel configuration
// applies on its own `CCRDY` handshake:
// one for a `CHSELRMOD` change (raised only when the bit really changes),
// and one for the `CHSELR` write, so a conversion started right after is reliable
static void ADC_SetChannels(ADC_t *adc, uint8_t *cha, uint8_t count, uint8_t pad)
{
  bool seq = count + pad <= 8;
  for(uint8_t i = 0; i < count && seq; i++) {
    if(cha[i] > 14) seq = false;
  }
  uint32_t chselr;
  if(seq) {
    chselr = 0xFFFFFFFFu; // unused slots keep the 0xF end-of-sequence marker
    for(uint8_t i = 0; i < count + pad; i++) {
      chselr &= ~(0xFu << (4 * i));
      chselr |= (uint32_t)cha[i < count ? i : count - 1] << (4 * i);
    }
  }
  else {
    chselr = 0;
    for(uint8_t i = 0; i < count; i++) chselr |= (1u << cha[i]);
  }
  bool enabled = adc->reg->CR & ADC_CR_ADEN;
  uint32_t cfgr = adc->reg->CFGR1;
  uint32_t cfgr_new = seq ? (cfgr | ADC_CFGR1_CHSELRMOD) : (cfgr & ~ADC_CFGR1_CHSELRMOD);
  if(cfgr_new != cfgr) {
    adc->reg->ISR = ADC_ISR_CCRDY;
    adc->reg->CFGR1 = cfgr_new;
    if(enabled) while(!(adc->reg->ISR & ADC_ISR_CCRDY)) __NOP();
  }
  adc->reg->ISR = ADC_ISR_CCRDY;
  adc->reg->CHSELR = chselr;
  if(enabled) {
    while(!(adc->reg->ISR & ADC_ISR_CCRDY)) __NOP();
    adc->reg->ISR = ADC_ISR_CCRDY;
  }
}

static void ADC_SetOversampling(ADC_t *adc, ADC_Oversampling_t *ovs)
{
  adc->reg->CFGR2 =
    (ovs->shift << ADC_CFGR2_OVSS_Pos) |
    (ovs->ratio << ADC_CFGR2_OVSR_Pos) |
    (ovs->enable ? ADC_CFGR2_OVSE : 0);
}

//-------------------------------------------------------------------------------------------- GPIO

// Channel-to-pin map: 0..7 = PA0..PA7, 8..10 = PB0..PB2, 11 = PB10, 15..16 = PB11..PB12,
// 17..18 = PC4..PC5; channels 12..14 are the internal temperature, VREFINT and VBAT sources
void ADC_InitGPIO(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  unused(adc); // single common register block on this family
  while(count--) {
    uint8_t ch = *cha++;
    GPIO_TypeDef *port;
    uint8_t pin;
    switch(ch) {
      case 12: ADC->CCR |= ADC_CCR_TSEN; continue;
      case 13: ADC->CCR |= ADC_CCR_VREFEN; continue;
      case 14: ADC->CCR |= ADC_CCR_VBATEN; continue;
      case 11: port = GPIOB; pin = 10; break;
      case 15: port = GPIOB; pin = 11; break;
      case 16: port = GPIOB; pin = 12; break;
      case 17: port = GPIOC; pin = 4; break;
      case 18: port = GPIOC; pin = 5; break;
      default:
        if(ch > 18) continue;
        if(ch <= 7) { port = GPIOA; pin = ch; }
        else { port = GPIOB; pin = ch - 8; }
    }
    RCC_EnableGPIO(port);
    port->MODER |= 3u << (2 * pin);
  }
}

//----------------------------------------------------------------------------------------- Handler

// An overrun means a sample was lost, and in a scanned sequence that also loses the channel
// alignment of everything that follows, so the run is aborted and counted instead of limping
// on with shifted data. Restart policy belongs to the application: it alone knows whether
// a gap in the stream is acceptable
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

//---------------------------------------------------------------------------------- Enable/Disable

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

//-------------------------------------------------------------------------------------------- Stop

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

//----------------------------------------------------------------------------------------- Measure

status_t ADC_Measure(ADC_t *adc)
{
  if(adc->_busy) return BUSY;
  adc->_busy = ADC_State_Measure;
  adc->measure._active = 0;
  ADC_SetOversampling(adc, &adc->measure.oversampling);
  adc->reg->SMPR = adc->measure.sampling_time;
  ADC_SetChannels(adc, adc->measure.chan, adc->measure.chan_count, 0);
  // Single-shot by nature: the sequence ends on its own after the last channel,
  // which keeps the data rate at the interrupt's pace instead of racing a free-running ADC
  adc->reg->CFGR1 &= ~(ADC_CFGR1_EXTEN | ADC_CFGR1_CONT);
  adc->reg->IER |= ADC_IER_EOCIE;
  adc->reg->CR |= ADC_CR_ADSTART;
  return OK;
}

//------------------------------------------------------------------------------------------ Record

#if(ADC_RECORD)

status_t ADC_Record(ADC_t *adc)
{
  if(adc->_busy) return BUSY;
  adc->_busy = ADC_State_Record;
  ADC_SetOversampling(adc, &adc->record.oversampling);
  adc->reg->SMPR = adc->record.sampling_time;
  // Sequencer + oversampling errata: pad multi-channel scans with one sacrificial
  // conversion, matching the buffer layout `adc_scan_len` promises the application
  adc->record._pad = (uint8_t)(adc_scan_len(adc->record.chan_count,
    adc->record.oversampling.enable) - adc->record.chan_count);
  ADC_SetChannels(adc, adc->record.chan, adc->record.chan_count, adc->record._pad);
  adc->record._dma.cha->CCR &= ~DMA_CCR_EN;
  adc->record._dma.cha->CMAR = (uint32_t)adc->record.buff;
  adc->record._dma.cha->CNDTR = adc->record.buff_len;
  // Triggered mode arms one sequence per hardware event; otherwise the ADC free-runs
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
    if(adc->record.HalfCallback) adc->record._dma.cha->CCR |= DMA_CCR_HTIE;
    else adc->record._dma.cha->CCR &= ~DMA_CCR_HTIE;
    if(adc->record.CompleteCallback) adc->record._dma.cha->CCR |= DMA_CCR_TCIE;
    else adc->record._dma.cha->CCR &= ~DMA_CCR_TCIE;
  }
  else {
    adc->record._dma.cha->CCR &= ~(DMA_CCR_CIRC | DMA_CCR_HTIE);
    adc->record._dma.cha->CCR |= DMA_CCR_TCIE;
  }
  adc->record._dma.cha->CCR |= DMA_CCR_EN;
  adc->reg->CR |= ADC_CR_ADSTART;
  return OK;
}

#endif

//-------------------------------------------------------------------------------------------- Init

void ADC_Init(ADC_t *adc)
{
  if(!adc->reg) adc->reg = ADC1;
  ADC_Disable(adc);
  // Register route per `ADC_Clock_t`: `Default` is HSI16 on this family
  static const uint8_t clock_sel[] = { 2, 0, 1, 2 };
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_ADCSEL_Msk)
    | ((uint32_t)clock_sel[adc->clock] << RCC_CCIPR_ADCSEL_Pos);
  RCC->APBENR2 |= RCC_APBENR2_ADCEN;
  ADC->CCR = (ADC->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
  // Voltage regulator startup (tADCVREG_SETUP), then self-calibration on a disabled ADC
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
  ADC_InitGPIO(adc, adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
  if(adc->record.chan) {
    ADC_InitGPIO(adc, adc->record.chan, adc->record.chan_count);
  }
  #endif
  adc->reg->IER |= ADC_IER_OVRIE;
  IRQ_EnableADC(adc->irq_priority, (IRQ_Handler_t)ADC_IRQHandler, adc);
  ADC_Enable(adc);
}

//-------------------------------------------------------------------------------------------------

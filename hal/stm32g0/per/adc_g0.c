// hal/stm32g0/per/adc_g0.c

#include "adc.h"

#include "dma.h"

//------------------------------------------------------------------------------------------ Config

// Voltage regulator start-up, datasheet `tADCVREG_STUP`
#define ADC_VREG_STARTUP_us 20

// Internal source start-up after its `ADC_CCR` bit, temperature sensor worst case
#define ADC_SOURCE_STARTUP_us 120

// Self-calibration runs averaged into `CALFACT` (RM0444 §15.3.3)
#define ADC_CAL_RUNS 8

//------------------------------------------------------------------------------------------ Tables

const uint16_t ADC_PRESCALER_TAB[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t ADC_SAMPLING_TIME_TAB[] = { 14, 16, 20, 25, 32, 52, 92, 173 };
const uint16_t ADC_OVERSAMPLING_RATIO_TAB[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

uint32_t ADC_Frequency_Hz(ADC_t *adc)
{
  uint32_t base;
  switch(adc->clock) {
    case ADC_Clock_SYSCLK: base = SystemCoreClock; break;
    case ADC_Clock_PLLP: return 0; // framework does not run PLL
    default: base = 16000000u; break; // HSI16
  }
  return base / ADC_PRESCALER_TAB[adc->prescaler];
}

//---------------------------------------------------------------------------------------- Internal

static ADC_t *self;       // `ADC_Suspend` target, one converter on this family
static uint32_t sources;  // internal sources `ADC_Suspend` switched off

// Channel setup keeping list order:
// configurable sequencer takes up to 8 conversions of channels 0..14 in any order,
// bitmask scan takes an ascending list only.
// With oversampling, last conversion of a sequencer scan reads wrong on G0,
// though ST lists no erratum: a repeat of last channel closes the scan and is dropped.
// Returns those dropped repeats, `-1` when neither mode keeps order.
static int8_t select_channels(const uint8_t *cha, uint8_t count, bool ovs,
  uint32_t *chselr, uint32_t *mode)
{
  if(!cha || !count) return -1;
  int8_t tail = (ovs && count > 1) ? 1 : 0;
  bool sequencer = count + tail <= 8;
  bool ascending = true;
  for(uint8_t i = 0; i < count; i++) {
    if(cha[i] > 14) sequencer = false;
    if(cha[i] > 18 || (i && cha[i] <= cha[i - 1])) ascending = false;
  }
  if(sequencer) {
    *mode = ADC_CFGR1_CHSELRMOD;
    *chselr = 0xFFFFFFFFu; // unused slots keep `0xF` end-of-sequence marker
    for(uint8_t i = 0; i < count + tail; i++) {
      *chselr &= ~(0xFu << (4 * i));
      *chselr |= (uint32_t)cha[i < count ? i : count - 1] << (4 * i);
    }
    return tail;
  }
  if(!ascending) return -1;
  *mode = 0;
  *chselr = 0;
  for(uint8_t i = 0; i < count; i++) *chselr |= 1u << cha[i];
  return 0;
}

static uint32_t oversampling_bits(ADC_Oversampling_t *ovs) {
  return (ovs->shift << ADC_CFGR2_OVSS_Pos) |
    (ovs->ratio << ADC_CFGR2_OVSR_Pos) |
    (ovs->enable ? ADC_CFGR2_OVSE : 0);
}

// `CFGR1` and `CFGR2` take writes only with converter disabled (RM0444 §15.12.4, §15.12.5):
// a write to an enabled one clears `RES`, and `CKMODE` on G071/G081 (errata).
// Every channel setup change applies on its own `CCRDY` handshake, or `ADSTART` is ignored.
// `LFTRIG` rearms converter at each trigger: without it a conversion may read corrupted
// when it starts over `tIDLE` (100µs) after the previous one or the enable (RM0444 §15.4.6).
static void configure(ADC_t *adc, uint32_t cfgr1, uint32_t cfgr2, uint32_t chselr,
  ADC_SamplingTime_t sampling_time)
{
  cfgr2 |= ADC_CFGR2_LFTRIG;
  if(adc->reg->CFGR1 != cfgr1 || adc->reg->CFGR2 != cfgr2 || adc->reg->SMPR != sampling_time) {
    // `CCRDY` answers `CHSELRMOD` write only when bit changes
    bool mode_change = (adc->reg->CFGR1 ^ cfgr1) & ADC_CFGR1_CHSELRMOD;
    ADC_Disable(adc);
    adc->reg->ISR = ADC_ISR_CCRDY;
    adc->reg->CFGR1 = cfgr1;
    adc->reg->CFGR2 = cfgr2;
    adc->reg->SMPR = sampling_time;
    if(mode_change) while(!(adc->reg->ISR & ADC_ISR_CCRDY)) __NOP();
  }
  ADC_Enable(adc);
  if(adc->reg->CHSELR != chselr) {
    adc->reg->ISR = ADC_ISR_CCRDY;
    adc->reg->CHSELR = chselr;
    while(!(adc->reg->ISR & ADC_ISR_CCRDY)) __NOP();
  }
}

//-------------------------------------------------------------------------------------------- GPIO

// Channel-to-pin map: 0..7 = PA0..PA7, 8..10 = PB0..PB2, 11 = PB10, 15..16 = PB11..PB12,
// 17..18 = PC4..PC5; channels 12..14 are internal temperature, VREFINT and VBAT sources.
void ADC_InitGPIO(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  unused(adc); // single common register block on this family
  uint32_t ccr = ADC->CCR;
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
  if(ADC->CCR != ccr) ADC_Delay_us(ADC_SOURCE_STARTUP_us);
}

//----------------------------------------------------------------------------------------- Handler

// Overrun means a sample was lost,
// and with it the channel alignment of everything that follows in a scanned sequence,
// so run is aborted and counted instead of limping on with shifted data.
// Restart policy belongs to application: it alone knows whether a gap is acceptable.
// `DR` is read with `EOC` still set, so a result landing before the read raises `OVR`.
static void irq_handler(ADC_t *adc)
{
  if(adc->reg->ISR & ADC_ISR_OVR) {
    adc->reg->ISR = ADC_ISR_OVR;
    adc->_overrun++;
    ADC_Stop(adc);
  }
  else if(adc->reg->ISR & ADC_ISR_EOC) {
    ADC_Measure_t *mea = &adc->measure;
    uint8_t k = mea->_active++ - mea->_lead; // past list for a dropped conversion
    uint16_t value = adc->reg->DR;
    if(k < mea->chan_count) mea->output[k] = value;
    if(mea->_active >= mea->_total) ADC_Stop(adc);
  }
}

#if(ADC_RECORD)
static void dma_handler(ADC_t *adc)
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
  if(adc->reg->CR & ADC_CR_ADEN) return;
  adc->reg->ISR = ADC_ISR_ADRDY;
  adc->reg->CR |= ADC_CR_ADEN;
  while(!(adc->reg->ISR & ADC_ISR_ADRDY)) __NOP();
}

void ADC_Disable(ADC_t *adc)
{
  if(adc->reg->CR & ADC_CR_ADSTART) {
    adc->reg->CR |= ADC_CR_ADSTP;
    while(adc->reg->CR & ADC_CR_ADSTP) __NOP();
  }
  if(adc->reg->CR & ADC_CR_ADEN) {
    adc->reg->CR |= ADC_CR_ADDIS;
    while(adc->reg->CR & ADC_CR_ADEN) __NOP();
  }
}

// Stop modes want converter, its regulator and internal sources off
// (RM0444 §15.3.2 and §15.9); calibration stays.
void ADC_Suspend(void)
{
  if(!self) return;
  if(self->_busy) {
    self->_overrun++;
    ADC_Stop(self);
  }
  ADC_Disable(self);
  sources = ADC->CCR & (ADC_CCR_TSEN | ADC_CCR_VREFEN | ADC_CCR_VBATEN);
  ADC->CCR &= ~sources;
  self->reg->CR &= ~ADC_CR_ADVREGEN;
}

void ADC_Resume(void)
{
  if(!self) return;
  self->reg->CR |= ADC_CR_ADVREGEN;
  ADC_Delay_us(ADC_VREG_STARTUP_us);
  ADC->CCR |= sources;
  if(sources) ADC_Delay_us(ADC_SOURCE_STARTUP_us);
}

//-------------------------------------------------------------------------------------------- Stop

void ADC_Stop(ADC_t *adc)
{
  // `ADSTP` acts on running conversion only
  if(adc->reg->CR & ADC_CR_ADSTART) {
    adc->reg->CR |= ADC_CR_ADSTP;
    while(adc->reg->CR & ADC_CR_ADSTP) __NOP();
  }
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
  ADC_Measure_t *mea = &adc->measure;
  uint32_t chselr, mode;
  int8_t tail = select_channels(mea->chan, mea->chan_count, mea->oversampling.enable,
    &chselr, &mode);
  if(tail < 0) return ERR;
  adc->_busy = ADC_State_Measure;
  mea->_active = 0;
  mea->_lead = 0;
  mea->_total = mea->chan_count + tail;
  // Single-shot by nature: sequence ends on its own after last channel,
  // which keeps data rate at interrupt's pace instead of racing a free-running ADC.
  configure(adc, mode, oversampling_bits(&mea->oversampling), chselr, mea->sampling_time);
  adc->reg->ISR = ADC_ISR_EOC | ADC_ISR_OVR;
  adc->reg->IER |= ADC_IER_EOCIE;
  adc->reg->CR |= ADC_CR_ADSTART;
  return OK;
}

//------------------------------------------------------------------------------------------ Record

#if(ADC_RECORD)

status_t ADC_Record(ADC_t *adc)
{
  if(adc->_busy) return BUSY;
  ADC_Record_t *rec = &adc->record;
  uint32_t chselr, mode;
  int8_t tail = select_channels(rec->chan, rec->chan_count, rec->oversampling.enable,
    &chselr, &mode);
  if(tail < 0 || !rec->_dma.cha) return ERR;
  rec->_lead = 0;
  rec->_tail = tail;
  rec->_len = rec->buff_len - rec->buff_len % ADC_RecordStride(adc);
  if(!rec->_len) return ERR;
  adc->_busy = ADC_State_Record;
  // Triggered mode arms one sequence per hardware event; otherwise ADC free-runs
  uint32_t trigger = rec->ext_trig
    ? ADC_CFGR1_EXTEN_0 | (rec->ext_select << ADC_CFGR1_EXTSEL_Pos)
    : ADC_CFGR1_CONT;
  configure(adc, mode | trigger | ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG,
    oversampling_bits(&rec->oversampling), chselr, rec->sampling_time);
  DMA_Channel_TypeDef *cha = rec->_dma.cha;
  cha->CCR &= ~DMA_CCR_EN;
  cha->CMAR = (uint32_t)rec->buff;
  cha->CNDTR = rec->_len;
  if(rec->continuous_mode) {
    cha->CCR |= DMA_CCR_CIRC;
    if(rec->HalfCallback) cha->CCR |= DMA_CCR_HTIE;
    else cha->CCR &= ~DMA_CCR_HTIE;
    if(rec->CompleteCallback) cha->CCR |= DMA_CCR_TCIE;
    else cha->CCR &= ~DMA_CCR_TCIE;
  }
  else {
    cha->CCR &= ~(DMA_CCR_CIRC | DMA_CCR_HTIE);
    cha->CCR |= DMA_CCR_TCIE;
  }
  adc->reg->ISR = ADC_ISR_EOC | ADC_ISR_OVR;
  cha->CCR |= DMA_CCR_EN;
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
  // Self-calibration with `DMAEN` clear: each factor reads one short,
  // their mean rounded up goes back once converter is on.
  adc->reg->CFGR1 &= ~ADC_CFGR1_DMAEN;
  adc->reg->CR |= ADC_CR_ADVREGEN;
  ADC_Delay_us(ADC_VREG_STARTUP_us);
  uint32_t sum = 0;
  for(uint8_t i = 0; i < ADC_CAL_RUNS; i++) {
    adc->reg->CR |= ADC_CR_ADCAL;
    while(!(adc->reg->ISR & ADC_ISR_EOCAL)) let();
    adc->reg->ISR = ADC_ISR_EOCAL;
    sum += (adc->reg->CALFACT & ADC_CALFACT_CALFACT) + 1;
  }
  ADC_Enable(adc);
  adc->reg->CALFACT = minv((sum + ADC_CAL_RUNS - 1) / ADC_CAL_RUNS, 0x7Fu);
  #if(ADC_RECORD)
  if(adc->record.chan) {
    DMA_SetRegisters(adc->record.dma, &adc->record._dma);
    RCC_EnableDMA(adc->record._dma.reg);
    adc->record._dma.mux->CCR = (adc->record._dma.mux->CCR & ~0x3Fu) | DMAMUX_REQ_ADC;
    adc->record._dma.cha->CPAR = (uint32_t)&adc->reg->DR;
    adc->record._dma.cha->CCR = DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    IRQ_EnableDMA(adc->record.dma, adc->irq_priority, (IRQ_Handler_t)dma_handler, adc);
  }
  #endif
  ADC_InitGPIO(adc, adc->measure.chan, adc->measure.chan_count);
  #if(ADC_RECORD)
  if(adc->record.chan) {
    ADC_InitGPIO(adc, adc->record.chan, adc->record.chan_count);
  }
  #endif
  adc->reg->IER |= ADC_IER_OVRIE;
  IRQ_EnableADC(adc->irq_priority, (IRQ_Handler_t)irq_handler, adc);
  self = adc;
}

//-------------------------------------------------------------------------------------------------

// hal/stm32wb/per/adc_wb.c

#include "adc.h"

#include "dma.h"

//------------------------------------------------------------------------------------------ Config

// Voltage regulator start-up, datasheet `tADCVREG_STUP`
#define ADC_VREG_STARTUP_us 20

// Internal source start-up after its `ADC_CCR` bit, temperature sensor worst case
#define ADC_SOURCE_STARTUP_us 120

//------------------------------------------------------------------------------------------ Tables

const uint16_t ADC_PRESCALER_TAB[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t ADC_SAMPLING_TIME_TAB[] = { 15, 19, 25, 37, 60, 105, 260, 653 };
const uint16_t ADC_OVERSAMPLING_RATIO_TAB[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

uint32_t ADC_Frequency_Hz(ADC_t *adc)
{
  if(adc->clock == ADC_Clock_PLLP || adc->clock == ADC_Clock_PLLSAI) return 0;
  return SystemCoreClock / ADC_PRESCALER_TAB[adc->prescaler];
}

//---------------------------------------------------------------------------------------- Internal

static ADC_t *self;       // `ADC_Suspend` target, one converter on this family
static uint32_t sources;  // internal sources `ADC_Suspend` switched off

static ADC_Common_TypeDef *adc_common(ADC_t *adc)
{
  unused(adc);
  return ADC1_COMMON;
}

// A conversion over 1ms after the previous one or the calibration reads wrong
// (errata "Wrong ADC result if conversion done late after calibration or previous conversion"),
// so every sequence opens with a conversion of its first channel, dropped.
// `SQ1..SQ4` sit in `SQR1` past the length field, `SQ5..SQ16` fill `SQR2..SQR4` five each.
static bool set_sequence(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  if(!cha || !count || count > 15) return false; // 16 conversions at most, the opening one too
  volatile uint32_t *sqr = &adc->reg->SQR1;
  sqr[0] = (uint32_t)count << ADC_SQR1_L_Pos;
  sqr[1] = 0;
  sqr[2] = 0;
  sqr[3] = 0;
  for(uint8_t i = 0; i <= count; i++) {
    uint32_t ch = cha[i ? i - 1 : 0];
    if(i < 4) sqr[0] |= ch << (6u * (i + 1));
    else sqr[1 + (i - 4) / 5] |= ch << (6u * ((i - 4) % 5));
  }
  return true;
}

static void set_sampling_time(ADC_t *adc, uint8_t *cha, uint8_t count, ADC_SamplingTime_t st)
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

static void set_oversampling(ADC_t *adc, ADC_Oversampling_t *ovs)
{
  adc->reg->CFGR2 =
    (ovs->shift << ADC_CFGR2_OVSS_Pos) |
    (ovs->ratio << ADC_CFGR2_OVSR_Pos) |
    (ovs->enable ? ADC_CFGR2_ROVSE : 0);
}

//----------------------------------------------------------------------------------------- Handler

// An overrun means a sample was lost, and in a scanned sequence that also loses the channel
// alignment of everything that follows, so the run is aborted and counted instead of limping
// on with shifted data. Restart policy belongs to the application: it alone knows whether
// a gap in the stream is acceptable.
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
    uint8_t k = mea->_active++ - mea->_lead; // past the list for a dropped conversion
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

//-------------------------------------------------------------------------------------------- GPIO

void ADC_InitGPIO(ADC_t *adc, uint8_t *cha, uint8_t count)
{
  ADC_Common_TypeDef *common = adc_common(adc);
  uint32_t ccr = common->CCR;
  while(count--) {
    uint8_t ch = *cha++;
    switch(ch) {
      case ADC_IN_VREFEN: ccr |= ADC_CCR_VREFEN; break;
      case ADC_IN_TSEN:   ccr |= ADC_CCR_TSEN; break;
      case ADC_IN_VBATEN: ccr |= ADC_CCR_VBATEN; break;
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
  // Source bits change on a disabled converter, as the vendor library does it
  if(ccr != common->CCR) {
    ADC_Disable(adc);
    common->CCR = ccr;
    ADC_Delay_us(ADC_SOURCE_STARTUP_us);
  }
}

//---------------------------------------------------------------------------------- Enable/Disable

void ADC_Enable(ADC_t *adc)
{
  if(adc->reg->CR & ADC_CR_ADEN) return;
  adc->reg->ISR = ADC_ISR_ADRDY;
  adc->reg->CR |= ADC_CR_ADEN;
  while(!(adc->reg->ISR & ADC_ISR_ADRDY)) __NOP();
}

// Inputs still selected in `SQRx` are clamped to VDD once every analog peripheral is off
// (errata "Selected external ADC inputs unduly clamped to VDD when all analog peripherals
// are disabled"), so the sequence goes before the converter.
void ADC_Disable(ADC_t *adc)
{
  if(adc->reg->CR & ADC_CR_ADSTART) {
    adc->reg->CR |= ADC_CR_ADSTP;
    while(adc->reg->CR & ADC_CR_ADSTP) __NOP();
  }
  adc->reg->SQR1 = 0;
  adc->reg->SQR2 = 0;
  adc->reg->SQR3 = 0;
  adc->reg->SQR4 = 0;
  if(adc->reg->CR & ADC_CR_ADEN) {
    adc->reg->CR |= ADC_CR_ADDIS;
    while(adc->reg->CR & ADC_CR_ADEN) __NOP();
  }
}

// A Stop mode entered with the temperature sensor and its converter on corrupts the internal
// reference (errata "Internal voltage reference corrupted upon Stop mode entry with temperature
// sensing enabled"), and Stop 2 wants the regulator off (RM0434 §16.4.6): all of it goes off,
// the calibration stays.
void ADC_Suspend(void)
{
  if(!self) return;
  if(self->_busy) {
    self->_overrun++;
    ADC_Stop(self);
  }
  ADC_Disable(self);
  ADC_Common_TypeDef *common = adc_common(self);
  sources = common->CCR & (ADC_CCR_TSEN | ADC_CCR_VREFEN | ADC_CCR_VBATEN);
  common->CCR &= ~sources;
  self->reg->CR &= ~ADC_CR_ADVREGEN;
}

void ADC_Resume(void)
{
  if(!self) return;
  self->reg->CR |= ADC_CR_ADVREGEN;
  ADC_Delay_us(ADC_VREG_STARTUP_us);
  adc_common(self)->CCR |= sources;
  if(sources) ADC_Delay_us(ADC_SOURCE_STARTUP_us);
}

//-------------------------------------------------------------------------------------------- Stop

void ADC_Stop(ADC_t *adc)
{
  // `ADSTP` acts on a running conversion only
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
  // Sequence registers take writes on an enabled converter (RM0434 §16.4.10)
  ADC_Enable(adc);
  if(!set_sequence(adc, mea->chan, mea->chan_count)) return ERR;
  adc->_busy = ADC_State_Measure;
  mea->_active = 0;
  mea->_lead = 1;
  mea->_total = mea->chan_count + 1;
  set_oversampling(adc, &mea->oversampling);
  set_sampling_time(adc, mea->chan, mea->chan_count, mea->sampling_time);
  // Single-shot by nature: the sequence ends on its own after the last channel,
  // which keeps the data rate at the interrupt's pace instead of racing a free-running ADC.
  adc->reg->CFGR &= ~(ADC_CFGR_EXTEN | ADC_CFGR_CONT);
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
  if(!rec->_dma.cha) return ERR;
  ADC_Enable(adc);
  if(!set_sequence(adc, rec->chan, rec->chan_count)) return ERR;
  rec->_lead = 1;
  rec->_tail = 0;
  rec->_len = rec->buff_len - rec->buff_len % ADC_RecordStride(adc);
  if(!rec->_len) return ERR;
  adc->_busy = ADC_State_Record;
  set_oversampling(adc, &rec->oversampling);
  set_sampling_time(adc, rec->chan, rec->chan_count, rec->sampling_time);
  DMA_Channel_TypeDef *cha = rec->_dma.cha;
  cha->CCR &= ~DMA_CCR_EN;
  cha->CMAR = (uint32_t)rec->buff;
  cha->CNDTR = rec->_len;
  uint32_t cfgr_rst = ADC_CFGR_EXTSEL_Msk;
  uint32_t cfgr_set;
  if(rec->ext_trig) {
    cfgr_set = ADC_CFGR_EXTEN_0 | (rec->ext_select << ADC_CFGR_EXTSEL_Pos);
    cfgr_rst |= ADC_CFGR_CONT;
  }
  else {
    cfgr_set = ADC_CFGR_CONT;
    cfgr_rst |= ADC_CFGR_EXTEN;
  }
  adc->reg->CFGR = (adc->reg->CFGR & ~cfgr_rst) | cfgr_set;
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
  ADC_Common_TypeDef *common = adc_common(adc);
  // Register route per `ADC_Clock_t`: reset `00` is no clock at all,
  // `Default` is the system clock on this family.
  static const uint8_t clock_sel[] = { 3, 3, 2, 1 };
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_ADCSEL_Msk)
    | ((uint32_t)clock_sel[adc->clock] << RCC_CCIPR_ADCSEL_Pos);
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  common->CCR = (common->CCR & ~ADC_CCR_PRESC_Msk) | (adc->prescaler << ADC_CCR_PRESC_Pos);
  adc->reg->CR &= ~ADC_CR_DEEPPWD;
  adc->reg->CR |= ADC_CR_ADVREGEN;
  ADC_Delay_us(ADC_VREG_STARTUP_us);
  adc->reg->CR &= ~ADC_CR_ADCALDIF;
  adc->reg->CR |= ADC_CR_ADCAL;
  while(adc->reg->CR & ADC_CR_ADCAL) let();
  // `ADEN` four kernel cycles after the calibration at the earliest (RM0434 §16.4.9),
  // 100µs when a PLL route leaves the kernel clock unknown.
  uint32_t freq_Hz = ADC_Frequency_Hz(adc);
  ADC_Delay_us(freq_Hz ? 4000000u / freq_Hz + 1 : 100);
  #if(ADC_RECORD)
  if(adc->record.chan) {
    DMA_SetRegisters(adc->record.dma, &adc->record._dma);
    RCC_EnableDMA(adc->record._dma.reg);
    adc->reg->CFGR &= ~ADC_CFGR_DMAEN;
    adc->record._dma.mux->CCR = (adc->record._dma.mux->CCR & ~0x3Fu) | DMAMUX_REQ_ADC;
    adc->record._dma.cha->CPAR = (uint32_t)&adc->reg->DR;
    adc->record._dma.cha->CCR = DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    adc->reg->CFGR |= ADC_CFGR_DMAEN | ADC_CFGR_DMACFG;
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
  ADC_Enable(adc);
  self = adc;
}

//-------------------------------------------------------------------------------------------------

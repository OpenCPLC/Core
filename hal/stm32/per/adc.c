// hal/stm32/per/adc.c

#include "adc.h"
#include <string.h>

//-------------------------------------------------------------------------------------------------

bool ADC_IsBusy(ADC_t *adc) { return adc->_busy != ADC_State_Free; }
bool ADC_IsFree(ADC_t *adc) { return adc->_busy == ADC_State_Free; }

void ADC_Wait(ADC_t *adc)
{
  while(ADC_IsBusy(adc)) let();
}

uint16_t ADC_Overruns(ADC_t *adc)
{
  uint16_t count = adc->_overrun;
  adc->_overrun -= count;
  return count;
}

//-------------------------------------------------------------------------------------------------

static uint16_t ADC_ReadAs(ADC_t *adc, uint8_t chan, ADC_SamplingTime_t sampling_time,
  ADC_Oversampling_t oversampling)
{
  uint16_t out = 0;
  ADC_InitGPIO(adc, &chan, 1);
  while(ADC_IsBusy(adc)) let();
  ADC_Measure_t save = adc->measure;
  adc->measure.chan = &chan;
  adc->measure.chan_count = 1;
  adc->measure.output = &out;
  adc->measure.sampling_time = sampling_time;
  adc->measure.oversampling = oversampling;
  if(ADC_Measure(adc) == OK) ADC_Wait(adc);
  adc->measure = save;
  return out;
}

uint16_t ADC_Read(ADC_t *adc, uint8_t chan)
{
  if(adc->measure.chan_count) {
    return ADC_ReadAs(adc, chan, adc->measure.sampling_time, adc->measure.oversampling);
  }
  return ADC_ReadAs(adc, chan, ADC_SamplingTime_Max, (ADC_Oversampling_t){0});
}

//-------------------------------------------------------------------------------------------------

// Longest sampling time, no oversampling: the calibration data lives on the native
// 12-bit scale. The first conversion is discarded, it covers the source startup time
static uint16_t ADC_ReadInternal(ADC_t *adc, uint8_t chan)
{
  ADC_ReadAs(adc, chan, ADC_SamplingTime_Max, (ADC_Oversampling_t){0});
  return ADC_ReadAs(adc, chan, ADC_SamplingTime_Max, (ADC_Oversampling_t){0});
}

uint16_t ADC_Vdda_mV(ADC_t *adc)
{
  uint16_t raw = ADC_ReadInternal(adc, ADC_IN_VREFEN);
  if(!raw) return 0;
  return (uint16_t)((uint32_t)ADC_CAL_VDDA_mV * ADC_VREFINT_CAL / raw);
}

float ADC_Temperature_C(ADC_t *adc)
{
  uint16_t vdda = ADC_Vdda_mV(adc);
  float data = (float)ADC_ReadInternal(adc, ADC_IN_TSEN) * vdda / ADC_CAL_VDDA_mV;
  return 30.0f + 100.0f * (data - ADC_TS_CAL1) / (float)(ADC_TS_CAL2 - ADC_TS_CAL1);
}

//-------------------------------------------------------------------------------------------------
#if(ADC_RECORD)

status_t ADC_LastSamples(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort)
{
  if(!adc || !buffer) return ERR;
  DMA_Channel_TypeDef *cha = adc->record._dma.cha;
  uint16_t *src = adc->record.buff;
  uint16_t len = adc->record.buff_len;
  if(!src || !len || !count || count > len) return ERR;
  volatile uint16_t cnt1, cnt2;
  do {
    cnt1 = cha->CNDTR;
    cnt2 = cha->CNDTR;
  } while(cnt1 != cnt2);
  uint16_t cnt = cnt1;
  uint16_t write_idx = (uint16_t)((len - cnt) % len);
  uint16_t start_idx = (uint16_t)((write_idx + len - count) % len);
  if(!sort) {
    if(start_idx + count <= len) {
      memcpy(buffer, &src[start_idx], (size_t)count * sizeof(uint16_t));
    }
    else {
      uint16_t first = (uint16_t)(len - start_idx);
      memcpy(buffer, &src[start_idx], (size_t)first * sizeof(uint16_t));
      memcpy(buffer + first, &src[0], (size_t)(count - first) * sizeof(uint16_t));
    }
    return OK;
  }
  // Stride is the scan: channels plus `_pad`. The pad repeats the last channel, drop it
  uint16_t n = adc->record.chan_count;
  uint16_t scan = (uint16_t)(n + adc->record._pad);
  if(n == 0 || (len % scan) != 0) return ERR;
  uint16_t samples = count / scan;
  if(samples == 0) return ERR;
  // A scan-aligned window keeps every channel at a fixed offset
  uint16_t end_idx = (uint16_t)(write_idx - (write_idx % scan));
  start_idx = (uint16_t)((end_idx + len - (samples * scan)) % len);
  for(uint16_t t = 0; t < samples; t++) {
    for(uint16_t c = 0; c < n; c++) {
      buffer[c * samples + t] = src[(start_idx + t * scan + c) % len];
    }
  }
  return OK;
}

uint16_t ADC_RecordPosition(ADC_t *adc)
{
  uint16_t len = adc->record.buff_len;
  if(!len) return 0;
  return (uint16_t)((len - adc->record._dma.cha->CNDTR) % len);
}

float ADC_RecordScanTime_s(ADC_t *adc)
{
  uint32_t hz = ADC_Frequency_Hz(adc);
  float freq = hz ? (float)hz : (float)SystemCoreClock; // a PLL route is the caller's guess
  float cycles = (float)ADC_SAMPLING_TIME_TAB[adc->record.sampling_time];
  float time = (float)(adc->record.chan_count + adc->record._pad) * cycles / freq;
  if(adc->record.oversampling.enable) {
    time *= (float)ADC_OVERSAMPLING_RATIO_TAB[adc->record.oversampling.ratio];
  }
  return time;
}

#endif
//-------------------------------------------------------------------------------------------------

// hal/stm32/per/adc.c

#include "adc.h"

#include <string.h>

//------------------------------------------------------------------------------------------- State

bool ADC_IsBusy(ADC_t *adc) { return adc->_busy != ADC_State_Free; }
bool ADC_IsFree(ADC_t *adc) { return adc->_busy == ADC_State_Free; }

void ADC_Wait(ADC_t *adc)
{
  while(ADC_IsBusy(adc)) let();
}

uint16_t ADC_Overruns(ADC_t *adc)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t count = adc->_overrun;
  adc->_overrun = 0;
  __set_PRIMASK(primask);
  return count;
}

//-------------------------------------------------------------------------------------------- Read

// One channel through `measure` job, configuration is restored after.
// Pin or internal source is set up only once ADC is free:
// source bit takes writes with no conversion running.
static uint16_t read_as(ADC_t *adc, uint8_t chan,
  ADC_SamplingTime_t sampling_time, ADC_Oversampling_t oversampling)
{
  uint16_t out = 0;
  while(ADC_IsBusy(adc)) let();
  ADC_InitGPIO(adc, &chan, 1);
  ADC_Measure_t save = adc->measure;
  adc->measure.chan = &chan;
  adc->measure.chan_count = 1;
  adc->measure.output = &out;
  adc->measure.sampling_time = sampling_time;
  adc->measure.oversampling = oversampling;
  if(!ADC_Measure(adc)) ADC_Wait(adc);
  adc->measure = save;
  return out;
}

uint16_t ADC_Read(ADC_t *adc, uint8_t chan)
{
  if(adc->measure.chan_count) {
    return read_as(adc, chan, adc->measure.sampling_time, adc->measure.oversampling);
  }
  return read_as(adc, chan, ADC_SamplingTime_Max, (ADC_Oversampling_t){0});
}

// Longest sampling time, no oversampling: calibration data lives on native 12-bit scale
static uint16_t read_internal(ADC_t *adc, uint8_t chan) {
  return read_as(adc, chan, ADC_SamplingTime_Max, (ADC_Oversampling_t){0});
}

uint16_t ADC_Vdda_mV(ADC_t *adc)
{
  uint16_t raw = read_internal(adc, ADC_IN_VREFEN);
  if(!raw) return 0;
  return (uint16_t)((uint32_t)ADC_CAL_VDDA_mV * ADC_VREFINT_CAL / raw);
}

float ADC_Temperature_C(ADC_t *adc)
{
  uint16_t vdda = ADC_Vdda_mV(adc);
  float data = (float)read_internal(adc, ADC_IN_TSEN) * vdda / ADC_CAL_VDDA_mV;
  return 30.0f + 100.0f * (data - ADC_TS_CAL1) / (float)(ADC_TS_CAL2 - ADC_TS_CAL1);
}

//------------------------------------------------------------------------------------------ Record
#if(ADC_RECORD)

uint8_t ADC_RecordStride(ADC_t *adc)
{
  return adc->record._lead + adc->record.chan_count + adc->record._tail;
}

uint16_t ADC_RecordPosition(ADC_t *adc)
{
  uint16_t len = adc->record._len;
  if(!len) return 0;
  return (uint16_t)((len - adc->record._dma.cha->CNDTR) % len);
}

const uint16_t *ADC_RecordScan(ADC_t *adc)
{
  uint16_t stride = ADC_RecordStride(adc);
  uint16_t scans = stride ? adc->record._len / stride : 0;
  if(!scans) return NULL;
  // Scan behind DMA writer, wrapping to end of buffer
  uint16_t newest = (ADC_RecordPosition(adc) / stride + scans - 1) % scans;
  return &adc->record.buff[newest * stride + adc->record._lead];
}

status_t ADC_LastSamples(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort)
{
  if(!adc || !buffer) return ERR;
  uint16_t *src = adc->record.buff;
  uint16_t len = adc->record._len;
  if(!src || !len || !count || count > len) return ERR;
  uint16_t write_idx = ADC_RecordPosition(adc);
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
  // Results only: scan-aligned window keeps every channel at fixed offset
  uint16_t n = adc->record.chan_count;
  uint16_t scan = ADC_RecordStride(adc);
  uint16_t samples = n ? count / n : 0;
  if(!samples) return ERR;
  // While recording, scan under DMA writer mixes two passes
  uint16_t complete = len / scan - (adc->_busy == ADC_State_Record ? 1 : 0);
  if(samples > complete) return ERR;
  uint16_t end_idx = (uint16_t)(write_idx - (write_idx % scan));
  start_idx = (uint16_t)((end_idx + len - (samples * scan)) % len);
  for(uint16_t t = 0; t < samples; t++) {
    for(uint16_t c = 0; c < n; c++) {
      buffer[c * samples + t] = src[(start_idx + t * scan + adc->record._lead + c) % len];
    }
  }
  return OK;
}

float ADC_RecordScanTime_s(ADC_t *adc)
{
  // PLL route leaves frequency unknown: core clock is the caller's guess
  uint32_t freq_Hz = ADC_Frequency_Hz(adc);
  if(!freq_Hz) freq_Hz = SystemCoreClock;
  float cycles = (float)ADC_SAMPLING_TIME_TAB[adc->record.sampling_time];
  float time_s = (float)ADC_RecordStride(adc) * cycles / (float)freq_Hz;
  if(adc->record.oversampling.enable) {
    time_s *= (float)ADC_OVERSAMPLING_RATIO_TAB[adc->record.oversampling.ratio];
  }
  return time_s;
}

uint32_t ADC_SampleDelay_ns(ADC_t *adc, uint8_t k)
{
  uint32_t freq_Hz = ADC_Frequency_Hz(adc);
  if(!freq_Hz) return 0;
  ADC_Record_t *rec = &adc->record;
  uint32_t ratio = rec->oversampling.enable
    ? ADC_OVERSAMPLING_RATIO_TAB[rec->oversampling.ratio] : 1;
  // In half kernel cycles, which keep 12.5 conversion cycles whole:
  // trigger latency, first-conversion cycle, every conversion ahead, then its own sampling.
  uint32_t conv_half = 2u * ADC_SAMPLING_TIME_TAB[rec->sampling_time];
  uint32_t delay_half = ADC_TRIGGER_LATENCY_HALF + 2 * adc_first_extra_cycles(rec->sampling_time)
    + (rec->_lead + k) * ratio * conv_half + (conv_half - 25);
  return (uint32_t)(((uint64_t)delay_half * 500000000u + freq_Hz / 2) / freq_Hz);
}

#endif
//---------------------------------------------------------------------------------------- Internal

void ADC_Delay_us(uint32_t us)
{
  // Two core cycles a pass at least, so wait never comes out short
  for(uint32_t n = SystemCoreClock / 2000000u * us + 1; n; n--) __NOP();
}

//-------------------------------------------------------------------------------------------------

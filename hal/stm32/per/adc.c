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
  uint16_t n = adc->record.chan_count;
  if(n == 0 || (len % n) != 0) return ERR;
  uint16_t samples = count / n;
  if(samples == 0) return ERR;
  uint16_t used = samples * n;
  start_idx = (uint16_t)((write_idx + len - used) % len);
  uint16_t ch0 = start_idx % n;
  for(uint16_t i = 0; i < used; i++) {
    uint16_t src_idx = (start_idx + i) % len;
    uint16_t pos = i % n;
    uint16_t t = i / n;
    uint16_t ch = (ch0 + pos) % n;
    buffer[ch * samples + t] = src[src_idx];
  }
  return OK;
}

float ADC_RecordSampleTime_s(ADC_t *adc)
{
  float freq = (adc->use_hsi ? 16000000.0f : (float)SystemCoreClock);
  freq /= (float)ADC_PRESCALER_TAB[adc->record.prescaler];
  float cycles = (float)ADC_SAMPLING_TIME_TAB[adc->record.sampling_time];
  float time = (float)adc->record.chan_count * cycles / freq;
  if(adc->record.oversampling.enable) {
    time *= (float)ADC_OVERSAMPLING_RATIO_TAB[adc->record.oversampling.ratio];
  }
  return time;
}

#endif
//-------------------------------------------------------------------------------------------------

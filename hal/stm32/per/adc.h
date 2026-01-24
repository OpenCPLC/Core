#ifndef ADC_H_
#define ADC_H_
//-------------------------------------------------------------------------------------------------

#include "irq.h"
#include "gpio.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

#ifndef ADC_RECORD
  #define ADC_RECORD 1
#endif

#if(ADC_RECORD)
  #include "tim.h"
#endif

#if defined(STM32G0)
  #include "adc-g0.h"
#elif defined(STM32WB)
  #include "adc-wb.h"
#endif

#define adc_record_buffer_size(time_ms, sample_time, oversampling, channel_count) \
  (uint16_t)(channel_count * (time_ms * 16000 / sample_time / oversampling / channel_count))

#define resistor_divider_factor(vcc, up, down, resolution) \
  ((float)vcc * ((float)up + down) / down / ((1 << resolution) - 1))

#define volts_factor(vcc, resolution) \
  ((float)vcc / (float)((1 << resolution) - 1))

//-------------------------------------------------------------------------------------------------

typedef enum {
  ADC_State_Free = 0,
  ADC_State_Measure = 1,
  ADC_State_Record = 2
} ADC_State_t;

typedef enum {
  ADC_OversamplingRatio_2 = 0,
  ADC_OversamplingRatio_4 = 1,
  ADC_OversamplingRatio_8 = 2,
  ADC_OversamplingRatio_16 = 3,
  ADC_OversamplingRatio_32 = 4,
  ADC_OversamplingRatio_64 = 5,
  ADC_OversamplingRatio_128 = 6,
  ADC_OversamplingRatio_256 = 7
} ADC_OversamplingRatio_t;

typedef enum {
  ADC_Prescaler_1 = 0,
  ADC_Prescaler_2 = 1,
  ADC_Prescaler_4 = 2,
  ADC_Prescaler_6 = 3,
  ADC_Prescaler_8 = 4,
  ADC_Prescaler_10 = 5,
  ADC_Prescaler_12 = 6,
  ADC_Prescaler_16 = 7,
  ADC_Prescaler_32 = 8,
  ADC_Prescaler_64 = 9,
  ADC_Prescaler_128 = 10,
  ADC_Prescaler_256 = 11
} ADC_Prescaler_t;

//-------------------------------------------------------------------------------------------------

typedef struct {
  bool enable;
  ADC_OversamplingRatio_t ratio;
  uint8_t shift; // 0-8 [bit]
} ADC_Oversampling_t;

typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  uint16_t *output;
  ADC_Prescaler_t prescaler;
  ADC_SamplingTime_t sampling_time;
  ADC_Oversampling_t oversampling;
  uint8_t active;
} ADC_Measure_t;

#if(ADC_RECORD)
typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  DMA_Nbr_t dma_nbr;
  ADC_Prescaler_t prescaler;
  ADC_SamplingTime_t sampling_time;
  ADC_Oversampling_t oversampling;
  bool continuous_mode;
  uint16_t *buff;
  uint16_t buff_len;
  TIM_t *tim;
  DMA_t dma;
} ADC_Record_t;
#endif

typedef struct {
  IRQ_Priority_t int_prioryty;
  bool freq_16Mhz;
  ADC_Prescaler_t prescaler;
  ADC_Measure_t measure;
  #if(ADC_RECORD)
    ADC_Record_t record;
  #endif
  uint16_t overrun;
  volatile uint8_t busy;
} ADC_t;

//-------------------------------------------------------------------------------------------------

uint8_t ADC_Measure(ADC_t *adc);
#if(ADC_RECORD)
  uint8_t ADC_Record(ADC_t *adc);
  status_t ADC_LastMeasurements(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort);
  float ADC_RecordSampleTime_s(ADC_t *adc);
#endif
bool ADC_IsBusy(ADC_t *adc);
bool ADC_IsFree(ADC_t *adc);
void ADC_Wait(ADC_t *adc);
void ADC_Stop(ADC_t *adc);
void ADC_Enable(void);
void ADC_Disable(void);

void ADC_Init(ADC_t *adc);

//-------------------------------------------------------------------------------------------------

extern const uint32_t AdcFrequencyTab[];
extern const uint16_t AdcSamplingTimeTab[];
extern const uint16_t AdcOversamplingRatioTab[];

//-------------------------------------------------------------------------------------------------
#endif

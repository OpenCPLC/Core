// hal/stm32/per/adc.h

#ifndef ADC_H_
#define ADC_H_

#include "irq.h"
#include "dma.h"
#include "gpio.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

#ifndef ADC_RECORD
  #define ADC_RECORD 1
#endif

#if defined(STM32G0)
  #include "adc_g0.h"
#elif defined(STM32WB)
  #include "adc_wb.h"
#elif defined(STM32G4)
  #include "adc_g4.h"
#endif

//------------------------------------------------------------------------------------------------- Macros

#define adc_record_buffer_size(time_ms, sample_time, oversampling, channel_count) \
  (uint16_t)((channel_count) * ((time_ms) * 16000 / (sample_time) / (oversampling) / (channel_count)))

#define resistor_divider_factor(vcc, up, down, resolution) \
  ((float)(vcc) * ((float)(up) + (down)) / (down) / ((1 << (resolution)) - 1))

#define volts_factor(vcc, resolution) \
  ((float)(vcc) / (float)((1 << (resolution)) - 1))

//------------------------------------------------------------------------------------------------- Types

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

//------------------------------------------------------------------------------------------------- Structures

/**
 * @brief ADC oversampling configuration.
 * @param[in] enable Enable oversampling
 * @param[in] ratio Oversampling ratio (2x to 256x)
 * @param[in] shift Right shift for result (0-8 bits)
 */
typedef struct {
  bool enable;
  ADC_OversamplingRatio_t ratio;
  uint8_t shift;
} ADC_Oversampling_t;

/**
 * @brief ADC single/multi-channel measurement configuration.
 * @param[in] chan Pointer to channel array
 * @param[in] chan_count Number of channels
 * @param[in] output Pointer to output buffer (size >= `chan_count`)
 * @param[in] prescaler ADC clock prescaler
 * @param[in] sampling_time Sampling time setting
 * @param[in] oversampling Oversampling configuration
 * Internal:
 * @param _active Current channel index during conversion
 */
typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  uint16_t *output;
  ADC_Prescaler_t prescaler;
  ADC_SamplingTime_t sampling_time;
  ADC_Oversampling_t oversampling;
  uint8_t _active;
} ADC_Measure_t;

#if(ADC_RECORD)

/** @brief DMA callback type for continuous mode */
typedef void (*ADC_DmaCallback_t)(void *arg);

/**
 * @brief ADC DMA recording configuration.
 * @param[in] chan Pointer to channel array
 * @param[in] chan_count Number of channels
 * @param[in] dma DMA channel number
 * @param[in] prescaler ADC clock prescaler
 * @param[in] sampling_time Sampling time setting
 * @param[in] oversampling Oversampling configuration
 * @param[in] continuous_mode Circular DMA mode (continuous recording)
 * @param[in] buff Pointer to DMA buffer
 * @param[in] buff_len Buffer length in samples
 * @param[in] tim Optional timer for triggered sampling (`NULL` = continuous)
 * @param[in] HalfCallback Called when first half of buffer filled (continuous mode, NULL = disabled)
 * @param[in] CompleteCallback Called when buffer complete (continuous mode, NULL = disabled)
 * @param[in] callback_arg User argument passed to callbacks
 * Internal:
 * @param _dma DMA registers structure
 */
typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  DMA_CHx_t dma;
  ADC_Prescaler_t prescaler;
  ADC_SamplingTime_t sampling_time;
  ADC_Oversampling_t oversampling;
  bool continuous_mode;
  bool ext_trig;
  ADC_ExtTrig_t ext_select;
  uint16_t *buff;
  uint16_t buff_len;
  ADC_DmaCallback_t HalfCallback;
  ADC_DmaCallback_t CompleteCallback;
  void *callback_arg;
  DMA_t _dma;
} ADC_Record_t;
#endif

/**
 * @brief ADC controller structure.
 * @param[in] reg Pointer to ADC peripheral registers (`ADC1`, `ADC2`, etc.)
 * @param[in] irq_priority Interrupt priority for ADC and DMA
 * @param[in] use_hsi Use HSI as ADC clock source (instead of system clock)
 * @param[in] prescaler Initial prescaler (tracked to avoid unnecessary reconfiguration)
 * @param[in] measure Single/multi-channel measurement configuration
 * @param[in] record DMA recording configuration (if `ADC_RECORD` enabled)
 * Internal:
 * @param _busy Current ADC state
 * @param _overrun Overrun error counter
 */
typedef struct {
  ADC_TypeDef *reg;
  IRQ_Priority_t irq_priority;
  bool use_hsi;
  ADC_Prescaler_t prescaler;
  ADC_Measure_t measure;
  #if(ADC_RECORD)
    ADC_Record_t record;
  #endif
  volatile ADC_State_t _busy;
  uint16_t _overrun;
} ADC_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize ADC peripheral.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Init(ADC_t *adc);

/**
 * @brief Start single/multi-channel measurement (interrupt mode).
 * @param[in,out] adc Pointer to ADC structure
 * @return `OK` if started, `BUSY` if conversion in progress
 */
status_t ADC_Measure(ADC_t *adc);

#if(ADC_RECORD)
/**
 * @brief Start DMA recording.
 * @param[in,out] adc Pointer to ADC structure
 * @return `OK` if started, `BUSY` if conversion in progress
 */
status_t ADC_Record(ADC_t *adc);

/**
 * @brief Copy last samples from circular DMA buffer.
 * @param[in] adc Pointer to ADC structure
 * @param[out] buffer Output buffer
 * @param[in] count Number of samples to copy
 * @param[in] sort `true` = deinterleave into channel blocks
 * @return `OK` on success, `ERR` on invalid parameters
 */
status_t ADC_LastSamples(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort);

/**
 * @brief Calculate sample time for one complete sequence.
 * @param[in] adc Pointer to ADC structure
 * @return Sample time in seconds
 */
float ADC_RecordSampleTime_s(ADC_t *adc);
#endif

/**
 * @brief Stop current conversion.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Stop(ADC_t *adc);

/**
 * @brief Check if ADC is busy.
 * @param[in] adc Pointer to ADC structure
 * @return `true` if busy
 */
bool ADC_IsBusy(ADC_t *adc);

/**
 * @brief Check if ADC is free.
 * @param[in] adc Pointer to ADC structure
 * @return `true` if free
 */
bool ADC_IsFree(ADC_t *adc);

/**
 * @brief Wait for conversion to complete.
 * @param[in] adc Pointer to ADC structure
 */
void ADC_Wait(ADC_t *adc);

/**
 * @brief Enable ADC (wait for ADRDY).
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Enable(ADC_t *adc);

/**
 * @brief Disable ADC (stop conversion, wait for disable).
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Disable(ADC_t *adc);

//------------------------------------------------------------------------------------------------- Tables

extern const uint16_t ADC_PRESCALER_TAB[];
extern const uint16_t ADC_SAMPLING_TIME_TAB[];
extern const uint16_t ADC_OVERSAMPLING_RATIO_TAB[];

//-------------------------------------------------------------------------------------------------
#endif
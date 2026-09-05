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

//------------------------------------------------------------------------------------------ Macros

// Buffer length (in samples) that holds `time_ms` of recording,
// rounded down to whole scans. The kernel frequency is the caller's to state,
// `ADC_Frequency_Hz` tells it at runtime
#define adc_record_buffer_size(freq_Hz, time_ms, sample_time, oversampling, channel_count) \
  (uint16_t)((channel_count) * \
    ((time_ms) * ((freq_Hz) / 1000) / (sample_time) / (oversampling) / (channel_count)))

// Multiply a raw conversion by this factor to get the voltage at the top of a resistor divider
#define resistor_divider_factor(vcc, up, down, resolution) \
  ((float)(vcc) * ((float)(up) + (down)) / (down) / ((1 << (resolution)) - 1))

// Multiply a raw conversion by this factor to get the voltage at the ADC pin
#define volts_factor(vcc, resolution) \
  ((float)(vcc) / (float)((1 << (resolution)) - 1))

// Full-scale count of a hardware-oversampled result: `ratio` (enum) samples accumulated,
// then shifted; the maximum is a multiple of 4095, not a power of two minus one
#define adc_oversampling_max(ratio, shift) \
  ((4095u << ((ratio) + 1)) >> (shift))

// Number of accumulated samples for an oversampling ratio enum value
#define adc_oversampling_samples(ratio) (2u << (ratio))

//------------------------------------------------------------------------------------------- Types

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

//-------------------------------------------------------------------------------------- Structures

/**
 * @brief Hardware oversampling: each result is the average of `ratio` back-to-back samples.
 * With `shift` equal to log2 of the ratio the result stays on the native 12-bit scale;
 * a smaller shift extends the effective resolution instead.
 * @param[in] enable Enable oversampling
 * @param[in] ratio Samples averaged per result (2x to 256x)
 * @param[in] shift Right shift applied to the accumulated sum (0-8 bits)
 */
typedef struct {
  bool enable;
  ADC_OversamplingRatio_t ratio;
  uint8_t shift;
} ADC_Oversampling_t;

/**
 * @brief One-shot conversion of the `chan` list, paced by the end-of-conversion interrupt.
 * Start with `ADC_Measure` and wait with `ADC_Wait`,
 * or use the `ADC_Read` shortcut for a single channel.
 * @param[in] chan Channel list, converted in this order when the sequencer allows it
 * @param[in] chan_count Number of channels
 * @param[in] output Result buffer, at least `chan_count` long
 * @param[in] sampling_time Total conversion time per channel (see the family enum)
 * @param[in] oversampling Oversampling configuration
 * Internal:
 * @param _active Result index of the conversion in progress
 */
typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  uint16_t *output;
  ADC_SamplingTime_t sampling_time;
  ADC_Oversampling_t oversampling;
  uint8_t _active;
} ADC_Measure_t;

#if(ADC_RECORD)

/** @brief DMA callback, executed in interrupt context: set a flag and leave */
typedef void (*ADC_DmaCallback_t)(void *arg);

/**
 * @brief Free-running acquisition of the `chan` list into a DMA buffer,
 * started with `ADC_Record`.
 * In `continuous_mode` the buffer is circular and the stream never stops:
 * read it with `ADC_LastSamples` or react to the half/complete callbacks.
 * Otherwise the recording fills the buffer once and stops.
 * With `ext_trig` each sequence is started by the selected timer event
 * instead of free-running, which pins every scan to a known moment of the timer period.
 * @param[in] chan Channel list, converted in this order when the sequencer allows it
 * @param[in] chan_count Number of channels
 * @param[in] dma DMA channel number
 * @param[in] sampling_time Total conversion time per channel (see the family enum)
 * @param[in] oversampling Oversampling configuration
 * @param[in] continuous_mode Circular DMA, stream runs until stopped
 * @param[in] ext_trig Start each sequence on a hardware trigger
 * @param[in] ext_select Trigger source, used when `ext_trig` is set
 * @param[in] buff DMA buffer, a multiple of `chan_count` keeps scans aligned
 * @param[in] buff_len Buffer length in samples
 * @param[in] HalfCallback Called when the first half of the buffer is filled (`NULL` = off)
 * @param[in] CompleteCallback Called when the buffer wraps or fills (`NULL` = off)
 * @param[in] callback_arg User argument passed to both callbacks
 * Internal:
 * @param _dma DMA register set resolved from `dma`
 * @param _pad Sacrificial conversions appended per scan (errata, see `adc_scan_len`)
 */
typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  DMA_CHx_t dma;
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
  uint8_t _pad;
} ADC_Record_t;
#endif

/**
 * @brief ADC controller. Fill the configuration, call `ADC_Init` once,
 * then start work with `ADC_Measure`, `ADC_Record` or `ADC_Read`.
 * One job runs at a time: starting another returns `BUSY`
 * until the current one completes or `ADC_Stop` is called.
 * @param[in] reg ADC peripheral registers, `NULL` selects `ADC1`
 * @param[in] irq_priority Interrupt priority for the ADC and its DMA channel
 * @param[in] clock Kernel clock route (`ADC_Clock_...`), zero = family default
 * @param[in] prescaler ADC clock prescaler, common to every job on this ADC
 * @param[in] measure One-shot conversion configuration
 * @param[in] record DMA recording configuration (when `ADC_RECORD` is enabled)
 * Internal:
 * @param _busy Job in progress
 * @param _overrun Count of aborted runs due to data overrun
 */
typedef struct {
  ADC_TypeDef *reg;
  IRQ_Priority_t irq_priority;
  ADC_Clock_t clock;
  ADC_Prescaler_t prescaler;
  ADC_Measure_t measure;
  #if(ADC_RECORD)
    ADC_Record_t record;
  #endif
  volatile ADC_State_t _busy;
  uint16_t _overrun;
} ADC_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize the peripheral: clocking, voltage regulator, self-calibration, analog
 * pins for every configured channel, DMA and interrupts. Call once before any conversion.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Init(ADC_t *adc);

/**
 * @brief Start the one-shot conversion described by `measure`. Returns immediately;
 * results land in `measure.output` and the ADC frees itself after the last channel.
 * @param[in,out] adc Pointer to ADC structure
 * @return `OK` when started, `BUSY` when another job is in progress
 */
status_t ADC_Measure(ADC_t *adc);

/**
 * @brief Blocking single-channel read.
 * The pin is switched to analog mode on demand,
 * so the channel does not have to be listed in any configuration.
 * Yields to the scheduler until the ADC is free, then while the conversion runs.
 * Timing follows the `measure` configuration;
 * without one, the longest sampling time is used with oversampling off,
 * so the result stays on the native 12-bit scale.
 * @param[in,out] adc Pointer to ADC structure
 * @param[in] chan Channel number (`ADC_IN_...`)
 * @return Raw conversion result
 */
uint16_t ADC_Read(ADC_t *adc, uint8_t chan);

/**
 * @brief Kernel clock frequency of the configured route after the prescaler.
 * @param[in] adc Pointer to ADC structure
 * @return Frequency [Hz], `0` when the framework cannot know it (a PLL route)
 */
uint32_t ADC_Frequency_Hz(ADC_t *adc);

/**
 * @brief Supply voltage computed from the internal reference (`ADC_IN_VREFEN`)
 * and its factory calibration, so the result does not rely on any assumed VDDA.
 * @param[in,out] adc Pointer to ADC structure
 * @return VDDA in millivolts, `0` on a failed conversion
 */
uint16_t ADC_Vdda_mV(ADC_t *adc);

/**
 * @brief Internal temperature sensor (`ADC_IN_TSEN`) read through the two-point factory
 * calibration, compensated for the actual supply voltage.
 * @param[in,out] adc Pointer to ADC structure
 * @return Junction temperature in °C
 */
float ADC_Temperature_C(ADC_t *adc);

#if(ADC_RECORD)
/**
 * @brief Start the DMA recording described by `record`.
 * @param[in,out] adc Pointer to ADC structure
 * @return `OK` when started, `BUSY` when another job is in progress
 */
status_t ADC_Record(ADC_t *adc);

/**
 * @brief Copy the most recent samples from the circular DMA buffer.
 * With `sort` the copy is deinterleaved into channel blocks aligned to scan boundaries,
 * so a one-scan copy holds in `buffer[k]` the latest complete result of channel `k`.
 * @param[in] adc Pointer to ADC structure
 * @param[out] buffer Output buffer
 * @param[in] count Number of samples to copy
 * @param[in] sort `true` = deinterleave into channel blocks
 * @return `OK` on success, `ERR` on invalid arguments
 */
status_t ADC_LastSamples(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort);

/**
 * @brief Position of the DMA writer inside the record buffer:
 * how many samples of the current pass are already written.
 * Lets a reader pick data the DMA is not touching.
 * @param[in] adc Pointer to ADC structure
 * @return Write position in samples, `0` to `buff_len - 1`
 */
uint16_t ADC_RecordPosition(ADC_t *adc);

/**
 * @brief Duration of one complete scan of the `record` sequence, including oversampling.
 * @param[in] adc Pointer to ADC structure
 * @return Scan time in seconds
 */
float ADC_RecordScanTime_s(ADC_t *adc);
#endif

/**
 * @brief Number of runs aborted by data overrun since the last call;
 * reading clears the counter.
 * Restart policy stays with the application:
 * it alone knows whether a gap in the stream is acceptable.
 * @param[in,out] adc Pointer to ADC structure
 * @return Overrun count
 */
uint16_t ADC_Overruns(ADC_t *adc);

/**
 * @brief Abort the job in progress and free the ADC.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Stop(ADC_t *adc);

/** @brief `true` while a job is in progress */
bool ADC_IsBusy(ADC_t *adc);

/** @brief `true` when the ADC is free to start a job */
bool ADC_IsFree(ADC_t *adc);

/** @brief Yield to the scheduler until the job in progress completes */
void ADC_Wait(ADC_t *adc);

/**
 * @brief Enable the ADC and wait until it is ready.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Enable(ADC_t *adc);

/**
 * @brief Stop any conversion and disable the ADC.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Disable(ADC_t *adc);

//---------------------------------------------------------------------------------------- Internal

// Enable analog mode, or the internal source, for every channel on the list (per family)
void ADC_InitGPIO(ADC_t *adc, uint8_t *chan, uint8_t count);

extern const uint16_t ADC_PRESCALER_TAB[];
extern const uint16_t ADC_SAMPLING_TIME_TAB[];
extern const uint16_t ADC_OVERSAMPLING_RATIO_TAB[];

//-------------------------------------------------------------------------------------------------
#endif

// hal/stm32/per/adc.h

#ifndef ADC_H_
#define ADC_H_

#include "irq.h"
#include "dma.h"
#include "gpio.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef ADC_RECORD
  // DMA recording API, `ADC_Record_t`; off leaves one-shot conversions only
  #define ADC_RECORD 1
#endif

//---------------------------------------------------------------------------------- Family include

#if defined(STM32G0)
  #include "adc_g0.h"
#elif defined(STM32WB)
  #include "adc_wb.h"
#endif

//------------------------------------------------------------------------------------------ Macros

// Words one scan of `channel_count` channels can take in record buffer, padding included
#define adc_record_scan_max(channel_count) \
  ((uint16_t)(channel_count) + adc_record_pad_max(channel_count))

/**
 * @brief Buffer length that holds `time_ms` of recording, rounded down to whole scans.
 * @param[in] freq_Hz Kernel clock, the caller's to state: `ADC_Frequency_Hz` tells it at runtime
 * @param[in] time_ms Recording time
 * @param[in] cycles Conversion cycles of one sample, sampling included
 * @param[in] oversampling Samples accumulated per result, `1` without oversampling
 * @param[in] scan_len Words per scan, `adc_record_scan_max`
 * @return Length in samples
 */
#define adc_record_buffer_size(freq_Hz, time_ms, cycles, oversampling, scan_len) \
  (uint16_t)((scan_len) * \
    ((time_ms) * ((freq_Hz) / 1000) / (cycles) / (oversampling) / (scan_len)))

// Multiply raw conversion by this factor to get voltage at top of resistor divider
#define resistor_divider_factor(vcc, up, down, resolution) \
  ((float)(vcc) * ((float)(up) + (down)) / (down) / ((1 << (resolution)) - 1))

// Multiply raw conversion by this factor to get voltage at ADC pin
#define volts_factor(vcc, resolution) \
  ((float)(vcc) / (float)((1 << (resolution)) - 1))

// Full-scale count of hardware-oversampled result: `ratio` (enum) samples accumulated,
// then shifted; maximum is a multiple of 4095, not a power of two minus one.
#define adc_oversampling_max(ratio, shift) \
  ((4095u << ((ratio) + 1)) >> (shift))

// Number of accumulated samples for oversampling ratio enum value
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
 * @brief Hardware oversampling: each result is average of `ratio` back-to-back samples.
 * With `shift` equal to log2 of ratio, result stays on native 12-bit scale;
 * smaller shift extends effective resolution instead.
 * @param[in] enable Enable oversampling
 * @param[in] ratio Samples averaged per result (2× to 256×)
 * @param[in] shift Right shift applied to accumulated sum (0-8 bits)
 */
typedef struct {
  bool enable;
  ADC_OversamplingRatio_t ratio;
  uint8_t shift;
} ADC_Oversampling_t;

/**
 * @brief One-shot conversion of `chan` list, paced by end-of-conversion interrupt.
 * Start with `ADC_Measure` and wait with `ADC_Wait`,
 * or use `ADC_Read` shortcut for single channel.
 * @param[in] chan Channel list, converted in this order
 * @param[in] chan_count Number of channels
 * @param[in] output Result buffer, at least `chan_count` long
 * @param[in] sampling_time Total conversion time per channel (see family enum)
 * @param[in] oversampling Oversampling configuration
 * Internal:
 * @param _active Conversion in progress
 * @param _lead Sacrificial conversions ahead of list
 * @param _total Conversions in sequence
 */
typedef struct {
  uint8_t *chan;
  uint8_t chan_count;
  uint16_t *output;
  ADC_SamplingTime_t sampling_time;
  ADC_Oversampling_t oversampling;
  // internal
  uint8_t _active;
  uint8_t _lead;
  uint8_t _total;
} ADC_Measure_t;

#if(ADC_RECORD)

// DMA callback, interrupt context: set flag and leave
typedef void (*ADC_DmaCallback_t)(void *arg);

/**
 * @brief Free-running acquisition of `chan` list into DMA buffer,
 * started with `ADC_Record`.
 * In `continuous_mode`, buffer is circular and stream never stops:
 * read it with `ADC_LastSamples` or `ADC_RecordScan`, or react to half/complete callbacks.
 * Otherwise recording fills buffer once and stops.
 * Until buffer is filled once, readers can return slots the DMA has not written yet.
 * With `ext_trig` each sequence is started by selected timer event instead of free-running,
 * which pins every scan to a known moment of timer period.
 * @param[in] chan Channel list, converted in this order
 * @param[in] chan_count Number of channels
 * @param[in] dma DMA channel number
 * @param[in] sampling_time Total conversion time per channel (see family enum)
 * @param[in] oversampling Oversampling configuration
 * @param[in] continuous_mode Circular DMA, stream runs until stopped
 * @param[in] ext_trig Start each sequence on hardware trigger
 * @param[in] ext_select Trigger source, used when `ext_trig` is set
 * @param[in] buff DMA buffer, sized with `adc_record_scan_max`; whole scans of it are used
 * @param[in] buff_len Buffer length in samples
 * @param[in] HalfCallback Called when first half of buffer is filled (`NULL` = off)
 * @param[in] CompleteCallback Called when buffer wraps or fills (`NULL` = off)
 * @param[in] callback_arg User argument passed to both callbacks
 * Internal:
 * @param _dma DMA register set resolved from `dma`
 * @param _lead Sacrificial conversions opening every scan
 * @param _tail Sacrificial conversions closing every scan
 * @param _len Buffer samples in use, whole scans
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
  // internal
  DMA_t _dma;
  uint8_t _lead;
  uint8_t _tail;
  uint16_t _len;
} ADC_Record_t;
#endif

/**
 * @brief ADC controller. Fill configuration, call `ADC_Init` once,
 * then start work with `ADC_Measure`, `ADC_Record` or `ADC_Read`.
 * One job runs at a time: starting another returns `BUSY`
 * until the current one completes or `ADC_Stop` is called.
 * @param[in] reg ADC peripheral registers, `NULL` selects `ADC1`
 * @param[in] irq_priority Interrupt priority for ADC and its DMA channel
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
  // internal
  volatile ADC_State_t _busy;
  volatile uint16_t _overrun;
} ADC_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize peripheral: clocking, voltage regulator, self-calibration,
 * analog pins for every configured channel, DMA and interrupts.
 * Call once before any conversion.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Init(ADC_t *adc);

/**
 * @brief Start one-shot conversion described by `measure`. Returns immediately;
 * results land in `measure.output` and ADC frees itself after last channel.
 * @param[in,out] adc Pointer to ADC structure
 * @return `OK` when started, `BUSY` when another job is in progress,
 *   `ERR` when list cannot be converted in its order
 */
status_t ADC_Measure(ADC_t *adc);

/**
 * @brief Blocking single-channel read.
 * Pin is switched to analog mode on demand,
 * so channel does not have to be listed in any configuration.
 * Yields to scheduler until ADC is free, then while conversion runs.
 * Timing follows `measure` configuration;
 * without one, longest sampling time is used with oversampling off,
 * so result stays on native 12-bit scale.
 * @param[in,out] adc Pointer to ADC structure
 * @param[in] chan Channel number (`ADC_IN_...`)
 * @return Raw conversion result
 */
uint16_t ADC_Read(ADC_t *adc, uint8_t chan);

/**
 * @brief Kernel clock frequency of configured route after prescaler.
 * @param[in] adc Pointer to ADC structure
 * @return Frequency [Hz], `0` when framework cannot know it (PLL route)
 */
uint32_t ADC_Frequency_Hz(ADC_t *adc);

/**
 * @brief Supply voltage from internal reference (`ADC_IN_VREFEN`) and its factory calibration,
 * so result does not rely on any assumed VDDA.
 * @param[in,out] adc Pointer to ADC structure
 * @return VDDA in millivolts, `0` on failed conversion
 */
uint16_t ADC_Vdda_mV(ADC_t *adc);

/**
 * @brief Internal temperature sensor (`ADC_IN_TSEN`) read through two-point factory calibration,
 * compensated for actual supply voltage.
 * @param[in,out] adc Pointer to ADC structure
 * @return Junction temperature in °C
 */
float ADC_Temperature_C(ADC_t *adc);

#if(ADC_RECORD)
/**
 * @brief Start DMA recording described by `record`.
 * @param[in,out] adc Pointer to ADC structure
 * @return `OK` when started, `BUSY` when another job is in progress,
 *   `ERR` when list cannot be converted in its order or `buff` holds no whole scan
 */
status_t ADC_Record(ADC_t *adc);

/**
 * @brief Copy most recent samples from circular DMA buffer.
 * With `sort`, copy holds results only, deinterleaved into channel blocks aligned to scans,
 * so a copy of `chan_count` samples holds latest result of channel `k` in `buffer[k]`.
 * While recording, sorted copy can span all scans but the one in progress,
 * and DMA overwrites oldest samples first: copy has to keep ahead of it.
 * @param[in] adc Pointer to ADC structure
 * @param[out] buffer Output buffer
 * @param[in] count Number of samples to copy, with `sort` a multiple of `chan_count`
 * @param[in] sort `true` = deinterleave into channel blocks
 * @return `OK` on success, `ERR` on invalid arguments
 */
status_t ADC_LastSamples(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort);

/**
 * @brief Position of DMA writer inside record buffer:
 * how many samples of current pass are already written.
 * Lets reader pick data the DMA is not touching.
 * @param[in] adc Pointer to ADC structure
 * @return Write position in samples, within whole scans `buff` holds
 */
uint16_t ADC_RecordPosition(ADC_t *adc);

// Samples one scan takes in DMA buffer, sacrificial conversions included
uint8_t ADC_RecordStride(ADC_t *adc);

/**
 * @brief Newest scan the DMA has completed, from its first listed channel on:
 * `scan[k]` is result of `chan[k]`.
 * @param[in] adc Pointer to ADC structure
 * @return Scan in DMA buffer, `NULL` before first `ADC_Record`
 */
const uint16_t *ADC_RecordScan(ADC_t *adc);

/**
 * @brief Duration of one complete scan of `record` sequence,
 * sacrificial conversions and oversampling included.
 * @param[in] adc Pointer to ADC structure
 * @return Scan time in seconds
 */
float ADC_RecordScanTime_s(ADC_t *adc);

/**
 * @brief Time from trigger to end of sampling of `record.chan[k]`,
 * of first conversion it accumulates with oversampling. Known once `ADC_Record` has run.
 * @param[in] adc Pointer to ADC structure
 * @param[in] k Index into `record.chan`
 * @return Delay [ns], `0` when kernel frequency is unknown (PLL route)
 */
uint32_t ADC_SampleDelay_ns(ADC_t *adc, uint8_t k);
#endif

/**
 * @brief Number of runs aborted by data overrun or Stop mode since last call;
 * reading clears counter.
 * Restart policy stays with application:
 * it alone knows whether a gap in stream is acceptable.
 * @param[in,out] adc Pointer to ADC structure
 * @return Overrun count
 */
uint16_t ADC_Overruns(ADC_t *adc);

/**
 * @brief Abort job in progress and free ADC.
 * @param[in,out] adc Pointer to ADC structure
 */
void ADC_Stop(ADC_t *adc);

// Job in progress, or none: ADC takes the next one
bool ADC_IsBusy(ADC_t *adc);
bool ADC_IsFree(ADC_t *adc);

// Yield to scheduler until job in progress completes
void ADC_Wait(ADC_t *adc);

// Enable ADC and wait until it is ready, or stop any conversion and disable it
void ADC_Enable(ADC_t *adc);
void ADC_Disable(ADC_t *adc);

//---------------------------------------------------------------------------------------- Internal

// Family glue: analog mode, or internal source, for every channel on list
void ADC_InitGPIO(ADC_t *adc, uint8_t *chan, uint8_t count);

// Family glue for `PWR_Sleep`: converter and its internal sources off before Stop mode,
// job in progress counted as aborted, and sources back after it.
void ADC_Suspend(void);
void ADC_Resume(void);

// Busy wait of at least `us` at core clock, usable in interrupt
void ADC_Delay_us(uint32_t us);

// Divider, conversion cycles and oversampling ratio behind each enum value
extern const uint16_t ADC_PRESCALER_TAB[];
extern const uint16_t ADC_SAMPLING_TIME_TAB[];
extern const uint16_t ADC_OVERSAMPLING_RATIO_TAB[];

//-------------------------------------------------------------------------------------------------
#endif

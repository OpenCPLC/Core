// lib/ext/xmath.h

#ifndef XMATH_H_
#define XMATH_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"

//----------------------------------------------------------------------------------------- Integer

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

/**
 * @brief Division rounded half away from zero.
 * @param[in] num Dividend, may be negative
 * @param[in] den Divisor, sign normalized to positive
 * @return Nearest integer quotient, `0` when `den` is `0`
 */
int64_t div_round(int64_t num, int64_t den);

// Integer square root, rounded to nearest
uint32_t sqrt_u64(uint64_t value);

// Raw IEEE 754 bits of a `float` and back
uint32_t ieee754_pack(float nbr);
float ieee754_unpack(uint32_t value);

//------------------------------------------------------------------------------------------- Float

/**
 * @brief Greatest of `count` floats, `NaN` inputs skipped.
 * @param[in] count Number of arguments that follow
 * @param[in] ... Float values, promoted to `double`
 * @return Greatest valid value, `NaN` when every input is `NaN`
 */
float max_f32_NaN(uint16_t count, ...);

/**
 * @brief Smallest of `count` floats, `NaN` inputs skipped.
 * @param[in] count Number of arguments that follow
 * @param[in] ... Float values, promoted to `double`
 * @return Smallest valid value, `NaN` when every input is `NaN`
 */
float min_f32_NaN(uint16_t count, ...);

//-------------------------------------------------------------------------------------------- Sort

// Insertion sort in place, `len` elements
void sort_asc_u16(uint16_t *array, uint16_t len);
void sort_asc_i16(int16_t *array, uint16_t len);
void sort_desc_u16(uint16_t *array, uint16_t len);
void sort_desc_i16(int16_t *array, uint16_t len);
void sort_asc_u32(uint32_t *array, uint16_t len);
void sort_asc_i32(int32_t *array, uint16_t len);
void sort_desc_u32(uint32_t *array, uint16_t len);
void sort_desc_i32(int32_t *array, uint16_t len);

//----------------------------------------------------------------------------------------- Average

// Rounded `mean × mul`, all-integer: scaling happens before the division,
// so the fraction of the mean is not lost.
// `mul = 1` plain mean, `mul = 65536` Q16 result.
// `mean × mul` must fit the return type; `0` when `len` is `0`
uint32_t avg_u16(const uint16_t *array, uint16_t len, uint32_t mul);
int32_t avg_i16(const int16_t *array, uint16_t len, uint32_t mul);
uint32_t avg_u32(const uint32_t *array, uint16_t len, uint32_t mul);
int32_t avg_i32(const int32_t *array, uint16_t len, uint32_t mul);

/**
 * @brief Rounded `mean × mul` of the middle third: the lowest and the highest third
 *   are dropped, so single outliers never reach the mean. Reorders `buff` in place.
 *   Up to 2 samples give a plain mean.
 * @param[in,out] buff Samples, partitioned in place
 * @param[in] len Number of samples
 * @param[in] mul Result scale factor, see `avg_u16`
 * @return Rounded `mean × mul`
 */
uint32_t mid_mean_u16(uint16_t *buff, uint16_t len, uint32_t mul);
int32_t mid_mean_i16(int16_t *buff, uint16_t len, uint32_t mul);

/**
 * @brief Rounded `mean × mul` of the inter-quartile core: sorts in place, drops outliers
 *   beyond `1.5 × IQR`, averages the rest. Under 4 samples a plain mean.
 * @param[in,out] data Values, sorted in place
 * @param[in] count Number of values
 * @param[in] mul Result scale factor, see `avg_u16`
 * @return Rounded `mean × mul`, `0` when `count` is `0`
 */
uint32_t iqr_mean_u32(uint32_t *data, uint16_t count, uint32_t mul);

/**
 * @brief RMS of `int32_t` samples as `rms × mul`, all-integer.
 *   64-bit sum of squares; the mean square is lifted to Q16 before the root,
 *   so it must fit 48 bits.
 * @param[in] array Samples
 * @param[in] len Number of samples
 * @param[in] mul Result scale factor, see `avg_u16`
 * @return Rounded `rms × mul`, `0` when `len` is `0`
 */
uint32_t rms_i32(const int32_t *array, uint16_t len, uint32_t mul);

//------------------------------------------------------------------------------------------- Stats

/**
 * @brief Min, max and optionally sum with rounded `mean × mul`, one integer pass.
 * @param[in] data Samples
 * @param[in] count Number of samples
 * @param[out] min Smallest sample, required
 * @param[out] max Greatest sample, required
 * @param[out] sum Exact sum, `NULL` = skip
 * @param[out] avg Rounded `mean × mul`, `NULL` = skip
 * @param[in] mul Average scale factor, see `avg_u16`
 * @return `true` on success, `false` on empty data or a `NULL` required pointer
 */
bool stats_u16(const uint16_t *data, uint16_t count,
  uint16_t *min, uint16_t *max, uint32_t *sum, uint32_t *avg, uint32_t mul);
bool stats_i16(const int16_t *data, uint16_t count,
  int16_t *min, int16_t *max, int32_t *sum, int32_t *avg, uint32_t mul);
bool stats_u32(const uint32_t *data, uint16_t count,
  uint32_t *min, uint32_t *max, uint64_t *sum, uint32_t *avg, uint32_t mul);
bool stats_i32(const int32_t *data, uint16_t count,
  int32_t *min, int32_t *max, int64_t *sum, int32_t *avg, uint32_t mul);

// Sample standard deviation as `stddev × mul`, all-integer loops; the mean is carried
// in Q8, so its fraction does not bias the squared differences and the scaled result
// keeps sub-unit precision. Rounded `mean × mul` lands in `avg` (`NULL` = skip).
// `0` when `count` is below 2
uint32_t stddev_u16(const uint16_t *data, uint16_t count, uint32_t *avg, uint32_t mul);
uint32_t stddev_i16(const int16_t *data, uint16_t count, int32_t *avg, uint32_t mul);
uint32_t stddev_u32(const uint32_t *data, uint16_t count, uint32_t *avg, uint32_t mul);
uint32_t stddev_i32(const int32_t *data, uint16_t count, int32_t *avg, uint32_t mul);

//------------------------------------------------------------------------------------------ Arrays

// Keep only values inside `[min_val, max_val]`, compacting in place.
// Returns the kept count. Plausibility gate for interval/period measurements
uint16_t filter_range_u32(uint32_t *data, uint16_t count, uint32_t min_val, uint32_t max_val);

// Copy `uint16_t` samples into an `int32_t` array, zero-extended
void convert_u16_to_i32(const uint16_t *u16, int32_t *i32, uint16_t len);

// Shift every element in place: `shift > 0` left with saturation at the type maximum,
// `shift < 0` right, bits shifted out are lost
void shift_u16(uint16_t *array, uint16_t len, int16_t shift);
void shift_u32(uint32_t *array, uint16_t len, int16_t shift);

// Add `value` to every element in place, saturating at the type bounds
void add_scalar_u16(uint16_t *array, uint16_t len, int32_t value);
void add_scalar_i16(int16_t *array, uint16_t len, int32_t value);
void add_scalar_u32(uint32_t *array, uint16_t len, int64_t value);
void add_scalar_i32(int32_t *array, uint16_t len, int64_t value);
void add_scalar_f32(float *array, uint16_t len, float value);

// `true` when `value` occurs in the array
bool contains_u8(const uint8_t *array, uint16_t len, uint8_t value);
bool contains_u16(const uint16_t *array, uint16_t len, uint16_t value);
bool contains_u32(const uint32_t *array, uint16_t len, uint32_t value);

//------------------------------------------------------------------------------------------ Median

// Median of three values
int16_t median3_i16(int16_t a, int16_t b, int16_t c);
uint16_t median3_u16(uint16_t a, uint16_t b, uint16_t c);
int32_t median3_i32(int32_t a, int32_t b, int32_t c);
uint32_t median3_u32(uint32_t a, uint32_t b, uint32_t c);
float median3_f32(float a, float b, float c);

// Median of five values
int16_t median5_i16(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e);
uint16_t median5_u16(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t e);
int32_t median5_i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e);
uint32_t median5_u32(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
float median5_f32(float a, float b, float c, float d, float e);

//----------------------------------------------------------------------------------------- Filters

/**
 * @brief Step limiter: the output moves toward `input` by at most `max_delta`.
 * @param[in] input New sample
 * @param[in] prev Previous output
 * @param[in] max_delta Largest allowed step
 * @return Limited output
 */
int16_t step_limiter_i16(int16_t input, int16_t prev, uint16_t max_delta);
uint16_t step_limiter_u16(uint16_t input, uint16_t prev, uint16_t max_delta);
float step_limiter_f32(float input, float prev, float max_delta);

/**
 * @brief Exponential moving average with a power-of-two factor.
 *   A step that rounds to zero still moves by one when the difference is over 4,
 *   so the output converges instead of stalling short of the input.
 * @param[in] input New sample
 * @param[in] prev Previous output
 * @param[in] alpha_shift Smoothing as a shift, `3` = `1/8`; larger is smoother
 * @return Smoothed output
 */
int16_t ema_filter_i16(int16_t input, int16_t prev, uint8_t alpha_shift);
uint16_t ema_filter_u16(uint16_t input, uint16_t prev, uint8_t alpha_shift);
int32_t ema_filter_i32(int32_t input, int32_t prev, uint8_t alpha_shift);
uint32_t ema_filter_u32(uint32_t input, uint32_t prev, uint8_t alpha_shift);

/**
 * @brief Exponential moving average with a float factor.
 * @param[in] input New sample
 * @param[in] prev Previous output
 * @param[in] alpha Smoothing factor `0.0..1.0`, smaller is smoother
 * @return Smoothed output
 */
float ema_filter_f32(float input, float prev, float alpha);

/**
 * @brief Hampel outlier filter over a window of three: the sample is replaced by the
 *   median when it deviates by more than `k` scaled MADs. Integer variants scale the
 *   MAD by `1.5`, the float one by the exact `1.4826`.
 * @param[in] input Current sample
 * @param[in] z1 Previous sample
 * @param[in] z2 Sample before previous
 * @param[in] k Threshold multiplier, typically `2..3`, `0` disables
 * @return Cleaned sample
 */
int16_t hampel_i16(int16_t input, int16_t z1, int16_t z2, uint8_t k);
uint16_t hampel_u16(uint16_t input, uint16_t z1, uint16_t z2, uint8_t k);
int32_t hampel_i32(int32_t input, int32_t z1, int32_t z2, uint8_t k);
uint32_t hampel_u32(uint32_t input, uint32_t z1, uint32_t z2, uint8_t k);
float hampel_f32(float input, float z1, float z2, float k);

//----------------------------------------------------------------------------------- Interpolation

/**
 * @brief Piecewise-linear interpolation over `(x, y)` points sorted by `x`,
 *   ascending or descending. Outside the range the end value holds, no extrapolation.
 * @param[in] x Input coordinate
 * @param[in] xs X coordinates, strictly monotonic
 * @param[in] ys Y values, one per `xs` entry
 * @param[in] count Number of points, `1` returns `ys[0]`
 * @return Interpolated value, `NaN` for a `NaN` input or an empty table
 */
float interp_f32(float x, const float *xs, const float *ys, uint16_t count);

/**
 * @brief Fill a log-lin blended scale from `start` to `end`,
 *   reversed when `start > end`.
 * @param[in] start Start value, `> 0`
 * @param[in] end End value, `> 0`
 * @param[in] n Number of points, `>= 2`
 * @param[in] blend `0` = logarithmic, `1` = linear, in between a mix
 * @param[out] scale_array Output, `n` entries
 * @return `true` when filled, `false` on invalid input
 */
bool scale_fill(float start, float end, int n, float blend, float *scale_array);

//-------------------------------------------------------------------------------------------------
#endif

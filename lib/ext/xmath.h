/** @file lib/ext/xmath.h */

#ifndef XMATH_H_
#define XMATH_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include "xdef.h"

//-------------------------------------------------------------------------------------------------

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

int64_t div_round(int64_t num, int64_t den);

uint32_t ieee754_pack(float nbr);
float ieee754_unpack(uint32_t value);

float max_f32_NaN(uint16_t count, ...);
float min_f32_NaN(uint16_t count, ...);

//-------------------------------------------------------------------------------------- Median

/**
 * @brief Sorts `uint16_t` array in ascending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_asc_u16(uint16_t *array, uint16_t len);

/**
 * @brief Sorts `int16_t` array in ascending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_asc_i16(int16_t *array, uint16_t len);

/**
 * @brief Sorts `uint16_t` array in descending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_desc_u16(uint16_t *array, uint16_t len);

/**
 * @brief Sorts `int16_t` array in descending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_desc_i16(int16_t *array, uint16_t len);

/**
 * @brief Sorts `uint32_t` array in ascending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_asc_u32(uint32_t *array, uint16_t len);

/**
 * @brief Sorts `int32_t` array in ascending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_asc_i32(int32_t *array, uint16_t len);

/**
 * @brief Sorts `uint32_t` array in descending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_desc_u32(uint32_t *array, uint16_t len);

/**
 * @brief Sorts `int32_t` array in descending order (in-place).
 * @param array Pointer to array to sort.
 * @param len Number of elements in `array`.
 */
void sort_desc_i32(int32_t *array, uint16_t len);

//-------------------------------------------------------------------------------------- median

float avg_u16(const uint16_t *array, uint16_t len);
float avg_i16(const int16_t *array, uint16_t len);
float avg_u32(const uint32_t *array, uint16_t len);
float avg_i32(const int32_t *array, uint16_t len);

bool stats_u16(const uint16_t *data, uint16_t count, uint16_t *min, uint16_t *max, uint32_t *sum, float *avg);
bool stats_i16(const int16_t *data, uint16_t count, int16_t *min, int16_t *max, int32_t *sum, float *avg);
bool stats_u32(const uint32_t *data, uint16_t count, uint32_t *min, uint32_t *max, uint64_t *sum, float *avg);
bool stats_i32(const int32_t *data, uint16_t count, int32_t *min, int32_t *max, int64_t *sum, float *avg);


/**
 * @brief Copies a `uint16_t` array into an `int32_t` array (zero-extended).
 * @note Often measurements are stored as `uint16_t`, while processing
 *   and filtering are done on `int32_t`.
 * @param[in] u16 Pointer to source array.
 * @param[out] i32 Pointer to destination array.
 * @param[in] len Number of elements to copy.
 */
void convert_u16_to_i32(const uint16_t *u16, int32_t *i32, uint16_t len);

/**
 * @brief Calculates trimmed mean from middle third of `uint16_t` samples.
 * @note Reorders input buffer in-place. For `len <= 2`, returns simple average.
 *   For longer buffers, discards lowest third and highest third,
 *   then averages middle third only.
 * @param[in,out] buff Pointer to input sample buffer.
 * @param[in] len Number of elements in `buff`.
 * @return Mean value of middle third as `float`.
 */
float mid_mean_u16(uint16_t *buff, uint16_t len);

/**
 * @brief Calculates RMS value of an `int32_t` sample array.
 * @note Uses 64-bit accumulator for sum of squares. If len is 0, returns 0.0f.
 * @param[in] serie Pointer to input samples array.
 * @param[in] len Number of samples in the array.
 * @return RMS value of the samples as a float (in ADC units).
 */
float rms_i32(int32_t *array, uint16_t len);

//--------------------------------------------------------------------------------------- shift

/**
 * @brief Shifts each element of a uint16_t array (in-place, with saturation on left shift).
 * Positive shift: left shift (<<), with saturation to 0xFFFF.
 * Negative shift: right shift (>>), bits shifted out are discarded.
 * @param array Pointer to array to modify.
 * @param len Number of elements in the array.
 * @param shift Shift amount in bits: >0 = left, <0 = right, 0 = no change.
 */
void shift_u16(uint16_t *array, uint16_t len, int16_t shift);

/**
 * @brief Shifts each element of a uint32_t array (in-place, with saturation on left shift).
 * Positive shift: left shift (<<), with saturation to 0xFFFFFFFF.
 * Negative shift: right shift (>>), bits shifted out are discarded.
 * @param array Pointer to array to modify.
 * @param len   Number of elements in the array.
 * @param shift Shift amount in bits: >0 = left, <0 = right, 0 = no change.
 */
void shift_u32(uint32_t *array, uint16_t len, int16_t shift);

//-------------------------------------------------------------------------------------- scalar

void add_scalar_u16(uint16_t *array, uint16_t len, int32_t value);

/**
 * @brief Adds a constant scalar to each element of an `int16_t` array
 *   (in-place, with saturation).
 * @param array Pointer to array to modify.
 * @param len Number of elements in the array.
 * @param value Scalar value to add to each element (can be negative).
 */
void add_scalar_i16(int16_t *array, uint16_t len, int32_t value);

/**
 * @brief Adds a constant scalar to each element of a `uint32_t` array
 *   (in-place, with saturation).
 * @param array Pointer to array to modify.
 * @param len Number of elements in the array.
 * @param value Scalar value to add to each element (can be negative).
 */
void add_scalar_u32(uint32_t *array, uint16_t len, int64_t value);

/**
 * @brief Adds a constant scalar to each element of an `int32_t` array
 *   (in-place, with saturation).
 * @param array Pointer to array to modify.
 * @param len Number of elements in the array.
 * @param value Scalar value to add to each element (can be negative).
 */
void add_scalar_i32(int32_t *array, uint16_t len, int64_t value);

/**
 * @brief Adds a constant scalar to each element of a `float` array (in-place).
 * @param array Pointer to array to modify.
 * @param len Number of elements in the array.
 * @param value Scalar value to add to each element (can be negative).
 */
void add_scalar_f32(float *array, uint16_t len, float value);

//--------------------------------------------------------------------------------------

float stddev_u16(const uint16_t *data, uint16_t count, float *avg);
float stddev_i16(const int16_t *data, uint16_t count, float *avg);
float stddev_u32(const uint32_t *data, uint16_t count, float *avg);
float stddev_i32(const int32_t *data, uint16_t count, float *avg);

bool contains_u8(const uint8_t *array, uint16_t len, uint8_t value);
bool contains_u16(const uint16_t *array, uint16_t len, uint16_t value);
bool contains_u32(const uint32_t *array, uint16_t len, uint32_t value);

//-------------------------------------------------------------------------------------- Median

int16_t median3_i16(int16_t a, int16_t b, int16_t c); // Median of three int16_t values. 
uint16_t median3_u16(uint16_t a, uint16_t b, uint16_t c); // Median of three uint16_t values.
int32_t median3_i32(int32_t a, int32_t b, int32_t c); // Median of three int32_t values.
uint32_t median3_u32(uint32_t a, uint32_t b, uint32_t c); // Median of three uint32_t values.
float median3_f32(float a, float b, float c); // Median of three float values.
// Median of five int16_t values
int16_t median5_i16(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e);
// Median of five uint16_t values
uint16_t median5_u16(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t e);
// Median of five int32_t values
int32_t median5_i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e);
// Median of five uint32_t values
uint32_t median5_u32(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
// Median of five float values
float median5_f32(float a, float b, float c, float d, float e);

//------------------------------------------------------------------------ Filter: Step Limiter

/**
 * @brief Limits max change between samples for `int16_t` (-32768..32767).
 * @param input New sample.
 * @param prev Previous sample.
 * @param max_delta Maximum allowed step.
 * @return Limited output.
 */
int16_t step_limiter_i16(int16_t input, int16_t prev, uint16_t max_delta);

/**
 * @brief Limits max change between samples for `uint16_t` (0..65535).
 * @param input New sample.
 * @param prev Previous sample.
 * @param max_delta Maximum allowed step.
 * @return Limited output.
 */
uint16_t step_limiter_u16(uint16_t input, uint16_t prev, uint16_t max_delta);

/**
 * @brief Limits maximum step change between `float` values.
 * @param input New input sample.
 * @param prev Previous value.
 * @param max_delta Maximum allowed change.
 * @return Limited output value.
 */
float step_limiter_f32(float input, float prev, float max_delta);

//--------------------------------------------------------------------------------- Filter: EMA

/**
 * @brief Exponential moving average filter for `int16_t` (-32768..32767).
 * @param input New input sample.
 * @param prev Previous sample.
 * @param alpha_shift Filter strength (higher = smoother), e.g. 3 → alpha = 1/8.
 * @return Smoothed value.
 */
int16_t ema_filter_i16(int16_t input, int16_t prev, uint8_t alpha_shift);

/**
 * @brief Exponential moving average filter for `uint16_t` (0..65535).
 * @param input New input sample.
 * @param prev Previous sample.
 * @param alpha_shift Filter strength (higher = smoother), e.g. 3 → alpha = 1/8.
 * @return Smoothed value.
 */
uint16_t ema_filter_u16(uint16_t input, uint16_t prev, uint8_t alpha_shift);

/**
 * @brief Exponential moving average filter for `float` values.
 * @param input New input sample.
 * @param prev Previous filtered value.
 * @param alpha Filter factor (0.0f to 1.0f). Lower = smoother.
 * @return Smoothed output value.
 */
float ema_filter_f32(float input, float prev, float alpha);

//------------------------------------------------------------------------------ Filter: Hampel

/**
 * @brief Hampel outlier filter for int16_t (window = 3).
 * Uses 1.5 approximation for 1.4826 consistency constant.
 * @param[in] input Current sample.
 * @param[in] z1 Previous sample.
 * @param[in] z2 Sample before previous.
 * @param[in] k Threshold multiplier (typical 2–3, 0 disables).
 * @return Cleaned value.
 */
int16_t hampel_i16(int16_t input, int16_t z1, int16_t z2, uint8_t k);

/**
 * @brief Hampel outlier filter for uint16_t (window = 3).
 * @param[in] input Current sample.
 * @param[in] z1 Previous sample.
 * @param[in] z2 Sample before previous.
 * @param[in] k Threshold multiplier (typical 2–3, 0 disables).
 * @return Cleaned value.
 */
uint16_t hampel_u16(uint16_t input, uint16_t z1, uint16_t z2, uint8_t k);

/**
 * @brief Hampel outlier filter for int32_t (window = 3).
 * @param[in] input Current sample.
 * @param[in] z1 Previous sample.
 * @param[in] z2 Sample before previous.
 * @param[in] k Threshold multiplier (typical 2–3, 0 disables).
 * @return Cleaned value.
 */
int32_t hampel_i32(int32_t input, int32_t z1, int32_t z2, uint8_t k);

/**
 * @brief Hampel outlier filter for uint32_t (window = 3).
 * @param[in] input Current sample.
 * @param[in] z1 Previous sample.
 * @param[in] z2 Sample before previous.
 * @param[in] k Threshold multiplier (typical 2–3, 0 disables).
 * @return Cleaned value.
 */
uint32_t hampel_u32(uint32_t input, uint32_t z1, uint32_t z2, uint8_t k);

/**
 * @brief Hampel outlier filter for float (window = 3).
 * @param[in] input Current sample.
 * @param[in] z1 Previous sample.
 * @param[in] z2 Sample before previous.
 * @param[in] k Threshold multiplier (typical 2.0–3.0, ≤0 disables).
 * @return Cleaned value.
 */
float hampel_f32(float input, float z1, float z2, float k);

//------------------------------------------------------------------------------------------------- Scale

/**
 * @brief Generates a log-lin scale between `start` and `end`
 *   (if `start > end`, the scale is generated in reverse order)
 * @param[in] start Start value `> 0`
 * @param[in] end End value `> 0`
 * @param[in] n Number of points `>= 2`
 * @param[in] blend Blend factor: 0 = full logarithmic, 1 = full linear
 * @param[out] scale_array Pointer to output array
 * @return `true` if scale was generated, `false` on invalid input
 */
bool scale_fill(float start, float end, int n, float blend, float *scale_array);

//-------------------------------------------------------------------------------------------------

#endif

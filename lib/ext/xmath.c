// lib/ext/xmath.c

#include "xmath.h"

/**
 * @brief Symmetric half-up rounding division for int64.
 * @param num Numerator dividend (may be negative).
 * @param den Denominator divisor (0 -> return 0; sign normalized to positive).
 * @return Nearest integer result; .5 rounds away from zero.
 */
int64_t div_round(int64_t num, int64_t den)
{
  if(den == 0) return 0;
  if(den < 0) {
    num = -num;
    den = -den;
  }
  if(num >= 0) return (num + den / 2) / den;
  uint64_t abs_num = (uint64_t)(-(num + 1)) + 1;
  uint64_t tmp = (abs_num + (uint64_t)den / 2) / (uint64_t)den;
  return -(int64_t)tmp;
}

uint32_t sqrt_u64(uint64_t value)
{
  uint64_t rem = value, res = 0, one = (uint64_t)1 << 62;
  while(one > rem) one >>= 2;
  while(one) {
    if(rem >= res + one) {
      rem -= res + one;
      res += 2 * one;
    }
    res >>= 1;
    one >>= 2;
  }
  if(rem > res) res++; // remainder past the midpoint rounds up
  return (uint32_t)res;
}

//----------------------------------------------------------------------------------------- ieee754

/**
 * @brief Packs `float` value into raw IEEE 754 `uint32_t` representation.
 * @param nbr Input float value.
 * @return Packed 32-bit representation of `nbr`.
 */
uint32_t ieee754_pack(float nbr)
{
  uint32_t value;
  memcpy(&value, &nbr, sizeof(value));
  return value;
}

/**
 * @brief Unpacks raw IEEE 754 `uint32_t` value into `float`.
 * @param value 32-bit binary representation of a float.
 * @return Unpacked float value.
 */
float ieee754_unpack(uint32_t value)
{
  float nbr;
  memcpy(&nbr, &value, sizeof(nbr));
  return nbr;
}

//------------------------------------------------------------------------------------------- float

/**
 * @brief Returns maximum of multiple `float` values, ignoring `NaN`.
 *   If all inputs are `NaN`, returns `NaN`.
 * @param count Number of float arguments (promoted to `double`).
 * @param ... List of float values.
 * @return Maximum valid value or `NaN` if none are valid.
 */
float max_f32_NaN(uint16_t count, ...)
{
  va_list args;
  va_start(args, count);
  float max_value = NaN;
  for(uint16_t i = 0; i < count; i++) {
    float v = (float)va_arg(args, double);
    if(!isNaN(v)) {
      if(isNaN(max_value) || v > max_value) max_value = v;
    }
  }
  va_end(args);
  return max_value;
}

/**
 * @brief Returns minimum of multiple `float` values, ignoring `NaN`.
 *   If all inputs are `NaN`, returns `NaN`.
 * @param count Number of float arguments (promoted to `double`).
 * @param ... List of float values.
 * @return Minimum valid value or `NaN` if none are valid.
 */
float min_f32_NaN(uint16_t count, ...)
{
  va_list args;
  va_start(args, count);
  float min_value = NaN;
  for(uint16_t i = 0; i < count; i++) {
    float v = (float)va_arg(args, double);
    if(!isNaN(v)) {
      if(isNaN(min_value) || v < min_value) min_value = v;
    }
  }
  va_end(args);
  return min_value;
}

//-------------------------------------------------------------------------------------------- sort

void sort_asc_u16(uint16_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    uint16_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] > key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_asc_i16(int16_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    int16_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] > key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_desc_u16(uint16_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    uint16_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] < key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_desc_i16(int16_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    int16_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] < key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_asc_u32(uint32_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    uint32_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] > key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_asc_i32(int32_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    int32_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] > key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_desc_u32(uint32_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    uint32_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] < key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

void sort_desc_i32(int32_t *array, uint16_t len)
{
  for(uint16_t i = 1; i < len; i++) {
    int32_t key = array[i];
    int32_t j = i - 1;
    while(j >= 0 && array[j] < key) {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

//--------------------------------------------------------------------------------------------- avg

/**
 * @brief Average of a `uint16_t` array as `mean × mul`, rounded, all-integer.
 * Scaling happens before the division, so the fraction of the mean is not lost:
 * `mul = 1` gives a plain mean, `mul = 65536` a Q16 result for fixed-point pipelines.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param mul Result scale factor; `mean × mul` must fit `uint32_t`.
 * @return Rounded `mean × mul`. Returns `0` if `len` is `0`.
 */
uint32_t avg_u16(const uint16_t *array, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  uint32_t sum = 0;
  for(uint16_t i = 0; i < len; i++) {
    sum += array[i];
  }
  return (uint32_t)(((uint64_t)sum * mul + len / 2) / len);
}

/**
 * @brief Signed counterpart of `avg_u16`: `mean × mul`, rounded, all-integer.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param mul Result scale factor; `mean × mul` must fit `int32_t`.
 * @return Rounded `mean × mul`. Returns `0` if `len` is `0`.
 */
int32_t avg_i16(const int16_t *array, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  int32_t sum = 0;
  for(uint16_t i = 0; i < len; i++) {
    sum += array[i];
  }
  return (int32_t)div_round((int64_t)sum * mul, len);
}

/**
 * @brief Average of a `uint32_t` array as `mean × mul`, rounded, all-integer.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param mul Result scale factor; `mean × mul` must fit `uint32_t`.
 * @return Rounded `mean × mul`. Returns `0` if `len` is `0`.
 */
uint32_t avg_u32(const uint32_t *array, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  uint64_t sum = 0;
  for(uint16_t i = 0; i < len; i++) {
    sum += array[i];
  }
  return (uint32_t)((sum * mul + len / 2) / len);
}

/**
 * @brief Keeps only values inside `[min_val, max_val]`, compacting the array in place.
 * @param data Values, filtered in place.
 * @param count Number of values.
 * @param min_val Lower bound, inclusive.
 * @param max_val Upper bound, inclusive.
 * @return Number of values kept.
 */
uint16_t filter_range_u32(uint32_t *data, uint16_t count, uint32_t min_val, uint32_t max_val)
{
  if(!data || min_val > max_val) return 0;
  uint16_t valid = 0;
  for(uint16_t i = 0; i < count; i++) {
    if(in_range(data[i], min_val, max_val)) data[valid++] = data[i];
  }
  return valid;
}

/**
 * @brief Rounded `mean × mul` of the inter-quartile core: sorts in place, drops
 * outliers beyond `1.5 × IQR`, averages the rest. Under 4 samples a plain mean.
 * @param data Values, sorted in place.
 * @param count Number of values.
 * @param mul Result scale factor (see `avg_u16`).
 * @return Rounded `mean × mul`. Returns `0` if `count` is `0`.
 */
uint32_t iqr_mean_u32(uint32_t *data, uint16_t count, uint32_t mul)
{
  if(!data || !count) return 0;
  if(count < 4) return avg_u32(data, count, mul);
  sort_asc_u32(data, count);
  uint32_t q1 = data[count / 4];
  uint32_t q3 = data[(count * 3) / 4];
  uint32_t iqr15 = (q3 - q1) + ((q3 - q1) >> 1);
  uint32_t lo = q1 > iqr15 ? q1 - iqr15 : 0;
  uint32_t hi = q3 + iqr15;
  uint64_t sum = 0;
  uint16_t valid = 0;
  for(uint16_t i = 0; i < count; i++) {
    if(in_range(data[i], lo, hi)) { sum += data[i]; valid++; }
  }
  if(!valid) return 0;
  return (uint32_t)((sum * mul + valid / 2) / valid);
}

/**
 * @brief Signed counterpart of `avg_u32`: `mean × mul`, rounded, all-integer.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param mul Result scale factor; `mean × mul` must fit `int32_t`.
 * @return Rounded `mean × mul`. Returns `0` if `len` is `0`.
 */
int32_t avg_i32(const int32_t *array, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  int64_t sum = 0;
  for(uint16_t i = 0; i < len; i++) {
    sum += array[i];
  }
  return (int32_t)div_round(sum * mul, len);
}

//----------------------------------------------------------------------- stats: min, max, sum, avg

/**
 * @brief Calculates min, max, and average of `uint16_t` buffer.
 * @param[in] data Input buffer of `uint16_t` values.
 * @param[in] count Number of elements in `data`.
 * @param[out] min Pointer to store minimum value (cannot be `NULL`).
 * @param[out] max Pointer to store maximum value (cannot be `NULL`).
 * @param[out] sum Optional pointer to store sum as `uint32_t` (can be `NULL`).
 * @param[out] avg Optional pointer to store rounded `mean × mul` (can be `NULL`).
 * @param[in] mul Average scale factor (see `avg_u16`).
 * @return `true` on success, `false` if `count` is 0 or pointers are invalid
 */
bool stats_u16(const uint16_t *data, uint16_t count,
  uint16_t *min, uint16_t *max, uint32_t *sum, uint32_t *avg, uint32_t mul)
{
  if(!data || !count || !min || !max) return false;
  *min = 0xFFFF;
  *max = 0;
  uint32_t local_sum = 0;
  for(uint16_t i = 0; i < count; ++i) {
    uint16_t v = data[i];
    if(v < *min) *min = v;
    if(v > *max) *max = v;
    local_sum += v;
  }
  if(avg) *avg = (uint32_t)(((uint64_t)local_sum * mul + count / 2) / count);
  if(sum) *sum = local_sum;
  return true;
}

/**
 * @brief Calculates min, max, and average of `int16_t` buffer.
 * @param[in] data Input buffer of `int16_t` values.
 * @param[in] count Number of elements in `data`.
 * @param[out] min Pointer to store minimum value (cannot be `NULL`).
 * @param[out] max Pointer to store maximum value (cannot be `NULL`).
 * @param[out] sum Optional pointer to store sum as `int32_t` (can be `NULL`).
 * @param[out] avg Optional pointer to store rounded `mean × mul` (can be `NULL`).
 * @param[in] mul Average scale factor (see `avg_u16`).
 * @return `true` on success, `false` if `count` is 0 or pointers are invalid
 */
bool stats_i16(const int16_t *data, uint16_t count,
  int16_t *min, int16_t *max, int32_t *sum, int32_t *avg, uint32_t mul)
{
  if(!data || !count || !min || !max) return false;
  *min = INT16_MAX;
  *max = INT16_MIN;
  int32_t local_sum = 0;
  for(uint16_t i = 0; i < count; ++i) {
    int16_t v = data[i];
    if(v < *min) *min = v;
    if(v > *max) *max = v;
    local_sum += v;
  }
  if(avg) *avg = (int32_t)div_round((int64_t)local_sum * mul, count);
  if(sum) *sum = local_sum;
  return true;
}

/**
 * @brief Calculates min, max, and average of `uint32_t` buffer.
 * @param[in] data Input buffer of `uint32_t` values.
 * @param[in] count Number of elements in `data`.
 * @param[out] min Pointer to store minimum value (cannot be `NULL`).
 * @param[out] max Pointer to store maximum value (cannot be `NULL`).
 * @param[out] sum Optional pointer to store sum as `uint64_t` (can be `NULL`).
 * @param[out] avg Optional pointer to store rounded `mean × mul` (can be `NULL`).
 * @param[in] mul Average scale factor (see `avg_u16`).
 * @return `true` on success, `false` if `count` is 0 or pointers are invalid
 */
bool stats_u32(const uint32_t *data, uint16_t count,
  uint32_t *min, uint32_t *max, uint64_t *sum, uint32_t *avg, uint32_t mul)
{
  if(!data || !count || !min || !max) return false;
  *min = 0xFFFFFFFF;
  *max = 0;
  uint64_t local_sum = 0;
  for(uint16_t i = 0; i < count; ++i) {
    uint32_t v = data[i];
    if(v < *min) *min = v;
    if(v > *max) *max = v;
    local_sum += v;
  }
  if(avg) *avg = (uint32_t)((local_sum * mul + count / 2) / count);
  if(sum) *sum = local_sum;
  return true;
}

/**
 * @brief Calculates min, max, and average of `int32_t` buffer.
 * @param[in] data Input buffer of `int32_t` values.
 * @param[in] count Number of elements in `data`.
 * @param[out] min Pointer to store minimum value (cannot be `NULL`).
 * @param[out] max Pointer to store maximum value (cannot be `NULL`).
 * @param[out] sum Optional pointer to store sum as `int64_t` (can be `NULL`).
 * @param[out] avg Optional pointer to store rounded `mean × mul` (can be `NULL`).
 * @param[in] mul Average scale factor (see `avg_u16`).
 * @return `true` on success, `false` if `count` is 0 or pointers are invalid
 */
bool stats_i32(const int32_t *data, uint16_t count,
  int32_t *min, int32_t *max, int64_t *sum, int32_t *avg, uint32_t mul)
{
  if(!data || !count || !min || !max) return false;
  *min = INT32_MAX;
  *max = INT32_MIN;
  int64_t local_sum = 0;
  for(uint16_t i = 0; i < count; ++i) {
    int32_t v = data[i];
    if(v < *min) *min = v;
    if(v > *max) *max = v;
    local_sum += v;
  }
  if(avg) *avg = (int32_t)div_round(local_sum * mul, count);
  if(sum) *sum = local_sum;
  return true;
}

//--------------------------------------------------------------------------------------------- adc

void convert_u16_to_i32(const uint16_t *u16, int32_t *i32, uint16_t len)
{
  for(uint16_t i = 0; i < len; i++) {
    i32[i] = (int32_t)u16[i];
  }
}

/**
 * @brief Partitions `uint16_t` array so element `nth` lands in its sorted position.
 * @note Reorders elements in-place. Does not fully sort array.
 *   After return, all elements before `nth` are `<= array[nth]`,
 *   and all elements after `nth` are `>= array[nth]`.
 * @param[in,out] array Pointer to array to partition.
 * @param[in] len Number of elements in `array`.
 * @param[in] nth Target order index to select.
 */
static void select_u16(uint16_t *array, uint16_t len, uint16_t nth)
{
  if(len < 2 || nth >= len) return;
  uint16_t left = 0;
  uint16_t right = len - 1;
  while(left < right) {
    uint16_t mid = left + (right - left) / 2;
    uint16_t a = array[left];
    uint16_t b = array[mid];
    uint16_t c = array[right];
    uint16_t pivot;
    if(a < b) pivot = (b < c) ? b : ((a < c) ? c : a);
    else pivot = (a < c) ? a : ((b < c) ? c : b);
    uint16_t i = left;
    uint16_t j = right;
    while(1) {
      while(i <= right && array[i] < pivot) i++;
      while(j > left && array[j] > pivot) j--;
      if(i >= j) break;
      swap(array[i], array[j]);
      i++;
      if(j == 0) break;
      j--;
    }
    if(nth <= j) right = j;
    else left = j + 1;
  }
}

uint32_t mid_mean_u16(uint16_t *buff, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  if(len <= 2) return avg_u16(buff, len, mul);
  uint16_t size = len / 3;
  if(size == 0) return avg_u16(buff, len, mul);
  uint16_t start = size;
  select_u16(buff, len, start);
  select_u16(&buff[start], len - start, size - 1);
  return avg_u16(&buff[start], size, mul);
}

static void select_i16(int16_t *array, uint16_t len, uint16_t nth)
{
  if(len < 2 || nth >= len) return;
  uint16_t left = 0;
  uint16_t right = len - 1;
  while(left < right) {
    uint16_t mid = left + (right - left) / 2;
    int16_t a = array[left];
    int16_t b = array[mid];
    int16_t c = array[right];
    int16_t pivot;
    if(a < b) pivot = (b < c) ? b : ((a < c) ? c : a);
    else pivot = (a < c) ? a : ((b < c) ? c : b);
    uint16_t i = left;
    uint16_t j = right;
    while(1) {
      while(i <= right && array[i] < pivot) i++;
      while(j > left && array[j] > pivot) j--;
      if(i >= j) break;
      swap(array[i], array[j]);
      i++;
      if(j == 0) break;
      j--;
    }
    if(nth <= j) right = j;
    else left = j + 1;
  }
}

int32_t mid_mean_i16(int16_t *buff, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  if(len <= 2) return avg_i16(buff, len, mul);
  uint16_t size = len / 3;
  if(size == 0) return avg_i16(buff, len, mul);
  uint16_t start = size;
  select_i16(buff, len, start);
  select_i16(&buff[start], len - start, size - 1);
  return avg_i16(&buff[start], size, mul);
}

uint32_t rms_i32(const int32_t *array, uint16_t len, uint32_t mul)
{
  if(len == 0) return 0;
  uint64_t sum_square = 0;
  for(uint16_t i = 0; i < len; i++) {
    int64_t sample = array[i];
    sum_square += (uint64_t)(sample * sample);
  }
  // Mean square lifted to Q16 before the root,
  // so the scaled result keeps sub-unit precision.
  // Mean square must fit 48 bits
  uint32_t rms_q8 = sqrt_u64(((sum_square + len / 2) / len) << 16);
  return (uint32_t)(((uint64_t)rms_q8 * mul + 128) >> 8);
}

//------------------------------------------------------------------------------------------- shift

void shift_u16(uint16_t *array, uint16_t len, int16_t shift)
{
  if(len == 0 || shift == 0) return;
  if(shift > 0) {
    uint32_t s = (uint32_t)shift;
    if(s >= 16) {
      // any non-zero value overflows, zero stays zero
      for(uint16_t i = 0; i < len; i++) {
        if(array[i] != 0) array[i] = 0xFFFF;
      }
      return;
    }
    uint16_t sh = (uint16_t)s;
    for(uint16_t i = 0; i < len; i++) {
      uint32_t v = (uint32_t)array[i] << sh;
      if(v > 0xFFFFu) v = 0xFFFFu;
      array[i] = (uint16_t)v;
    }
  }
  else { // shift < 0 -> right shift
    uint32_t s = (uint32_t)(-shift);
    if(s >= 16) {
      for(uint16_t i = 0; i < len; i++) array[i] = 0;
      return;
    }
    uint16_t sh = (uint16_t)s;
    for(uint16_t i = 0; i < len; i++) {
      array[i] >>= sh;
    }
  }
}

void shift_u32(uint32_t *array, uint16_t len, int16_t shift)
{
  if(len == 0 || shift == 0) return;
  if(shift > 0) {
    uint32_t s = (uint32_t)shift;
    if(s >= 32) {
      // any non-zero value overflows, zero stays zero
      for(uint16_t i = 0; i < len; i++) {
        if(array[i] != 0) array[i] = 0xFFFFFFFFu;
      }
      return;
    }
    uint32_t sh = s;
    for(uint16_t i = 0; i < len; i++) {
      uint64_t v = (uint64_t)array[i] << sh;
      if(v > 0xFFFFFFFFull) v = 0xFFFFFFFFull;
      array[i] = (uint32_t)v;
    }
  }
  else { // shift < 0 -> right shift
    uint32_t s = (uint32_t)(-shift);
    if(s >= 32) {
      for(uint16_t i = 0; i < len; i++) array[i] = 0;
      return;
    }
    uint32_t sh = s;
    for(uint16_t i = 0; i < len; i++) {
      array[i] >>= sh;
    }
  }
}

//--------------------------------------------------------------------------------------------- add

void add_scalar_u16(uint16_t *array, uint16_t len, int32_t value)
{
  for(uint16_t i = 0; i < len; i++) {
    int32_t v = (int32_t)array[i] + value;
    if(v < 0) v = 0;
    if(v > 0xFFFF) v = 0xFFFF;
    array[i] = (uint16_t)v;
  }
}

void add_scalar_i16(int16_t *array, uint16_t len, int32_t value)
{
  for(uint16_t i = 0; i < len; i++) {
    int32_t v = (int32_t)array[i] + value;
    if(v < INT16_MIN) v = INT16_MIN;
    if(v > INT16_MAX) v = INT16_MAX;
    array[i] = (int16_t)v;
  }
}

void add_scalar_u32(uint32_t *array, uint16_t len, int64_t value)
{
  for(uint16_t i = 0; i < len; i++) {
    int64_t v = (int64_t)array[i] + value;
    if(v < 0) v = 0;
    if(v > (int64_t)0xFFFFFFFF) v = (int64_t)0xFFFFFFFF;
    array[i] = (uint32_t)v;
  }
}

void add_scalar_i32(int32_t *array, uint16_t len, int64_t value)
{
  for(uint16_t i = 0; i < len; i++) {
    int64_t v = (int64_t)array[i] + value;
    if(v < INT32_MIN) v = INT32_MIN;
    if(v > INT32_MAX) v = INT32_MAX;
    array[i] = (int32_t)v;
  }
}

void add_scalar_f32(float *array, uint16_t len, float value)
{
  for(uint16_t i = 0; i < len; i++) {
    array[i] += value;
  }
}

//------------------------------------------------------------------------------ standard-deviation

/**
 * @brief Sample standard deviation of `uint16_t` data as `stddev × mul`, all-integer.
 * Mean is carried in Q8, so its fraction does not bias the squared differences,
 * and the scaled result keeps sub-unit precision.
 * @param data Pointer to input array.
 * @param count Number of elements in `data`. Must be >= 2.
 * @param avg Optional pointer to store the rounded `mean × mul`.
 * @param mul Result scale factor (see `avg_u16`).
 * @return Rounded `stddev × mul`. Returns `0` if `count <= 1`.
 */
uint32_t stddev_u16(const uint16_t *data, uint16_t count, uint32_t *avg, uint32_t mul)
{
  if(count <= 1) return 0;
  uint32_t mean = avg_u16(data, count, 256);
  if(avg) *avg = (uint32_t)(((uint64_t)mean * mul + 128) >> 8);
  uint64_t sum_sq = 0;
  for(uint16_t i = 0; i < count; i++) {
    int32_t diff = (int32_t)((uint32_t)data[i] << 8) - (int32_t)mean;
    sum_sq += (uint64_t)((int64_t)diff * diff);
  }
  uint32_t dev_q8 = sqrt_u64(sum_sq / (count - 1));
  return (uint32_t)(((uint64_t)dev_q8 * mul + 128) >> 8);
}

/**
 * @brief Signed counterpart of `stddev_u16`: `stddev × mul`, all-integer, Q8 mean.
 * @param data Pointer to input array.
 * @param count Number of elements in `data`. Must be >= 2.
 * @param avg Optional pointer to store the rounded `mean × mul`.
 * @param mul Result scale factor (see `avg_u16`).
 * @return Rounded `stddev × mul`. Returns `0` if `count <= 1`.
 */
uint32_t stddev_i16(const int16_t *data, uint16_t count, int32_t *avg, uint32_t mul)
{
  if(count <= 1) return 0;
  int32_t mean = avg_i16(data, count, 256);
  if(avg) *avg = (int32_t)div_round((int64_t)mean * mul, 256);
  uint64_t sum_sq = 0;
  for(uint16_t i = 0; i < count; i++) {
    int32_t diff = (int32_t)data[i] * 256 - mean;
    sum_sq += (uint64_t)((int64_t)diff * diff);
  }
  uint32_t dev_q8 = sqrt_u64(sum_sq / (count - 1));
  return (uint32_t)(((uint64_t)dev_q8 * mul + 128) >> 8);
}

/**
 * @brief Sample standard deviation of `uint32_t` data as `stddev × mul`, all-integer.
 * The variance is lifted to Q16 before the root, so it must fit 48 bits.
 * @param data Pointer to input array.
 * @param count Number of elements in `data`. Must be >= 2.
 * @param avg Optional pointer to store the rounded `mean × mul`.
 * @param mul Result scale factor (see `avg_u16`).
 * @return Rounded `stddev × mul`. Returns `0` if `count <= 1`.
 */
uint32_t stddev_u32(const uint32_t *data, uint16_t count, uint32_t *avg, uint32_t mul)
{
  if(count <= 1) return 0;
  uint64_t sum = 0;
  for(uint16_t i = 0; i < count; i++) sum += data[i];
  uint32_t mean = (uint32_t)((sum + count / 2) / count);
  if(avg) *avg = (uint32_t)((sum * mul + count / 2) / count);
  uint64_t sum_sq = 0;
  for(uint16_t i = 0; i < count; i++) {
    int64_t diff = (int64_t)data[i] - mean;
    sum_sq += (uint64_t)(diff * diff);
  }
  uint32_t dev_q8 = sqrt_u64((sum_sq / (count - 1)) << 16);
  return (uint32_t)(((uint64_t)dev_q8 * mul + 128) >> 8);
}

/**
 * @brief Signed counterpart of `stddev_u32`: `stddev × mul`, all-integer.
 * The variance is lifted to Q16 before the root, so it must fit 48 bits.
 * @param data Pointer to input array.
 * @param count Number of elements in `data`. Must be >= 2.
 * @param avg Optional pointer to store the rounded `mean × mul`.
 * @param mul Result scale factor (see `avg_u16`).
 * @return Rounded `stddev × mul`. Returns `0` if `count <= 1`.
 */
uint32_t stddev_i32(const int32_t *data, uint16_t count, int32_t *avg, uint32_t mul)
{
  if(count <= 1) return 0;
  int64_t sum = 0;
  for(uint16_t i = 0; i < count; i++) sum += data[i];
  int32_t mean = (int32_t)div_round(sum, count);
  if(avg) *avg = (int32_t)div_round(sum * mul, count);
  uint64_t sum_sq = 0;
  for(uint16_t i = 0; i < count; i++) {
    int64_t diff = (int64_t)data[i] - mean;
    sum_sq += (uint64_t)(diff * diff);
  }
  uint32_t dev_q8 = sqrt_u64((sum_sq / (count - 1)) << 16);
  return (uint32_t)(((uint64_t)dev_q8 * mul + 128) >> 8);
}

//---------------------------------------------------------------------------------------- contains

/**
 * @brief Checks if `value` exists in `uint8_t` array.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param value Value to search for.
 * @return `true` if found, `false` otherwise.
 */
bool contains_u8(const uint8_t *array, uint16_t len, uint8_t value)
{
  while(len--) {
    if(*array++ == value) return true;
  }
  return false;
}

/**
 * @brief Checks if `value` exists in `uint16_t` array.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param value Value to search for.
 * @return `true` if found, `false` otherwise.
 */
bool contains_u16(const uint16_t *array, uint16_t len, uint16_t value)
{
  while(len--) {
    if(*array++ == value) return true;
  }
  return false;
}

/**
 * @brief Checks if `value` exists in `uint32_t` array.
 * @param array Pointer to input array.
 * @param len Number of elements in `array`.
 * @param value Value to search for.
 * @return `true` if found, `false` otherwise.
 */
bool contains_u32(const uint32_t *array, uint16_t len, uint32_t value)
{
  while(len--) {
    if(*array++ == value) return true;
  }
  return false;
}

//------------------------------------------------------------------------------------------ median

int16_t median3_i16(int16_t a, int16_t b, int16_t c)
{
  if(a > b) { int16_t t = a; a = b; b = t; }
  if(b > c) { int16_t t = b; b = c; c = t; }
  if(a > b) { int16_t t = a; a = b; b = t; }
  return b;
}

uint16_t median3_u16(uint16_t a, uint16_t b, uint16_t c)
{
  if(a > b) { uint16_t t = a; a = b; b = t; }
  if(b > c) { uint16_t t = b; b = c; c = t; }
  if(a > b) { uint16_t t = a; a = b; b = t; }
  return b;
}

int32_t median3_i32(int32_t a, int32_t b, int32_t c)
{
  if(a > b) { int32_t t = a; a = b; b = t; }
  if(b > c) { int32_t t = b; b = c; c = t; }
  if(a > b) { int32_t t = a; a = b; b = t; }
  return b;
}

uint32_t median3_u32(uint32_t a, uint32_t b, uint32_t c)
{
  if(a > b) { uint32_t t = a; a = b; b = t; }
  if(b > c) { uint32_t t = b; b = c; c = t; }
  if(a > b) { uint32_t t = a; a = b; b = t; }
  return b;
}

float median3_f32(float a, float b, float c)
{
  if(a > b) { float t = a; a = b; b = t; }
  if(b > c) { float t = b; b = c; c = t; }
  if(a > b) { float t = a; a = b; b = t; }
  return b;
}

int16_t median5_i16(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e)
{
  if(a > b) { int16_t t = a; a = b; b = t; }
  if(c > d) { int16_t t = c; c = d; d = t; }
  if(a > c) { int16_t t = a; a = c; c = t; }
  if(b > d) { int16_t t = b; b = d; d = t; }
  if(b > c) { int16_t t = b; b = c; c = t; }
  if(d > e) { int16_t t = d; d = e; e = t; }
  if(c > d) { int16_t t = c; c = d; d = t; }
  if(b > c) { int16_t t = b; b = c; c = t; }
  if(d > e) { int16_t t = d; d = e; e = t; }
  return c;
}

uint16_t median5_u16(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t e)
{
  if(a > b) { uint16_t t = a; a = b; b = t; }
  if(c > d) { uint16_t t = c; c = d; d = t; }
  if(a > c) { uint16_t t = a; a = c; c = t; }
  if(b > d) { uint16_t t = b; b = d; d = t; }
  if(b > c) { uint16_t t = b; b = c; c = t; }
  if(d > e) { uint16_t t = d; d = e; e = t; }
  if(c > d) { uint16_t t = c; c = d; d = t; }
  if(b > c) { uint16_t t = b; b = c; c = t; }
  if(d > e) { uint16_t t = d; d = e; e = t; }
  return c;
}

int32_t median5_i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e)
{
  if(a > b) { int32_t t = a; a = b; b = t; }
  if(c > d) { int32_t t = c; c = d; d = t; }
  if(a > c) { int32_t t = a; a = c; c = t; }
  if(b > d) { int32_t t = b; b = d; d = t; }
  if(b > c) { int32_t t = b; b = c; c = t; }
  if(d > e) { int32_t t = d; d = e; e = t; }
  if(c > d) { int32_t t = c; c = d; d = t; }
  if(b > c) { int32_t t = b; b = c; c = t; }
  if(d > e) { int32_t t = d; d = e; e = t; }
  return c;
}

uint32_t median5_u32(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
  if(a > b) { uint32_t t = a; a = b; b = t; }
  if(c > d) { uint32_t t = c; c = d; d = t; }
  if(a > c) { uint32_t t = a; a = c; c = t; }
  if(b > d) { uint32_t t = b; b = d; d = t; }
  if(b > c) { uint32_t t = b; b = c; c = t; }
  if(d > e) { uint32_t t = d; d = e; e = t; }
  if(c > d) { uint32_t t = c; c = d; d = t; }
  if(b > c) { uint32_t t = b; b = c; c = t; }
  if(d > e) { uint32_t t = d; d = e; e = t; }
  return c;
}

float median5_f32(float a, float b, float c, float d, float e)
{
  if(a > b) { float t = a; a = b; b = t; }
  if(c > d) { float t = c; c = d; d = t; }
  if(a > c) { float t = a; a = c; c = t; }
  if(b > d) { float t = b; b = d; d = t; }
  if(b > c) { float t = b; b = c; c = t; }
  if(d > e) { float t = d; d = e; e = t; }
  if(c > d) { float t = c; c = d; d = t; }
  if(b > c) { float t = b; b = c; c = t; }
  if(d > e) { float t = d; d = e; e = t; }
  return c;
}

//---------------------------------------------------------------------------- Filter: Step Limiter

int16_t step_limiter_i16(int16_t input, int16_t prev, uint16_t max_delta)
{
  int32_t diff = (int32_t)input - (int32_t)prev;
  if(diff > (int32_t)max_delta) return prev + max_delta;
  if(diff < -(int32_t)max_delta) return prev - max_delta;
  return input;
}

uint16_t step_limiter_u16(uint16_t input, uint16_t prev, uint16_t max_delta)
{
  int32_t diff = (int32_t)input - (int32_t)prev;
  if(diff > (int32_t)max_delta) return prev + max_delta;
  if(diff < -(int32_t)max_delta) return prev - max_delta;
  return input;
}

/**
 * @brief Limits maximum step change between `float` values.
 * @param input New input sample.
 * @param prev Previous value.
 * @param max_delta Maximum allowed change.
 * @return Limited output value.
 */
float step_limiter_f32(float input, float prev, float max_delta)
{
  float diff = input - prev;
  if(diff > max_delta) prev += max_delta;
  else if(diff < -max_delta) prev -= max_delta;
  else prev = input;
  return prev;
}

//------------------------------------------------------------------------------------- Filter: EMA

int16_t ema_filter_i16(int16_t input, int16_t prev, uint8_t alpha_shift)
{
  if(alpha_shift > 15) alpha_shift = 15;
  int32_t diff = (int32_t)input - (int32_t)prev;
  int32_t step = diff >> alpha_shift;
  if(step == 0 && diff != 0) {
    if(abs(diff) > 4) step = (diff > 0) ? 1 : -1;
    else prev = input;
  }
  return (int16_t)((int32_t)prev + step);
}

uint16_t ema_filter_u16(uint16_t input, uint16_t prev, uint8_t alpha_shift)
{
  if(alpha_shift > 15) alpha_shift = 15;
  int32_t diff = (int32_t)input - (int32_t)prev;
  int32_t step = diff >> alpha_shift;
  if(step == 0 && diff != 0) {
    if(abs(diff) > 4) step = (diff > 0) ? 1 : -1;
    else return input;
  }
  int32_t result = (int32_t)prev + step;
  if(result < 0) result = 0;
  if(result > 0xFFFF) result = 0xFFFF;
  return (uint16_t)result;
}

int32_t ema_filter_i32(int32_t input, int32_t prev, uint8_t alpha_shift)
{
  if(alpha_shift > 31) alpha_shift = 31;
  int64_t diff = (int64_t)input - (int64_t)prev;
  int64_t step = diff >> alpha_shift;
  if(step == 0 && diff != 0) {
    int64_t ad = (diff < 0) ? -diff : diff;
    if(ad > 4) step = (diff > 0) ? 1 : -1;
    else prev = input;
  }
  return (int32_t)((int64_t)prev + step);
}

uint32_t ema_filter_u32(uint32_t input, uint32_t prev, uint8_t alpha_shift)
{
  if(alpha_shift > 31) alpha_shift = 31;
  int64_t diff = (int64_t)input - (int64_t)prev;
  int64_t step = diff >> alpha_shift;
  if(step == 0 && diff != 0) {
    int64_t ad = (diff < 0) ? -diff : diff;
    if(ad > 4) step = (diff > 0) ? 1 : -1;
    else return input;
  }
  return (uint32_t)((int64_t)(uint64_t)prev + step);
}

float ema_filter_f32(float input, float prev, float alpha)
{
  return prev + alpha * (input - prev);
}

//---------------------------------------------------------------------------------- Filter: Hampel

int16_t hampel_i16(int16_t input, int16_t z1, int16_t z2, uint8_t k)
{
  if(!k) return input;
  int16_t med = median3_i16(input, z1, z2);
  uint16_t d0 = (uint16_t)abs((int32_t)input - med);
  uint16_t d1 = (uint16_t)abs((int32_t)z1 - med);
  uint16_t d2 = (uint16_t)abs((int32_t)z2 - med);
  uint16_t mad = median3_u16(d0, d1, d2);
  uint32_t thresh = (uint32_t)k * mad * 3 / 2;
  return (d0 > thresh) ? med : input;
}

uint16_t hampel_u16(uint16_t input, uint16_t z1, uint16_t z2, uint8_t k)
{
  if(!k) return input;
  uint16_t med = median3_u16(input, z1, z2);
  uint16_t d0 = (input > med) ? input - med : med - input;
  uint16_t d1 = (z1 > med) ? z1 - med : med - z1;
  uint16_t d2 = (z2 > med) ? z2 - med : med - z2;
  uint16_t mad = median3_u16(d0, d1, d2);
  uint32_t thresh = (uint32_t)k * mad * 3 / 2;
  return (d0 > thresh) ? med : input;
}

int32_t hampel_i32(int32_t input, int32_t z1, int32_t z2, uint8_t k)
{
  if(!k) return input;
  int32_t med = median3_i32(input, z1, z2);
  uint32_t d0 = (uint32_t)llabs((int64_t)input - med);
  uint32_t d1 = (uint32_t)llabs((int64_t)z1 - med);
  uint32_t d2 = (uint32_t)llabs((int64_t)z2 - med);
  uint32_t mad = median3_u32(d0, d1, d2);
  uint64_t thresh = (uint64_t)k * mad * 3 / 2;
  return (d0 > thresh) ? med : input;
}

uint32_t hampel_u32(uint32_t input, uint32_t z1, uint32_t z2, uint8_t k)
{
  if(!k) return input;
  uint32_t med = median3_u32(input, z1, z2);
  uint32_t d0 = (input > med) ? input - med : med - input;
  uint32_t d1 = (z1 > med) ? z1 - med : med - z1;
  uint32_t d2 = (z2 > med) ? z2 - med : med - z2;
  uint32_t mad = median3_u32(d0, d1, d2);
  uint64_t thresh = (uint64_t)k * mad * 3 / 2;
  return (d0 > thresh) ? med : input;
}

float hampel_f32(float input, float z1, float z2, float k)
{
  if(k <= 0.0f) return input;
  float med = median3_f32(input, z1, z2);
  float d0 = fabsf(input - med);
  float d1 = fabsf(z1 - med);
  float d2 = fabsf(z2 - med);
  float mad = median3_f32(d0, d1, d2);
  return (d0 > k * 1.4826f * mad) ? med : input;
}

//------------------------------------------------------------------------------------------- Scale

bool scale_fill(float start, float end, int n, float blend, float *scale_array)
{
  if(start <= 0 || end <= 0 || n < 2 || blend < 0 || blend > 1 || !scale_array) {
    return false;
  }
  bool reverse = false;
  if(start > end) {
    swap(start, end);
    reverse = true;
  }
  float log_start = log10f(start);
  float log_end = log10f(end);
  float inv_steps = 1.0f / (float)(n - 1);
  for(int i = 0; i < n; i++) {
    float t = i * inv_steps;
    float log_val = powf(10.0f, log_start + t * (log_end - log_start));
    float lin_val = start + t * (end - start);
    scale_array[i] = (1.0f - blend) * log_val + blend * lin_val;
  }
  if(reverse) {
    for(int i = 0; i < n / 2; i++) {
      float tmp = scale_array[i];
      scale_array[i] = scale_array[n - 1 - i];
      scale_array[n - 1 - i] = tmp;
    }
  }
  return true;
}

//----------------------------------------------------------------------------------- Interpolation

float interp_f32(float x, const float *xs, const float *ys, uint16_t count)
{
  if(isNaN(x) || count == 0) return NaN;
  if(count == 1) return ys[0];
  bool asc = xs[0] < xs[count - 1];
  if(asc ? (x <= xs[0]) : (x >= xs[0])) return ys[0];
  for(uint16_t i = 1; i < count; i++) {
    if(asc ? (x <= xs[i]) : (x >= xs[i])) {
      float t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
      return ys[i - 1] + t * (ys[i] - ys[i - 1]);
    }
  }
  return ys[count - 1];
}

//-------------------------------------------------------------------------------------------------

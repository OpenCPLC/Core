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

void sort_asc_u16(uint16_t *array, uint16_t len);
void sort_asc_i16(int16_t *array, uint16_t len);
void sort_desc_u16(uint16_t *array, uint16_t len);
void sort_desc_i16(int16_t *array, uint16_t len);
void sort_asc_u32(uint32_t *array, uint16_t len);
void sort_asc_i32(int32_t *array, uint16_t len);
void sort_desc_u32(uint32_t *array, uint16_t len);
void sort_desc_i32(int32_t *array, uint16_t len);

float avg_u16(const uint16_t *array, uint16_t len);
float avg_i16(const int16_t *array, uint16_t len);
float avg_u32(const uint32_t *array, uint16_t len);
float avg_i32(const int32_t *array, uint16_t len);

bool stats_u16(const uint16_t *data, uint16_t count, uint16_t *min, uint16_t *max, uint32_t *sum, float *avg);
bool stats_i16(const int16_t *data, uint16_t count, int16_t *min, int16_t *max, int32_t *sum, float *avg);
bool stats_u32(const uint32_t *data, uint16_t count, uint32_t *min, uint32_t *max, uint64_t *sum, float *avg);
bool stats_i32(const int32_t *data, uint16_t count, int32_t *min, int32_t *max, int64_t *sum, float *avg);

void convert_u16_to_i32(const uint16_t *u16, int32_t *i32, uint16_t len);
float rms_i32(int32_t *array, uint16_t len);
void shift_u16(uint16_t *array, uint16_t len, int32_t shift);
void shift_u32(uint32_t *array, uint16_t len, int32_t shift);
void add_scalar_u16(uint16_t *array, uint16_t len, int32_t value);
void add_scalar_i16(int16_t *array, uint16_t len, int32_t value);
void add_scalar_u32(uint32_t *array, uint16_t len, int64_t value);
void add_scalar_i32(int32_t *array, uint16_t len, int64_t value);
void add_scalar_f32(float *array, uint16_t len, float value);

float stddev_u16(const uint16_t *data, uint16_t count, float *avg);
float stddev_i16(const int16_t *data, uint16_t count, float *avg);
float stddev_u32(const uint32_t *data, uint16_t count, float *avg);
float stddev_i32(const int32_t *data, uint16_t count, float *avg);

bool contains_u8(const uint8_t *array, uint16_t len, uint8_t value);
bool contains_u16(const uint16_t *array, uint16_t len, uint16_t value);
bool contains_u32(const uint32_t *array, uint16_t len, uint32_t value);

int16_t ema_filter_i16(int16_t input, int16_t prev, uint8_t alpha_shift);
uint16_t ema_filter_u16(uint16_t input, uint16_t prev, uint8_t alpha_shift);
float ema_filter_f32(float input, float prev, float alpha);
int16_t step_limiter_i16(int16_t input, int16_t prev, uint16_t max_delta);
uint16_t step_limiter_u16(uint16_t input, uint16_t prev, uint16_t max_delta);
float step_limiter_f32(float input, float prev, float max_delta);

//-------------------------------------------------------------------------------------------------
#endif

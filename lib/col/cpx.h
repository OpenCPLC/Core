/** @file lib/col/cpx.h */

#ifndef CPX_H_
#define CPX_H_

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

//-------------------------------------------------------------------------------------------------

typedef struct {
  float magnitude;
  float phase;
} cpx_polar_t;

typedef struct {
  float real;
  float imag;
} cpx_cartesian_t;

typedef struct {
  int32_t real;
  int32_t imag;
} cpx_cartesian_i32_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Convert Cartesian `int32_t` to polar.
 * @param[in] cart Input complex number.
 * @return Polar form.
 */
cpx_polar_t cpx_to_polar(cpx_cartesian_i32_t *cart);

/**
 * @brief Convert polar to Cartesian `float`.
 * @param[in] polar Input complex number.
 * @return Cartesian form.
 */
cpx_cartesian_t cpx_to_cartesian(cpx_polar_t *polar);

/**
 * @brief Convert Cartesian `float` to polar.
 * @param[in] cart Input complex number.
 * @return Polar form.
 */
cpx_polar_t cpx_from_cartesian(cpx_cartesian_t *cart);

/**
 * @brief Multiply `a * b` (polar).
 * @param[in] a First operand.
 * @param[in] b Second operand.
 * @param[out] out Result.
 * @return `true` if phase was wrapped.
 */
bool cpx_mul(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out);

/**
 * @brief Divide `a / b` (polar).
 * @param[in] a Numerator.
 * @param[in] b Denominator.
 * @param[out] out Result.
 * @return `true` if phase was wrapped.
 */
bool cpx_div(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out);

/**
 * @brief Invert `1 / a` (polar).
 * @param[in] a Input.
 * @param[out] out Result.
 * @return `true` if phase was wrapped.
 */
bool cpx_inv(cpx_polar_t *a, cpx_polar_t *out);

/**
 * @brief Add `a + b`. Converts internally via Cartesian.
 * @param[in] a First operand.
 * @param[in] b Second operand.
 * @param[out] out Result.
 */
void cpx_add(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out);

/**
 * @brief Subtract `a - b`. Converts internally via Cartesian.
 * @param[in] a First operand.
 * @param[in] b Second operand.
 * @param[out] out Result.
 */
void cpx_sub(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out);

/**
 * @brief Parallel impedance `(a * b) / (a + b)`.
 * @param[in] a First impedance.
 * @param[in] b Second impedance.
 * @param[out] out Result.
 */
void cpx_parallel(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out);

/**
 * @brief Copy polar complex number.
 * @param[out] dst Destination.
 * @param[in] src Source.
 */
static inline void cpx_copy(cpx_polar_t *dst, cpx_polar_t *src) {
  dst->magnitude = src->magnitude;
  dst->phase = src->phase;
}

//-------------------------------------------------------------------------------------------------
#endif
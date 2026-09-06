// lib/col/cpx.h

#ifndef CPX_H_
#define CPX_H_

#include <stdbool.h>
#include <stdint.h>

//------------------------------------------------------------------------------------------- Types

typedef struct {
  float magnitude;
  float phase; // [rad], wrapped to `(-pi, pi]`
} cpx_polar_t;

typedef struct {
  float real;
  float imag;
} cpx_cartesian_t;

typedef struct {
  int32_t real;
  int32_t imag;
} cpx_cartesian_i32_t;

//----------------------------------------------------------------------------------------- Convert

cpx_polar_t cpx_to_polar(const cpx_cartesian_i32_t *cart);
cpx_cartesian_t cpx_to_cartesian(const cpx_polar_t *polar);
cpx_polar_t cpx_from_cartesian(const cpx_cartesian_t *cart);

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Polar product, quotient or inverse: magnitudes multiply, phases add.
 * @param[in] a First operand
 * @param[in] b Second operand
 * @param[out] out Result
 * @return `true` when the phase wrapped around `pi`
 */
bool cpx_mul(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out);
bool cpx_div(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out);
bool cpx_inv(const cpx_polar_t *a, cpx_polar_t *out);

/**
 * @brief Polar sum or difference, computed through Cartesian form.
 * @param[in] a First operand
 * @param[in] b Second operand
 * @param[out] out Result
 */
void cpx_add(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out);
void cpx_sub(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out);

/**
 * @brief Parallel impedance `(a × b) / (a + b)`.
 * @param[in] a First impedance
 * @param[in] b Second impedance
 * @param[out] out Result
 */
void cpx_parallel(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out);

//-------------------------------------------------------------------------------------------------
#endif

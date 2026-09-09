// lib/col/cpx.c

#include "cpx.h"

#include <math.h>

// `M_PI` is not part of strict C
#define CPX_PI 3.14159265358979f

//----------------------------------------------------------------------------------------- Convert

// Bring the phase back to `(-pi, pi]`, `true` when it crossed
static bool wrap_phase(float *phase)
{
  if(*phase > CPX_PI) {
    *phase -= 2.0f * CPX_PI;
    return true;
  }
  if(*phase <= -CPX_PI) {
    *phase += 2.0f * CPX_PI;
    return true;
  }
  return false;
}

cpx_polar_t cpx_to_polar(const cpx_cartesian_i32_t *cart)
{
  float re = (float)cart->real;
  float im = (float)cart->imag;
  return (cpx_polar_t){ .magnitude = sqrtf(re * re + im * im), .phase = atan2f(im, re) };
}

cpx_cartesian_t cpx_to_cartesian(const cpx_polar_t *polar)
{
  return (cpx_cartesian_t){
    .real = polar->magnitude * cosf(polar->phase),
    .imag = polar->magnitude * sinf(polar->phase)
  };
}

cpx_polar_t cpx_from_cartesian(const cpx_cartesian_t *cart)
{
  return (cpx_polar_t){
    .magnitude = sqrtf(cart->real * cart->real + cart->imag * cart->imag),
    .phase = atan2f(cart->imag, cart->real)
  };
}

//--------------------------------------------------------------------------------------------- API

bool cpx_mul(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out)
{
  out->magnitude = a->magnitude * b->magnitude;
  out->phase = a->phase + b->phase;
  return wrap_phase(&out->phase);
}

bool cpx_div(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out)
{
  out->magnitude = a->magnitude / b->magnitude;
  out->phase = a->phase - b->phase;
  return wrap_phase(&out->phase);
}

bool cpx_inv(const cpx_polar_t *a, cpx_polar_t *out)
{
  out->magnitude = 1.0f / a->magnitude;
  out->phase = -a->phase;
  return wrap_phase(&out->phase);
}

void cpx_add(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out)
{
  cpx_cartesian_t ca = cpx_to_cartesian(a);
  cpx_cartesian_t cb = cpx_to_cartesian(b);
  cpx_cartesian_t sum = { .real = ca.real + cb.real, .imag = ca.imag + cb.imag };
  *out = cpx_from_cartesian(&sum);
}

void cpx_sub(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out)
{
  cpx_cartesian_t ca = cpx_to_cartesian(a);
  cpx_cartesian_t cb = cpx_to_cartesian(b);
  cpx_cartesian_t diff = { .real = ca.real - cb.real, .imag = ca.imag - cb.imag };
  *out = cpx_from_cartesian(&diff);
}

void cpx_parallel(const cpx_polar_t *a, const cpx_polar_t *b, cpx_polar_t *out)
{
  cpx_polar_t prod, sum;
  cpx_mul(a, b, &prod);
  cpx_add(a, b, &sum);
  cpx_div(&prod, &sum, out);
}

//-------------------------------------------------------------------------------------------------

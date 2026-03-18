/** @file lib/col/cpx.c */

#include "cpx.h"

//-------------------------------------------------------------------------------------------------

static bool cpx_wrap_phase(float *phase)
{
  if(*phase > (float)M_PI) {
    *phase -= 2.0f * (float)M_PI;
    return true;
  }
  if(*phase <= -(float)M_PI) {
    *phase += 2.0f * (float)M_PI;
    return true;
  }
  return false;
}

//-------------------------------------------------------------------------------------------------

cpx_polar_t cpx_to_polar(cpx_cartesian_i32_t *cart)
{
  float re = (float)cart->real;
  float im = (float)cart->imag;
  return (cpx_polar_t){
    .magnitude = sqrtf(re * re + im * im),
    .phase = atan2f(im, re)
  };
}

cpx_cartesian_t cpx_to_cartesian(cpx_polar_t *polar)
{
  return (cpx_cartesian_t){
    .real = polar->magnitude * cosf(polar->phase),
    .imag = polar->magnitude * sinf(polar->phase)
  };
}

cpx_polar_t cpx_from_cartesian(cpx_cartesian_t *cart)
{
  return (cpx_polar_t){
    .magnitude = sqrtf(cart->real * cart->real + cart->imag * cart->imag),
    .phase = atan2f(cart->imag, cart->real)
  };
}

//-------------------------------------------------------------------------------------------------

bool cpx_mul(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out)
{
  out->magnitude = a->magnitude * b->magnitude;
  out->phase = a->phase + b->phase;
  return cpx_wrap_phase(&out->phase);
}

bool cpx_div(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out)
{
  out->magnitude = a->magnitude / b->magnitude;
  out->phase = a->phase - b->phase;
  return cpx_wrap_phase(&out->phase);
}

bool cpx_inv(cpx_polar_t *a, cpx_polar_t *out)
{
  out->magnitude = 1.0f / a->magnitude;
  out->phase = -a->phase;
  return cpx_wrap_phase(&out->phase);
}

//-------------------------------------------------------------------------------------------------

void cpx_add(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out)
{
  cpx_cartesian_t ca = cpx_to_cartesian(a);
  cpx_cartesian_t cb = cpx_to_cartesian(b);
  cpx_cartesian_t sum = { .real = ca.real + cb.real, .imag = ca.imag + cb.imag };
  *out = cpx_from_cartesian(&sum);
}

void cpx_sub(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out)
{
  cpx_cartesian_t ca = cpx_to_cartesian(a);
  cpx_cartesian_t cb = cpx_to_cartesian(b);
  cpx_cartesian_t diff = { .real = ca.real - cb.real, .imag = ca.imag - cb.imag };
  *out = cpx_from_cartesian(&diff);
}

void cpx_parallel(cpx_polar_t *a, cpx_polar_t *b, cpx_polar_t *out)
{
  cpx_polar_t prod, sum;
  cpx_mul(a, b, &prod);
  cpx_add(a, b, &sum);
  cpx_div(&prod, &sum, out);
}

//-------------------------------------------------------------------------------------------------
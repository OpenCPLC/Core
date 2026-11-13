// .tests/test_ain.c

// Analog input `AIN_t` from the samples of its recording window, set as the ADC leaves them:
// the input voltage through the divider of `AIN_RESISTOR_UP` and `AIN_RESISTOR_DOWN`,
// read against the supply at the 16× scale.
// Units, the 2-10V window of a 4-20mA loop, range and threshold verdicts as `±Inf`,
// the reference channel that takes the supply out, and the filter of the window.

#include "test.h"
#include "plc/io/ain.c"

#define WINDOW 30 // samples a window

static uint16_t samples[WINDOW];
static AIN_t ain;
static float supply_V;

// Input fresh as after a reset: no reference, 3.3V of supply
static void input(bool mode_4_20mA)
{
  ain = (AIN_t){ .name = "AI1", .data = samples, .count = WINDOW, .mode_4_20mA = mode_4_20mA };
  AIN_SetVref(NULL);
  supply_V = 3.3f;
}

// Sample of `pin_V` at the ADC pin, at the 16× scale
static uint16_t sample(float pin_V)
{
  return (uint16_t)(pin_V / supply_V * AIN_FULL_SCALE + 0.5f);
}

static void fill(uint16_t *window, uint16_t value)
{
  for(uint16_t i = 0; i < WINDOW; i++) window[i] = value;
}

// Window of `input_V`, the divider takes it down to the pin;
// the next reading takes it, a window time later.
static void feed(float input_V)
{
  fill(samples, sample(input_V * AIN_RESISTOR_DOWN / (AIN_RESISTOR_UP + AIN_RESISTOR_DOWN)));
  TEST_Run(AIN_AVERAGE_TIME_ms);
}

//-------------------------------------------------------------------------------------------------

// 0-10V input: volts, milliamps over the 500Ω burden, percent of the range
static void input_scales_to_its_units(void)
{
  input(false);
  feed(0.0f);
  TEST_Near(AIN_Voltage_V(&ain), 0.0f, 0.001f, "0V");
  feed(5.0f);
  TEST_Near(AIN_Voltage_V(&ain), 5.0f, 0.001f, "5V");
  TEST_Near(AIN_Current_mA(&ain), 10.0f, 0.001f, "10mA");
  TEST_Near(AIN_Percent(&ain), 50.0f, 0.01f, "half the range");
  feed(10.0f);
  TEST_Near(AIN_Percent(&ain), 100.0f, 0.01f, "whole range");
}

// 4-20mA loop: the 2-10V window spans the percent, an open loop reads `-Inf` and logs it
static void loop_spans_its_window_and_breaks_open(void)
{
  input(true);
  feed(2.0f);
  TEST_Near(AIN_Percent(&ain), 0.0f, 0.01f, "4mA");
  feed(6.0f);
  TEST_Near(AIN_Percent(&ain), 50.0f, 0.01f, "12mA");
  feed(10.0f);
  TEST_Near(AIN_Percent(&ain), 100.0f, 0.01f, "20mA");
  feed(0.0f);
  TEST_Equal(AIN_Current_mA(&ain), -Inf, "open loop");
  TEST_Logged("AI1 under-range");
}

// Past 10.25V the input reads `+Inf`; thresholds narrow the range in any unit,
// percent following the window of a 4-20mA loop.
static void range_and_thresholds_give_infinity(void)
{
  input(false);
  feed(10.3f);
  TEST_Equal(AIN_Voltage_V(&ain), Inf, "over-range");
  TEST_Logged("AI1 over-range");
  AIN_Threshold(&ain, 2.0f, 8.0f, AIN_Thresh_V);
  feed(1.9f);
  TEST_Equal(AIN_Voltage_V(&ain), -Inf, "below the low threshold");
  feed(8.1f);
  TEST_Equal(AIN_Voltage_V(&ain), Inf, "above the high one");
  feed(5.0f);
  TEST_Near(AIN_Voltage_V(&ain), 5.0f, 0.001f, "between");
  input(true);
  AIN_Threshold(&ain, 10.0f, 90.0f, AIN_Thresh_Percent); // 2.8V and 9.2V
  feed(2.7f);
  TEST_Equal(AIN_Percent(&ain), -Inf, "below 10% of the window");
  feed(9.3f);
  TEST_Equal(AIN_Percent(&ain), Inf, "above 90% of the window");
}

// The reference channel takes the supply out:
// at 3.0V of supply the input reads true, where the nominal 3.3V reads it 10% high;
// a reference reading near zero, as from a dying supply, gives over-range, never a value in range.
static void reference_takes_the_supply_out(void)
{
  static uint16_t references[WINDOW];
  static AIN_t vref = { .name = "VREF", .data = references, .count = WINDOW };
  input(false);
  supply_V = 3.0f;
  feed(5.0f);
  TEST_Near(AIN_Voltage_V(&ain), 5.5f, 0.001f, "nominal supply");
  AIN_SetVref(&vref);
  // VREFINT reads `ADC_VREFINT_CAL` of 4095 at the 3.0V the factory calibrates at
  fill(references, sample(ADC_VREFINT_CAL * 3.0f / 4095));
  feed(5.0f);
  TEST_Near(AIN_Voltage_V(&ain), 5.0f, 0.001f, "measured supply");
  fill(references, 1);
  feed(5.0f);
  TEST_Equal(AIN_Voltage_V(&ain), Inf, "reference near zero");
}

// The window takes the middle third of its samples, so spikes stay out;
// with `ema` the first window seeds the filter, each next one moves it a 2^ema part of the way,
// and readings within a window step it once.
static void window_filters_its_samples(void)
{
  input(false);
  ain.ema = 2;
  feed(4.0f);
  samples[0] = AIN_FULL_SCALE;
  samples[1] = 0;
  TEST_Near(AIN_Voltage_V(&ain), 4.0f, 0.001f, "first window, spikes left out");
  feed(8.0f);
  TEST_Near(AIN_Voltage_V(&ain), 5.0f, 0.001f, "next window, a quarter of the way");
  TEST_Near(AIN_Voltage_V(&ain), 5.0f, 0.001f, "same window, no step");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(input_scales_to_its_units);
  TEST(loop_spans_its_window_and_breaks_open);
  TEST(range_and_thresholds_give_infinity);
  TEST(reference_takes_the_supply_out);
  TEST(window_filters_its_samples);
}

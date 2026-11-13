// .tests/test_din.c

// Digital input `DIN_t` on a pin of the host model, sampled every millisecond of virtual time,
// for what it adds over the filter `GPIF_t`: the pin read through `reverse`,
// the AC stretch that carries rectified mains over its gaps, the time parameters,
// and the fast counter handing over its measurement, the duty turned for `reverse`.

#include "test.h"
#include "plc/io/din.c"

#define PIN 6 // `PA6`

static DIN_t din;

// Line at the pin, as the outside world drives it
static void line(bool high)
{
  if(high) GPIOA->ODR |= 1u << PIN;
  else GPIOA->ODR &= ~(1u << PIN);
}

// `ms` samples, a millisecond apart
static void run(uint32_t ms)
{
  for(uint32_t i = 0; i < ms; i++) {
    TEST_Run(1);
    DIN_Loop(&din);
  }
}

// Milliseconds of sampling until the input reads `state`, ten seconds at most
static uint32_t until(bool state)
{
  uint32_t ms = 0;
  while(DIN_State(&din) != state && ms < 10000) {
    run(1);
    ms++;
  }
  return ms;
}

// `count` half-waves of rectified 50Hz mains: 10ms of nothing, then 10ms of line
static void mains(uint32_t count)
{
  for(uint32_t i = 0; i < count; i++) {
    line(false);
    run(10);
    line(true);
    run(10);
  }
}

//-------------------------------------------------------------------------------------------------

// A reversed input reads a low line as on, the filter takes it after its debounce
static void reversed_pin_drives_the_filter(void)
{
  line(true);
  din = (DIN_t){ .gpio = { .port = GPIOA, .pin = PIN, .reverse = true } };
  DIN_Init(&din);
  TEST_Check(!DIN_State(&din) && !DIN_Raw(&din), "high line, off");
  line(false);
  TEST_Check(DIN_Raw(&din), "low line, raw on at once");
  TEST_Near(until(true), GPIF_DEFAULT_TON_ms, 1, "on after its debounce");
  TEST_Check(DIN_Rise(&din), "rising edge");
}

// Rectified mains read as on: the stretch of `hold_ms` bridges each gap, so no edge falls.
// A line live at power-up starts off and rises once the filter catches up,
// and the line gone, the input falls `hold_ms + toff_ms` after the last pulse.
static void ac_stretch_bridges_the_gaps(void)
{
  line(true);
  din = (DIN_t){ .gpio = { .port = GPIOA, .pin = PIN }, .mode = DIN_Mode_AC };
  DIN_Init(&din);
  TEST_Check(!DIN_State(&din), "live line, off at power-up");
  mains(50);
  TEST_Check(DIN_State(&din) && DIN_Rise(&din) && !DIN_Fall(&din), "on, steady for 1s");
  line(false);
  TEST_Near(until(false), DIN_DEFAULT_HOLD_ms + GPIF_DEFAULT_TOFF_ms, 1,
    "off after the last pulse");
}

// `DIN_Time` sets a time the filter works with, `0` only reads it,
// and `DIN_DEFAULT_TIME` brings the default back.
static void time_sets_reads_and_restores(void)
{
  line(false);
  din = (DIN_t){ .gpio = { .port = GPIOA, .pin = PIN } };
  DIN_Init(&din);
  TEST_Equal(DIN_Time(&din, DIN_Time_Ton, 120), 120, "set");
  TEST_Equal(DIN_Time(&din, DIN_Time_Ton, 0), 120, "read");
  line(true);
  TEST_Near(until(true), 120, 1, "debounce of the time set");
  TEST_Equal(DIN_Time(&din, DIN_Time_Ton, DIN_DEFAULT_TIME), GPIF_DEFAULT_TON_ms, "default");
  TEST_Equal(DIN_Time(&din, DIN_Time_Hold, DIN_DEFAULT_TIME), DIN_DEFAULT_HOLD_ms,
    "hold, a default of its own");
}

// The fast counter hands over what `PWMI_t` measured, the duty turned for a reversed input;
// a plain input has neither.
static void fast_counter_turns_a_reversed_duty(void)
{
  PWMI_t pwmi = { .frequency = { [TIM_CH2] = 50.0f }, .duty = { [TIM_CH2] = 30.0f } };
  din = (DIN_t){
    .gpio = { .reverse = true },
    .mode = DIN_Mode_FastCounter,
    .pwmi = &pwmi,
    .channel = TIM_CH2
  };
  DIN_Init(&din);
  TEST_Equal(DIN_Frequency_Hz(&din), 50.0f, "frequency");
  TEST_Equal(DIN_Duty_Percent(&din), 70.0f, "duty of a reversed input");
  din = (DIN_t){ .gpio = { .port = GPIOA, .pin = PIN } };
  DIN_Init(&din);
  TEST_Check(isNaN(DIN_Frequency_Hz(&din)) && isNaN(DIN_Duty_Percent(&din)), "plain input");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(reversed_pin_drives_the_filter);
  TEST(ac_stretch_bridges_the_gaps);
  TEST(time_sets_reads_and_restores);
  TEST(fast_counter_turns_a_reversed_duty);
}

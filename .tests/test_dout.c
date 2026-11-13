// .tests/test_dout.c

// Digital output `DOUT_t` on the host models of GPIO and PWM, in virtual time.
// A relay holds each state for `DOUT_RELAY_STUN_ms` and counts its cycles,
// yet a release after a state that held long enough comes at once.
// Pulses keep their times, a relay refuses ones shorter than its stun,
// and a PWM output turns a duty into its compare value.

#include "test.h"
#include "plc/io/dout.c"

#define PIN 1 // `PA1`

static DOUT_t out;

// Level at the pin
static bool pin(void)
{
  return (GPIOA->ODR >> PIN) & 1u;
}

// `ms` passes of the loop, a millisecond apart
static void run(uint32_t ms)
{
  for(uint32_t i = 0; i < ms; i++) {
    TEST_Run(1);
    DOUT_Loop(&out);
  }
}

// Milliseconds of loop passes until the pin flips, ten seconds at most
static uint32_t until_flip(void)
{
  bool level = pin();
  uint32_t ms = 0;
  while(pin() == level && ms < 10000) {
    run(1);
    ms++;
  }
  return ms;
}

//-------------------------------------------------------------------------------------------------

// The stun of a switch holds the next one back, a release after a long state comes at once
static void relay_holds_its_state_and_releases_at_once(void)
{
  out = (DOUT_t){ .relay = true, .gpio = { .port = GPIOA, .pin = PIN } };
  DOUT_Init(&out);
  DOUT_Set(&out);
  DOUT_Loop(&out);
  TEST_Check(pin(), "on at once");
  DOUT_Rst(&out);
  TEST_Equal(until_flip(), DOUT_RELAY_STUN_ms, "release waits out the stun");
  run(1000);
  DOUT_Set(&out);
  run(1000);
  DOUT_Rst(&out);
  DOUT_Loop(&out);
  TEST_Check(!pin(), "release after a long state, at once");
  TEST_Equal(out.cycles, 2, "a cycle a switch-on");
}

// Pulses keep their on and off times, then the output comes back to its level,
// and the freeze holds that level past the last pulse, against a request too.
static void pulses_keep_their_times(void)
{
  out = (DOUT_t){ .gpio = { .port = GPIOA, .pin = PIN } };
  DOUT_Init(&out);
  TEST_Check(!DOUT_Pulse(&out, 128, 100, 50), "128 pulses overflow the half-period count");
  TEST_Check(DOUT_PulseFreeze(&out, 2, 100, 50, 500), "started");
  TEST_Check(!DOUT_Pulse(&out, 1, 100, 50), "one sequence at a time");
  DOUT_Loop(&out);
  TEST_Check(pin(), "first pulse at once");
  TEST_Equal(until_flip(), 100, "on");
  TEST_Equal(until_flip(), 50, "off");
  TEST_Equal(until_flip(), 100, "on again");
  TEST_Check(!DOUT_IsPulse(&out), "sequence over");
  DOUT_Set(&out);
  TEST_Equal(until_flip(), 50 + 500, "request after the freeze");
}

// A relay refuses pulses shorter than its stun and counts a cycle for each one it makes
static void relay_pulses_count_their_cycles(void)
{
  out = (DOUT_t){ .relay = true, .gpio = { .port = GPIOA, .pin = PIN } };
  DOUT_Init(&out);
  TEST_Check(!DOUT_Pulse(&out, 3, DOUT_RELAY_STUN_ms - 1, DOUT_RELAY_STUN_ms), "too short");
  TEST_Check(DOUT_Pulse(&out, 3, DOUT_RELAY_STUN_ms, DOUT_RELAY_STUN_ms), "times of the stun");
  run(6 * DOUT_RELAY_STUN_ms);
  TEST_Check(!DOUT_IsPulse(&out) && !pin(), "sequence over, off");
  TEST_Equal(out.cycles, 3, "a cycle a pulse");
}

// A PWM output turns a duty into its compare value, clamped to 0-100%,
// and `DOUT_Set` and `DOUT_Rst` take the channel to its ends.
static void pwm_output_takes_its_duty(void)
{
  PWM_t pwm = { .reg = TIM1, .auto_reload = 1000 };
  PWM_Init(&pwm);
  out = (DOUT_t){ .pwm = &pwm, .channel = TIM_CH2 };
  DOUT_Init(&out);
  TEST_Equal(DOUT_Duty(&out, 25.0f), 25.0f, "duty");
  TEST_Equal(TIM1->CCR2, 250, "compare value");
  TEST_Equal(DOUT_Duty(&out, 150.0f), 100.0f, "clamped");
  DOUT_Rst(&out);
  TEST_Equal(TIM1->CCR2, 0, "reset");
  DOUT_Set(&out);
  TEST_Equal(TIM1->CCR2, 1000, "set");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(relay_holds_its_state_and_releases_at_once);
  TEST(pulses_keep_their_times);
  TEST(relay_pulses_count_their_cycles);
  TEST(pwm_output_takes_its_duty);
}

// .tests/test_gpif.c
// Input filter `GPIF_t`, sampled every millisecond of virtual time:
// a level shows once it held for its debounce time, a shorter glitch never does,
// every edge raises its flag once, a level held past its long time raises the long flag,
// and a burst of switching flips the toggle where a single press does not.
// Timing is checked to the one tick of resolution the filter has.

#include "test.h"
#include "gpif.c"

static GPIF_t input;

// `ms` samples of one level, a millisecond apart
static void hold(bool level, uint32_t ms)
{
  for(uint32_t i = 0; i < ms; i++) {
    TEST_Run(1);
    GPIF_Loop(&input, level);
  }
}

static void level_shows_after_its_debounce(void)
{
  input = (GPIF_t){ .ton_ms = 50, .toff_ms = 100 };
  GPIF_Init(&input, false);
  hold(true, 49);
  TEST_Check(!GPIF_Input(&input), "high for 49 ms of 50");
  hold(true, 2);
  TEST_Check(GPIF_Input(&input), "high for 51 ms of 50");
  hold(false, 99);
  TEST_Check(GPIF_Input(&input), "low for 99 ms of 100");
  hold(false, 2);
  TEST_Check(!GPIF_Input(&input), "low for 101 ms of 100");
}

// Pulses shorter than the debounce never reach the output, in either direction
static void glitch_never_shows(void)
{
  input = (GPIF_t){ .ton_ms = 50, .toff_ms = 50 };
  GPIF_Init(&input, false);
  for(uint8_t i = 0; i < 10; i++) {
    hold(true, 40);
    hold(false, 40);
  }
  TEST_Check(!GPIF_Input(&input) && !GPIF_Rise(&input), "high pulses of 40 ms");
  hold(true, 60);
  TEST_Check(GPIF_Rise(&input));
  for(uint8_t i = 0; i < 10; i++) {
    hold(false, 40);
    hold(true, 40);
  }
  TEST_Check(GPIF_Input(&input) && !GPIF_Fall(&input), "low pulses of 40 ms");
}

// Every edge raises its flag once; `GPIF_Edge` takes both flags together
static void edge_raises_its_flag_once(void)
{
  input = (GPIF_t){ .ton_ms = 20, .toff_ms = 20 };
  GPIF_Init(&input, false);
  hold(true, 30);
  TEST_Check(GPIF_Rise(&input) && !GPIF_Rise(&input), "rise, once");
  TEST_Check(!GPIF_Fall(&input), "no fall");
  hold(false, 30);
  TEST_Check(GPIF_Fall(&input) && !GPIF_Fall(&input), "fall, once");
  hold(true, 30);
  hold(false, 30);
  TEST_Check(GPIF_Edge(&input), "both edges pending");
  TEST_Check(!GPIF_Rise(&input) && !GPIF_Fall(&input) && !GPIF_Edge(&input), "both taken");
}

// A level held past its long time raises the long flag once, a shorter one never
static void long_press_and_long_release(void)
{
  input = (GPIF_t){ .ton_ms = 50, .toff_ms = 50, .ton_long_ms = 1000, .toff_long_ms = 2000 };
  GPIF_Init(&input, false);
  hold(true, 900);
  hold(false, 100);
  TEST_Check(!GPIF_RiseLong(&input), "pressed for 900 ms of 1000");
  hold(true, 1200);
  TEST_Check(GPIF_RiseLong(&input) && !GPIF_RiseLong(&input), "pressed for 1200 ms, once");
  hold(false, 1900);
  TEST_Check(!GPIF_FallLong(&input), "released for 1900 ms of 2000");
  hold(false, 300);
  TEST_Check(GPIF_FallLong(&input) && !GPIF_FallLong(&input), "released for 2200 ms, once");
}

// A burst of switching, edges closer than half the toggle time, flips the state;
// a single press and a steady level leave it.
static void burst_flips_the_toggle(void)
{
  input = (GPIF_t){ .ton_ms = 20, .toff_ms = 20, .toggle_ms = 400 };
  GPIF_Init(&input, false);
  hold(true, 300);
  hold(false, 1000);
  TEST_Check(!GPIF_Toggle(&input), "single press of 300 ms");
  for(uint8_t i = 0; i < 6; i++) {
    hold(true, 60);
    hold(false, 60);
  }
  TEST_Check(GPIF_Toggle(&input), "burst of 720 ms flips it");
  hold(false, 1000);
  TEST_Check(GPIF_Toggle(&input), "steady level keeps it");
  for(uint8_t i = 0; i < 6; i++) {
    hold(true, 60);
    hold(false, 60);
  }
  TEST_Check(!GPIF_Toggle(&input), "next burst flips it back");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(level_shows_after_its_debounce);
  TEST(glitch_never_shows);
  TEST(edge_raises_its_flag_once);
  TEST(long_press_and_long_release);
  TEST(burst_flips_the_toggle);
}

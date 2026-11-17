// .tests/test_pwm.c

// Where a PWM counter stands in its period, on the host twin of the timer:
// edge-aligned it is the counter, center-aligned the climb to the crest and the way back down.

#include "test.h"
#include "pwm.c"

static void edge_aligned_is_the_counter(void)
{
  PWM_t pwm = { .reg = TIM3, .auto_reload = 999, .align = PWM_Align_Edge };
  PWM_Init(&pwm);
  pwm.reg->CR1 = 0;
  pwm.reg->CNT = 250;
  TEST_Equal(PWM_GetPosition(&pwm), 250u, "the counter itself");
}

static void center_aligned_runs_on_past_the_crest(void)
{
  PWM_t pwm = { .reg = TIM1, .auto_reload = 3200, .align = PWM_Align_Center1 };
  PWM_Init(&pwm);
  pwm.reg->CR1 = 0;
  pwm.reg->CNT = 3000;
  TEST_Equal(PWM_GetPosition(&pwm), 3000u, "counting up, the counter");
  pwm.reg->CR1 = TIM_CR1_DIR;
  TEST_Equal(PWM_GetPosition(&pwm), 3400u, "counting down, the crest and the way down from it");
  pwm.reg->CNT = 1;
  TEST_Equal(PWM_GetPosition(&pwm), 6399u, "a tick before the bottom, the last of the period");
}

// A 16-bit counter takes its value from the low half: bit 31 copies the update flag
static void a_short_counter_reads_its_low_half(void)
{
  PWM_t pwm = { .reg = TIM1, .auto_reload = 3200, .align = PWM_Align_Edge };
  PWM_Init(&pwm);
  pwm.reg->CR1 = 0;
  pwm.reg->CNT = 0x80000000u | 100u;
  TEST_Equal(PWM_GetPosition(&pwm), 100u, "the copied flag is no position");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(edge_aligned_is_the_counter);
  TEST(center_aligned_runs_on_past_the_crest);
  TEST(a_short_counter_reads_its_low_half);
}

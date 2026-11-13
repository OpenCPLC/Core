// .tests/test_vrts.c

// Waits of the thread scheduler, in virtual time:
// `timeout` returns at its condition and times out without it,
// and reads the condition once more past the deadline,
// so one met while another thread held the processor is no timeout.

#include "test.h"
#include "task.h"
#include "vrts.h"

static uint64_t met_at; // tick the condition holds from

static bool met(void *arg)
{
  unused(arg);
  return tick_now() >= met_at;
}

// Another thread keeps the processor for twice the wait
static void hold(void *arg)
{
  unused(arg);
  TEST_Run(200);
}

static void wait_ends_at_its_condition(void)
{
  met_at = tick_now() + 10;
  TEST_Check(!timeout(100, met, NULL), "condition met within the wait");
  TEST_Equal(tick_now(), met_at, "wait ends at the condition");
}

static void wait_without_its_condition_times_out(void)
{
  met_at = UINT64_MAX;
  TEST_Check(timeout(100, met, NULL), "condition never met times out");
}

static void condition_met_while_held_is_no_timeout(void)
{
  met_at = tick_now() + 3;
  TASK_Add(hold, NULL, 1);
  TEST_Check(!timeout(100, met, NULL), "condition met behind the deadline is no timeout");
}

void TEST_Cases(void)
{
  TEST(wait_ends_at_its_condition);
  TEST(wait_without_its_condition_times_out);
  TEST(condition_met_while_held_is_no_timeout);
}

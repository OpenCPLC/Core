// .tests/test_cron.c
// Cron table: a task without a handler or with an empty time field is refused,
// a full table refuses the next task, and a freed slot takes one again.

#define LOG_LEVEL LOG_LEVEL_DBG // cron reports its refusals at debug
#include "test.h"
#include "cron.c"

static void tick(void *arg) { unused(arg); }

static const CRON_t every_minute = {
  .Handler = tick,
  .minute = cron_any, .hour = cron_any, .month_day = cron_any,
  .month = cron_any, .week_day = cron_any,
  .enabled = true
};

static void task_without_handler_refused(void)
{
  CRON_t task = every_minute;
  task.Handler = NULL;
  TEST_Equal(CRON_Add(&task), 0, "no handler, no handle");
  TEST_Logged("null handler");
}

// Each of the five time fields empty on its own: a task that never fires is refused
static void empty_field_refused(void)
{
  CRON_t tasks[5] = { every_minute, every_minute, every_minute, every_minute, every_minute };
  tasks[0].minute = 0;
  tasks[1].hour = 0;
  tasks[2].month_day = 0;
  tasks[3].month = 0;
  tasks[4].week_day = 0;
  for(uint16_t i = 0; i < array_len(tasks); i++) {
    TEST_Check(CRON_Add(&tasks[i]) == 0, "field %u empty", i);
    TEST_Logged("empty field");
  }
}

static void full_table_refuses(void)
{
  for(uint16_t i = 1; i <= CRON_MAX_TASKS; i++) {
    TEST_Equal(CRON_Add(&every_minute), i, "handles in turn");
  }
  TEST_Equal(CRON_Add(&every_minute), 0, "table full");
  TEST_Logged("Add fault: full");
  TEST_Check(CRON_Remove(5), "slot 5 freed");
  TEST_Equal(CRON_Add(&every_minute), 5, "freed slot taken again");
}

void TEST_Cases(void)
{
  TEST(task_without_handler_refused);
  TEST(empty_field_refused);
  TEST(full_table_refuses);
}

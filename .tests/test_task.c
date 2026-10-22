// .tests/test_task.c
// Task queue: a task runs once at its deadline; past `TASK_LIMIT`, or under a key
// that still waits, the queue refuses with `false` and logs why.

#include "test.h"
#include "task.c"

static uint32_t runs;

static void count(void *arg)
{
  unused(arg);
  runs++;
}

static void due_task_runs_once(void)
{
  runs = 0;
  TEST_Check(TASK_Add(count, NULL, 10), "task queued");
  TEST_Run(9);
  TEST_Equal(runs, 0, "nothing runs before the deadline");
  TEST_Run(1);
  TEST_Equal(runs, 1, "task runs at its deadline");
  TEST_Run(10);
  TEST_Equal(runs, 1, "task runs once");
}

static void full_queue_refuses(void)
{
  runs = 0;
  for(uint32_t i = 0; i < TASK_LIMIT; i++) {
    TEST_Check(TASK_Add(count, NULL, 5), "task %u of %u queued", i + 1, TASK_LIMIT);
  }
  TEST_Equal(TASK_Add(count, NULL, 5), false, "task past `TASK_LIMIT` refused");
  TEST_Logged("Task queue full");
  TEST_Run(5);
  TEST_Equal(runs, TASK_LIMIT, "queued tasks run once due");
  TEST_Equal(TASK_Pending(), 0, "queue empty after the run");
}

static void waiting_key_refuses(void)
{
  TEST_Check(TASK_AddKey(count, NULL, 5, 7), "task under key 7 queued");
  TEST_Equal(TASK_AddKey(count, NULL, 5, 7), false, "second task under the waiting key refused");
  TEST_Logged("Task key already queued");
  TEST_Check(TASK_Cancel(7), "task under key 7 cancelled");
}

void TEST_Cases(void)
{
  TEST(due_task_runs_once);
  TEST(full_queue_refuses);
  TEST(waiting_key_refuses);
}

// lib/sys/task.c

#include "task.h"
#include "log.h"

//-------------------------------------------------------------------------------------------------

static bool task_equal(const void *a, const void *b)
{
  const TASK_t *ta = a, *tb = b;
  return ta->key && tb->key && (ta->key == tb->key);
}

static int task_compare(const void *a, const void *b)
{
  const TASK_t *ta = a, *tb = b;
  if(ta->_tick < tb->_tick) return -1;
  if(ta->_tick > tb->_tick) return 1;
  return 0;
}

static QUEUE_New(task_queue, TASK_t, TASK_LIMIT, task_equal, task_compare);

//-------------------------------------------------------------------------------------------------

static inline void task_full_error(void)
{
  LOG_ERR("Task queue full" LOG_LIB("TASK"));
}

void TASK_Add(void (*Handler)(void *), void *arg, uint32_t delay_ms)
{
  if(!delay_ms) { Handler(arg); return; }
  TASK_t task = { .Handler = Handler, .arg = arg, .key = 0, ._tick = tick_keep(delay_ms) };
  if(!QUEUE_Push(&task_queue, &task)) task_full_error();
}

void TASK_AddKey(void (*Handler)(void *), void *arg, uint32_t delay_ms, int32_t key)
{
  TASK_t task = { .Handler = Handler, .arg = arg, .key = key, ._tick = tick_keep(delay_ms) };
  if(!QUEUE_Push(&task_queue, &task)) task_full_error();
}

bool TASK_Cancel(int32_t key)
{
  if(!key) return false;
  TASK_t dummy = { .key = key };
  return QUEUE_Remove(&task_queue, &dummy, NULL);
}

static bool match_handler(const void *element, void *ctx)
{
  const TASK_t *t = element;
  return t->Handler == (void (*)(void *))ctx;
}

uint16_t TASK_CancelHandler(void (*Handler)(void *))
{
  return QUEUE_RemoveAll(&task_queue, match_handler, (void *)Handler);
}

bool TASK_Exists(int32_t key)
{
  if(!key) return false;
  TASK_t dummy = { .key = key };
  return QUEUE_Find(&task_queue, &dummy) >= 0;
}

bool TASK_Reschedule(int32_t key, uint32_t delay_ms)
{
  if(!key) return false;
  TASK_t dummy = { .key = key }, task;
  if(!QUEUE_Remove(&task_queue, &dummy, &task)) return false;
  task._tick = tick_keep(delay_ms);
  QUEUE_Push(&task_queue, &task);
  return true;
}

uint16_t TASK_Pending(void) { return QUEUE_Count(&task_queue); }
void TASK_ClearAll(void) { QUEUE_Clear(&task_queue); }

void TASK_Main(void)
{
  TASK_t task;
  while(1) {
    if(QUEUE_Peek(&task_queue, &task)) {
      if(tick_over(&task._tick)) {
        QUEUE_Pop(&task_queue, NULL);
        task.Handler(task.arg);
      }
    }
    let();
  }
}

//-------------------------------------------------------------------------------------------------
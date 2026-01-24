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
  if(ta->tick < tb->tick) return -1;
  if(ta->tick > tb->tick) return 1;
  return 0;
}

static TASK_t task_buffer[TASK_LIMIT];
static QUEUE_t task_queue = {
  .ary = {
    .data = task_buffer,
    .head = 0, .tail = 0, .count = 0,
    .limit = TASK_LIMIT,
    .element_size = sizeof(TASK_t),
    .overwrite = false
  },
  .unique = true,
  .invert = false,
  .Equal = task_equal,
  .Compare = task_compare
};

//-------------------------------------------------------------------------------------------------

static inline void task_full_error(void)
{
  LOG_Error("Task queue full" LOG_LIB("TASK"));
}

/**
 * @brief Schedule task after delay.
 * @param[in] Handler Function to call.
 * @param[in] arg User data.
 * @param[in] delay_ms Delay (0 = immediate).
 */
void TASK_Add(void (*Handler)(void *), void *arg, uint32_t delay_ms)
{
  if(!delay_ms) { Handler(arg); return; }
  TASK_t task = { .Handler = Handler, .arg = arg, .tick = tick_keep(delay_ms), .key = 0 };
  if(!QUEUE_Push(&task_queue, &task)) task_full_error();
}

/**
 * @brief Schedule task with unique key.
 * @param[in] Handler Function to call.
 * @param[in] arg User data.
 * @param[in] delay_ms Delay.
 * @param[in] key Unique key for cancel/reschedule.
 */
void TASK_AddKey(void (*Handler)(void *), void *arg, uint32_t delay_ms, int32_t key)
{
  TASK_t task = { .Handler = Handler, .arg = arg, .tick = tick_keep(delay_ms), .key = key };
  if(!QUEUE_Push(&task_queue, &task)) task_full_error();
}

/**
 * @brief Cancel task by key.
 * @param[in] key Task key.
 * @return true if found and removed.
 */
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

/**
 * @brief Cancel all tasks with given handler.
 * @param[in] Handler Function pointer.
 * @return Number cancelled.
 */
uint16_t TASK_CancelHandler(void (*Handler)(void *))
{
  return QUEUE_RemoveAll(&task_queue, match_handler, (void*)Handler);
}

/**
 * @brief Check if task exists.
 * @param[in] key Task key.
 * @return true if exists.
 */
bool TASK_Exists(int32_t key)
{
  if(!key) return false;
  TASK_t dummy = { .key = key };
  return QUEUE_Find(&task_queue, &dummy) >= 0;
}

/**
 * @brief Reschedule existing task.
 * @param[in] key Task key.
 * @param[in] delay_ms New delay from now.
 * @return true if rescheduled.
 */
bool TASK_Reschedule(int32_t key, uint32_t delay_ms)
{
  if(!key) return false;
  TASK_t dummy = { .key = key }, task;
  if(!QUEUE_Remove(&task_queue, &dummy, &task)) return false;
  task.tick = tick_keep(delay_ms);
  QUEUE_Push(&task_queue, &task);
  return true;
}

/**
 * @brief Get pending task count.
 * @return Count.
 */
uint16_t TASK_Pending(void)
{
  return QUEUE_Count(&task_queue);
}

/**
 * @brief Clear all tasks.
 */
void TASK_ClearAll(void)
{
  QUEUE_Clear(&task_queue);
}

/**
 * @brief Main scheduler loop (never returns).
 */
void TASK_Main(void)
{
  TASK_t task;
  while(1) {
    if(QUEUE_Peek(&task_queue, &task)) {
      if(tick_over(&task.tick)) {
        QUEUE_Pop(&task_queue, NULL);
        task.Handler(task.arg);
      }
    }
    let();
  }
}

//-------------------------------------------------------------------------------------------------
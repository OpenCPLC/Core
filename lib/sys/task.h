// lib/sys/task.h

#ifndef TASK_H_
#define TASK_H_

#include "queue.h"
#include "vrts.h"

//-------------------------------------------------------------------------------------------------

#ifndef TASK_LIMIT
  // Maximum tasks in queue.
  #define TASK_LIMIT 16
#endif

// Cast helper for handler functions.
#define TASK_ (void (*)(void *))

/**
 * @brief Task descriptor.
 * @param[in] Handler Function to call.
 * @param[in] arg User data.
 * @param[in] key Unique key (`0` = unused).
 * Internal:
 * @param _tick Execution time.
 */
typedef struct {
  void (*Handler)(void *);
  void *arg;
  int32_t key;
  uint64_t _tick;
} TASK_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Schedule task after delay.
 * @param[in] Handler Function to call.
 * @param[in] arg User data.
 * @param[in] delay_ms Delay (`0` = immediate execution).
 */
void TASK_Add(void (*Handler)(void *), void *arg, uint32_t delay_ms);

/**
 * @brief Schedule task with unique key.
 * @param[in] Handler Function to call.
 * @param[in] arg User data.
 * @param[in] delay_ms Delay.
 * @param[in] key Unique key for cancel/reschedule.
 */
void TASK_AddKey(void (*Handler)(void *), void *arg, uint32_t delay_ms, int32_t key);

/**
 * @brief Cancel task by key.
 * @param[in] key Task key.
 * @return `true` if found and removed.
 */
bool TASK_Cancel(int32_t key);

/**
 * @brief Cancel all tasks with given handler.
 * @param[in] Handler Function pointer.
 * @return Number cancelled.
 */
uint16_t TASK_CancelHandler(void (*Handler)(void *));

/**
 * @brief Check if task with key exists.
 * @param[in] key Task key.
 * @return `true` if exists.
 */
bool TASK_Exists(int32_t key);

/**
 * @brief Reschedule existing task.
 * @param[in] key Task key.
 * @param[in] delay_ms New delay from now.
 * @return `true` if rescheduled.
 */
bool TASK_Reschedule(int32_t key, uint32_t delay_ms);

/**
 * @brief Get pending task count.
 * @return Number of pending tasks.
 */
uint16_t TASK_Pending(void);

/**
 * @brief Clear all tasks.
 */
void TASK_ClearAll(void);

/**
 * @brief Main scheduler loop (never returns).
 */
void TASK_Main(void);

//-------------------------------------------------------------------------------------------------
#endif
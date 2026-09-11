// lib/sys/task.h

#ifndef TASK_H_
#define TASK_H_

#include "queue.h"
#include "vrts.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef TASK_LIMIT
  // Maximum tasks in queue
  #define TASK_LIMIT 16
#endif

// Cast a handler taking a typed pointer to the `void *` signature
#define TASK_ (void (*)(void *))

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Task descriptor.
 * @param[in] Handler Function to call
 * @param[in] arg User data
 * @param[in] key Unique key, `0` = none
 * Internal:
 * @param _tick Deadline
 */
typedef struct {
  void (*Handler)(void *);
  void *arg;
  int32_t key;
  // internal
  uint64_t _tick;
} TASK_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Schedule a task after a delay.
 * @param[in] Handler Function to call
 * @param[in] arg User data
 * @param[in] delay_ms Delay, `0` runs the handler at once
 * @return `true` when the task was queued or run, `false` when the queue is full
 */
bool TASK_Add(void (*Handler)(void *), void *arg, uint32_t delay_ms);

/**
 * @brief Schedule a task under a key, refused while that key waits in the queue.
 * @param[in] Handler Function to call
 * @param[in] arg User data
 * @param[in] delay_ms Delay
 * @param[in] key Unique key for cancel and reschedule
 * @return `true` when queued, `false` when the queue is full or the key waits
 */
bool TASK_AddKey(void (*Handler)(void *), void *arg, uint32_t delay_ms, int32_t key);

/**
 * @brief Cancel the task under `key`.
 * @param[in] key Task key
 * @return `true` if found and removed
 */
bool TASK_Cancel(int32_t key);

/**
 * @brief Cancel every task calling `Handler`.
 * @param[in] Handler Function pointer
 * @return Number cancelled
 */
uint16_t TASK_CancelHandler(void (*Handler)(void *));

/**
 * @brief Check if a task waits under `key`.
 * @param[in] key Task key
 * @return `true` if queued
 */
bool TASK_Exists(int32_t key);

/**
 * @brief Move the task under `key` to a new deadline.
 * @param[in] key Task key
 * @param[in] delay_ms New delay from now
 * @return `true` if rescheduled
 */
bool TASK_Reschedule(int32_t key, uint32_t delay_ms);

// Tasks waiting
uint16_t TASK_Pending(void);

// Drop every task
void TASK_ClearAll(void);

/**
 * @brief Run the earliest due task, if any: one pass of the queue.
 * @return `true` when a handler ran, `false` when nothing was due
 */
bool TASK_Step(void);

// Scheduler thread: `TASK_Step` in a loop, never returns
void TASK_Main(void);

//-------------------------------------------------------------------------------------------------
#endif

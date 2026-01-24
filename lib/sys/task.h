#ifndef TASK_H
#define TASK_H

#include "queue.h"
#include "vrts.h"

//-------------------------------------------------------------------------------------------------

#ifndef TASK_LIMIT
  #define TASK_LIMIT 16
#endif

#define TASK_ (void (*)(void *))

/**
 * @brief Task descriptor.
 * @param Handler Function to call.
 * @param arg User data.
 * @param tick Execution time.
 * @param key Unique key (0 = unused).
 */
typedef struct {
  void (*Handler)(void *);
  void *arg;
  uint64_t tick;
  int32_t key;
} TASK_t;

//-------------------------------------------------------------------------------------------------

void TASK_Add(void (*Handler)(void *), void *arg, uint32_t delay_ms);
void TASK_AddKey(void (*Handler)(void *), void *arg, uint32_t delay_ms, int32_t key);
bool TASK_Cancel(int32_t key);
uint16_t TASK_CancelHandler(void (*Handler)(void *));
bool TASK_Exists(int32_t key);
bool TASK_Reschedule(int32_t key, uint32_t delay_ms);
uint16_t TASK_Pending(void);
void TASK_ClearAll(void);
void TASK_Main(void);

//-------------------------------------------------------------------------------------------------
#endif
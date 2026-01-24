#ifndef CRON_H_
#define CRON_H_

#include "rtc.h"
#include "heap.h"
#include "pwr.h"

#ifndef CRON_TASK_LIMIT
  #define CRON_TASK_LIMIT 32
#endif

#define CRON_NULL 255

typedef struct {
  void (*function)(void *);
	void *function_struct;
  uint8_t month_day;
  uint8_t week_day;
  uint8_t hour;
  uint8_t minute;
} CRON_t;

bool CRON_Thread(void);
bool CRON_Task(void (*fnc)(void *), void *fnc_struct, uint8_t mo_day, uint8_t w_day, uint8_t h, uint8_t m);

#endif
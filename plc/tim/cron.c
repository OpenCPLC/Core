#include "cron.h"

CRON_t *cron_task[CRON_TASK_LIMIT];
uint8_t cron_task_count = 0;

bool CRON_Thread(void)
{
  /*
  RTC_Alarm_t alarm = {
    .day_mask = true,
    .hour_mask = true,
    .minute_mask = true,
    .second = 00
  };
  RTC_Alarm_B_Enable(&alarm);
  while(1) {
    if(RTC_Event_B()) {
      RTC_Datetime_t dt = RTC_Datetime();
      dt.second = 00;
      CRON_t *task;
      uint32_t timestamp = RTC_DatetimeToUnix(&dt);
      if(timestamp == BKPR_Read(BKPR_0)) continue;
      for(uint8_t i = 0; i < cron_task_count; i++) {
        task = cron_task[i];
        if(task->month_day != CRON_NULL && task->month_day == dt.month_day && \
          task->week_day != CRON_NULL && task->week_day == dt.week_day && \
          task->hour != CRON_NULL && task->hour == dt.hour && \
          task->minute != CRON_NULL && task->minute == dt.minute
        ) {
          if(task->function_struct) task->function(task->function_struct);
          else task->function(NULL);
          BKPR_Write(BKPR_0, timestamp);
        }
      }
    }
  }
    */
  return true;
}

bool CRON_Task(void (*fnc)(void *), void *fnc_struct, uint8_t mo_day, uint8_t w_day, uint8_t h, uint8_t m)
{
  if(cron_task_count >= CRON_TASK_LIMIT) return false;
  CRON_t *task = heap_alloc(sizeof(CRON_t));
  task->month_day = mo_day;
  task->week_day = w_day;
  task->hour = h;
  task->minute = m;
  cron_task[cron_task_count] = task;
  cron_task_count++;
  return true;
}


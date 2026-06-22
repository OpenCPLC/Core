// lib/sys/cron.c

#include "cron.h"

//-------------------------------------------------------------------------------------- Internal

// Bitmask of all valid `month_day` values (bits `1`-`31`)
#define CRON_MDAY_ALL 0xFFFFFFFEU
// Bitmask of all valid `week_day` values (bits `1`-`7`)
#define CRON_WDAY_ALL 0xFEU

static CRON_t tasks[CRON_MAX_TASKS];

// Match current datetime against task spec, POSIX `month_day`/`week_day` rule
static bool CRON_Match(const CRON_t *t, const RTC_Datetime_t *now)
{
  if(!(t->minute & (1ULL << now->minute))) return false;
  if(!(t->hour & (1U << now->hour))) return false;
  if(!(t->month & (1U << now->month))) return false;
  bool md_any = (t->month_day & CRON_MDAY_ALL) == CRON_MDAY_ALL;
  bool wd_any = (t->week_day & CRON_WDAY_ALL) == CRON_WDAY_ALL;
  bool md_hit = (t->month_day & (1U << now->month_day)) != 0;
  bool wd_hit = (t->week_day & (1U << now->week_day)) != 0;
  if(md_any && wd_any) return true;
  if(md_any) return wd_hit;
  if(wd_any) return md_hit;
  return md_hit || wd_hit;
}

//------------------------------------------------------------------------------------------- API

void CRON_Init(void)
{
  for(uint16_t i = 0; i < CRON_MAX_TASKS; i++) tasks[i]._used = false;
  RTC_AlarmCfg_t cfg = {
    .second = 0,
    .second_mask = false,
    .minute_mask = true,
    .hour_mask = true,
    .day_mask = true,
  };
  RTC_AlarmEnable(CRON_ALARM, &cfg);
  CRON_LOG("Init alarm:%c", CRON_ALARM + 'A');
}

uint16_t CRON_Add(const CRON_t *task)
{
  if(!task) return 0;
  if(!task->Handler) {
    CRON_LOG("Add fault: null handler");
    return 0;
  }
  if(!task->minute || !task->hour || !task->month_day || !task->month || !task->week_day) {
    CRON_LOG("Add fault: empty field");
    return 0;
  }
  for(uint16_t i = 0; i < CRON_MAX_TASKS; i++) {
    if(!tasks[i]._used) {
      tasks[i] = *task;
      tasks[i]._used = true;
      CRON_LOG("Add handle:%u", i + 1);
      return i + 1;
    }
  }
  CRON_LOG("Add fault: full");
  return 0;
}

bool CRON_Remove(uint16_t handle)
{
  if(!handle || handle > CRON_MAX_TASKS) return false;
  if(!tasks[handle - 1]._used) return false;
  tasks[handle - 1]._used = false;
  CRON_LOG("Remove handle:%u", handle);
  return true;
}

bool CRON_Enable(uint16_t handle)
{
  if(!handle || handle > CRON_MAX_TASKS) return false;
  if(!tasks[handle - 1]._used) return false;
  tasks[handle - 1].enabled = true;
  return true;
}

bool CRON_Disable(uint16_t handle)
{
  if(!handle || handle > CRON_MAX_TASKS) return false;
  if(!tasks[handle - 1]._used) return false;
  tasks[handle - 1].enabled = false;
  return true;
}

void CRON_Step(void)
{
  if(!RTC_Event(CRON_ALARM)) return;
  RTC_Datetime_t now = RTC_Datetime();
  for(uint16_t i = 0; i < CRON_MAX_TASKS; i++) {
    CRON_t *t = &tasks[i];
    if(!t->_used || !t->enabled) continue;
    if(CRON_Match(t, &now)) t->Handler(t->arg);
  }
}
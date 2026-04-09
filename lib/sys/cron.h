// lib/sys/cron.h

#ifndef CRON_H_
#define CRON_H_

#include <stdint.h>
#include <stdbool.h>
#include "rtc.h"
#include "log.h"
#include "main.h"

#ifndef CRON_MAX_TASKS
  // Maximum number of cron tasks
  #define CRON_MAX_TASKS 16
#endif

#ifndef CRON_ALARM
  // RTC alarm slot used by cron (`RTC_Alarm_A` or `RTC_Alarm_B`)
  #define CRON_ALARM RTC_Alarm_B
#endif

#ifndef CRON_LOG
  // Log function for cron messages
  #define CRON_LOG(fmt, ...) LOG_LIB_DBG("cron", fmt, ##__VA_ARGS__)
#endif

//------------------------------------------------------------------------------------- Wildcards

// Match all values for a field
#define CRON_ANY (~0ULL)
// Match a single value `x` (e.g. `CRON_AT(8)` = 8)
#define CRON_AT(x) (1ULL << (x))
// Match inclusive range `[a..b]` (e.g. `CRON_RANGE(1,5)` = 1..5)
#define CRON_RANGE(a, b) (((1ULL << ((b) - (a) + 1)) - 1) << (a))

//----------------------------------------------------------------------------------------- Types

/**
 * @brief Cron task definition.
 * Time fields are bitmasks: bit `n` set = match value `n`. Use `CRON_ANY`,
 * `CRON_AT(x)`, `CRON_RANGE(a,b)` or bit-or combinations.
 * `month_day` and `week_day` follow POSIX cron rule: if both restricted,
 * task fires when either matches (OR); if either is `CRON_ANY`, only the
 * other is checked (AND).
 * @param[in] Handler Called when task matches current time
 * @param[in] arg Passed to `Handler`
 * @param[in] minute Bit `n` = minute `n` (`0`-`59`)
 * @param[in] hour Bit `n` = hour `n` (`0`-`23`)
 * @param[in] month_day Bit `n` = day of month `n` (`1`-`31`)
 * @param[in] month Bit `n` = month `n` (`1`-`12`)
 * @param[in] week_day Bit `n` = weekday `n` (`1`=Mon, `7`=Sun)
 * @param[in] enabled Initial enabled state
 * Internal:
 * @param _used Slot occupancy flag
 */
typedef struct {
  void (*Handler)(void *arg);
  void *arg;
  uint64_t minute;
  uint32_t hour;
  uint32_t month_day;
  uint16_t month;
  uint8_t week_day;
  bool enabled;
  // internal
  bool _used;
} CRON_t;

//------------------------------------------------------------------------------------------- API

// Initialize cron — call after `RTC_Init`
void CRON_Init(void);

/**
 * @brief Register a new cron task.
 * @param[in] task Task definition (copied internally)
 * @return Handle `1`-`CRON_MAX_TASKS` on success, `0` on `NULL` or no free slots
 */
uint16_t CRON_Add(const CRON_t *task);

/**
 * @brief Remove a registered task.
 * @param[in] handle Handle from `CRON_Add`
 * @return `true` if task existed and was removed, `false` otherwise
 */
bool CRON_Remove(uint16_t handle);

/**
 * @brief Enable a registered task.
 * @param[in] handle Handle from `CRON_Add`
 * @return `true` if task existed, `false` otherwise
 */
bool CRON_Enable(uint16_t handle);

/**
 * @brief Disable a registered task (keeps slot reserved).
 * @param[in] handle Handle from `CRON_Add`
 * @return `true` if task existed, `false` otherwise
 */
bool CRON_Disable(uint16_t handle);

// Process pending cron tick — call from main loop
void CRON_Step(void);

//---------------------------------------------------------------------------------------------

#endif
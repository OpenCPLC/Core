// hal/stm32/sys/vrts.h

#ifndef VRTS_H_
#define VRTS_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef VRTS_THREAD_LIMIT
  // Max number of threads
  #define VRTS_THREAD_LIMIT 12
#endif

#ifndef VRTS_SWITCHING
  // Cooperative thread switching, off makes `let` a plain `__WFI`
  #define VRTS_SWITCHING 1
#endif

#ifndef VRTS_THREAD_TIMEOUT_MS
  // Longest hold of the core by one thread before `vrts_panic` [ms], `0` = off
  #define VRTS_THREAD_TIMEOUT_MS 2000
#endif

//------------------------------------------------------------------------------------------ Macros

// Cast a `bool (*)(MODULE_t *)` query to the `timeout` callback signature
#define WAIT_ (bool (*)(void *))(void (*)(void))
// Convert seconds or minutes to milliseconds
#define seconds(s) (1000 * (s))
#define minutes(m) (60000 * (m))
// Yield until `flag` is `true`
#define wait_for(flag) while(!(flag)) let()

// Declare an 8-byte aligned stack of `size` words
#define stack(name, size) \
  static uint32_t name[8 * ((size + 7) / 8)] __attribute__((aligned(8)))

// Register a thread on a stack declared with `stack`
#define thread(fnc, stack_name) \
  vrts_thread(&fnc, (uint32_t *)stack_name, array_len(stack_name))

// Alias for `let`
#define yield let

//------------------------------------------------------------------------------------------- Types

/**
 * @brief One thread of the scheduler.
 * @param stack Saved stack pointer, top of the context frame
 * @param handler Thread entry
 */
typedef struct {
  volatile uint32_t stack;
  void (*handler)(void);
} VRTS_Task_t;

//----------------------------------------------------------------------------------------- Globals

// System tick counter, one step per `systick_init` period
extern volatile uint64_t VrtsTicker;

//-------------------------------------------------------------------------------------------- Tick

// Deadline `offset_ms` from now, a tick value for the checks below
uint64_t tick_keep(uint32_t offset_ms);

// Current system tick
uint64_t tick_now(void);

/**
 * @brief One-shot expiry check, `*tick` resets to `0` when it fires.
 * @param[in,out] tick Deadline from `tick_keep`, `0` = disarmed
 * @return `true` once when the deadline passes, `false` otherwise
 */
bool tick_over(uint64_t *tick);

/**
 * @brief Pending check, `*tick` resets to `0` on expiry.
 * @param[in,out] tick Deadline from `tick_keep`, `0` = disarmed
 * @return `true` while waiting, `false` once expired or disarmed
 */
bool tick_away(uint64_t *tick);

/**
 * @brief Time since a reference tick.
 * @param[in] tick Reference tick
 * @return Elapsed time [ms], negative for a deadline still ahead
 */
int32_t tick_diff(uint64_t tick);

//------------------------------------------------------------------------------------------- Delay

// Wait `ms` yielding to other threads, or blocking with `__WFI`
void delay(uint32_t ms);
void sleep(uint32_t ms);

/**
 * @brief Yield until the condition holds or `ms` pass.
 * @param[in] ms Timeout [ms]
 * @param[in] Free Callback returning `true` when the condition is met
 * @param[in] subject Pointer passed to `Free`
 * @return `true` on timeout, `false` when the condition was met
 */
bool timeout(uint32_t ms, bool (*Free)(void *), void *subject);

// Wait for the deadline yielding or blocking, `*tick` resets to `0` after
void delay_until(uint64_t *tick);
void sleep_until(uint64_t *tick);

//----------------------------------------------------------------------------------------- Threads

/**
 * @brief Register a thread.
 * @param[in] handler Thread function
 * @param[in] stack Stack memory
 * @param[in] size Stack size in 32-bit words, at least `80` on M0+ and `128` on M4
 * @return `true` on success, `false` when the thread limit is reached
 */
bool vrts_thread(void (*handler)(void), uint32_t *stack, uint16_t size);

// Hand the core to the next thread
void let(void);

// Start the system tick, call before `vrts_init`
bool systick_init(uint32_t systick_ms);

// Enter the first thread, never returns
void vrts_init(void);

// Stop and resume thread switching, resume reports `false` before `vrts_init`
void vrts_lock(void);
bool vrts_unlock(void);

// Index of the running thread, `0` with switching off
uint8_t vrts_active_thread(void);

// Fatal scheduler error, weak: the default masks interrupts and halts
void vrts_panic(const char *msg);

//-------------------------------------------------------------------------------------------------
#endif

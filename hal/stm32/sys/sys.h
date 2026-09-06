// hal/stm32/sys/sys.h

#ifndef SYS_H_
#define SYS_H_

#include "log.h"
#include "vrts.h"
#include "heap.h"
#include "tim.h"
#include "pwr.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef SYS_BASETIME_ms
  // System tick period [ms]
  #define SYS_BASETIME_ms 1u
#endif

#ifndef SYS_CLOCK_FREQ
  // System clock [Hz], one of the values `clock_init` supports
  #define SYS_CLOCK_FREQ 16000000u
#endif

#ifndef SYS_PANIC_RESET
  // `panic` resets the system instead of halting it
  #define SYS_PANIC_RESET 0
#endif

//--------------------------------------------------------------------------------------------- API

// Clock, system tick and heap in one call
void sys_init(void);

// System clock from `SYS_CLOCK_FREQ`: HSI at 16, 48 or 64 MHz, HSE at 18.432 or 32 MHz,
// PLL at 59.904 MHz (18.432 MHz × 13 / 4); any other value fails to compile
void clock_init(void);

// Thread checking a canary word every 500 ms, `panic` on corruption
void memory_guard(void);

// Log the message on the panic level and halt, or reset with `SYS_PANIC_RESET`; never returns
void panic(const char *message);

// Handler called first by `panic`, `NULL` = none
void panic_hook(void (*handler)(void));

// Busy-wait microsecond delays on a timer, `sleep_us_init` once before the first wait
void sleep_us_init(TIM_t *tim);
void sleep_us(uint32_t us);

//-------------------------------------------------------------------------------------------------
#endif

#ifndef SYS_H_
#define SYS_H_

#include "tim.h"
#include "main.h"

#ifndef SYS_BASETIME_ms
  #define SYS_BASETIME_ms 1u
#endif

#ifndef SYS_CLOCK_FREQ
  #define SYS_CLOCK_FREQ 16000000u
#endif

#ifndef SYS_PANIC_RESET
  #define SYS_PANIC_RESET 0
#endif

void sys_init(void);
void clock_init(void);
void memory_guard(void);
void panic(const char *message);
void panic_hook(void (*handler)(void));
void sleep_us_init(TIM_t *tim);
void sleep_us(uint32_t us);

#endif
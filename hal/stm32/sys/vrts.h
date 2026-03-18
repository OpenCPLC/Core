// hal/stm32/sys/vrts.h

#ifndef VRTS_H_
#define VRTS_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "main.h"

// Max number of threads if not defined
#ifndef VRTS_THREAD_LIMIT
  #define VRTS_THREAD_LIMIT 12
#endif

// Default switching threads is enable
#ifndef VRTS_SWITCHING
  #define VRTS_SWITCHING 1
#endif

// Maximum time a thread is allowed to hold the CPU core
#ifndef VRTS_THREAD_TIMEOUT_MS
  #define VRTS_THREAD_TIMEOUT_MS 2000
#endif

#define WAIT_ (bool (*)(void *)) // Type cast for timeout function
#define seconds(s) (1000 * (s)) // Convert seconds to milliseconds
#define minutes(m) (60000 * (m)) // Convert minutes to milliseconds
#define wait_for(flag) while(!(flag)) let() // Wait until `flag` is `true`

// Struct to represent a thread in VRTS
typedef struct {
  volatile uint32_t stack;
  void (*handler)(void);
} VRTS_Task_t;

uint64_t tick_keep(uint32_t offset_ms);
uint64_t tick_now(void);
bool tick_over(uint64_t *tick);
bool tick_away(uint64_t *tick);
int32_t tick_diff(uint64_t tick);
void let(void);
#define yield let
void delay(uint32_t ms);
void sleep(uint32_t ms);
bool timeout(uint32_t ms, bool (*Free)(void *), void *subject);
void delay_until(uint64_t *tick);
void sleep_until(uint64_t *tick);

bool vrts_thread(void (*handler)(void), uint32_t *stack, uint16_t size);
#define stack(name, size) static uint32_t name[8 * ((size + 7) / 8)] __attribute__((aligned(8)))
#define thread(fnc, stack_name) vrts_thread(&fnc, (uint32_t *)stack_name, sizeof(stack_name) / sizeof(uint32_t));

void vrts_init(void);
void vrts_lock(void);
bool vrts_unlock(void);
uint8_t vrts_active_thread(void);
bool systick_init(uint32_t systick_ms);

extern volatile uint64_t VrtsTicker;

#endif

// hal/host/vrts.h

#ifndef VRTS_H_
#define VRTS_H_

#include <stdint.h>
#include <stdbool.h>
#include "xdef.h"

//------------------------------------------------------------------------------------------------- Config

#ifndef VRTS_THREAD_LIMIT
  #define VRTS_THREAD_LIMIT 12
#endif

#ifndef VRTS_SWITCHING
  #define VRTS_SWITCHING 1
#endif

//------------------------------------------------------------------------------------------------- Macros

#define WAIT_ (bool (*)(void *))
#define seconds(s) (1000 * (s))
#define minutes(m) (60000 * (m))
#define wait_for(flag) while(!(flag)) let()

#define stack(name, size) static uint32_t name[1]
#define thread(fnc, stack_name) vrts_thread(&fnc, (uint32_t *)stack_name, 0)

//------------------------------------------------------------------------------------------------- Timing

uint64_t tick_keep(uint32_t offset_ms);
uint64_t tick_now(void);
bool tick_over(uint64_t *tick);
bool tick_away(uint64_t *tick);
int32_t tick_diff(uint64_t tick);

//------------------------------------------------------------------------------------------------- Threading

void let(void);
#define yield let

void delay(uint32_t ms);
void sleep(uint32_t ms);
bool timeout(uint32_t ms, bool (*Free)(void *), void *subject);
void delay_until(uint64_t *tick);
void sleep_until(uint64_t *tick);

bool vrts_thread(void (*handler)(void), uint32_t *stack, uint16_t size);
void vrts_init(void);
void vrts_lock(void);
bool vrts_unlock(void);
uint8_t vrts_active_thread(void);

bool systick_init(uint32_t systick_ms);

//------------------------------------------------------------------------------------------------- Globals

extern volatile uint64_t VrtsTicker;

//-------------------------------------------------------------------------------------------------

#endif
// hal/stm32/sys/sys.c

#include "sys.h"

//-------------------------------------------------------------------------------------------- Init

void sys_init(void)
{
  clock_init();
  systick_init(SYS_BASETIME_ms);
  heap_init();
}

void clock_init(void)
{
  #if(SYS_CLOCK_FREQ == 16000000)
  RCC_16MHz();
  #elif(SYS_CLOCK_FREQ == 48000000)
  RCC_48MHz();
  #elif(SYS_CLOCK_FREQ == 64000000)
  RCC_64MHz();
  #elif(SYS_CLOCK_FREQ == 18432000)
  RCC_SetHSE(18432000);
  #elif(SYS_CLOCK_FREQ == 32000000)
  RCC_SetHSE(32000000);
  #elif(SYS_CLOCK_FREQ == 59904000)
  RCC_SetPLL(18432000, 2, 13, 2);
  #else
  #error "SYS_CLOCK_FREQ is not a supported system clock"
  #endif
}

//------------------------------------------------------------------------------------------- Panic

static void (*panic_handler)(void);

void panic_hook(void (*handler)(void))
{
  panic_handler = handler;
}

void panic(const char *message)
{
  if(panic_handler) panic_handler();
  LOG_Panic(message);
  __disable_irq();
  #if(SYS_PANIC_RESET)
  PWR_Reset();
  #endif
  while(1);
}

void vrts_panic(const char *msg)
{
  panic(msg);
}

// `volatile` keeps the check alive: a plain constant would fold to always-false.
// A stack overflow, a wild pointer or a misconfigured DMA overwrites it
static volatile uint32_t memory_guard_code = 0xA5A5DEAD;

void memory_guard(void)
{
  while(1) {
    if(memory_guard_code != 0xA5A5DEAD) panic("Memory corruption " LOG_TAG("SYS"));
    delay(500);
  }
}

//------------------------------------------------------------------------------------------- Delay

static TIM_t *sleep_tim;

void sleep_us_init(TIM_t *tim)
{
  sleep_tim = tim;
  DELAY_Init(tim, TIM_BaseTime_1us);
}

void sleep_us(uint32_t us)
{
  DELAY_Wait(sleep_tim, us);
}

//-------------------------------------------------------------------------------------------------

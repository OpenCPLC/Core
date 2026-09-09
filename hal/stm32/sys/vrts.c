// hal/stm32/sys/vrts.c

#include "vrts.h"

//------------------------------------------------------------------------------------------- State

__attribute__((weak)) void vrts_panic(const char *msg)
{
  unused(msg);
  __disable_irq();
  while(1);
}

volatile uint64_t VrtsTicker;
static uint32_t tick_ms; // system tick period [ms]

// Context switch operands of `PendSV_Handler` in `vrts_pendsv.s`
volatile VRTS_Task_t *vrts_now_thread;
volatile VRTS_Task_t *vrts_next_thread;

//----------------------------------------------------------------------------------------- Threads
#if(VRTS_SWITCHING)

#if(VRTS_THREAD_TIMEOUT_MS)
  static uint32_t hold_timeout;         // longest hold [ticks]
  static volatile uint32_t hold_ticker; // ticks left before `vrts_panic`
#endif

typedef struct {
  VRTS_Task_t threads[VRTS_THREAD_LIMIT];
  uint32_t i;            // active thread index
  uint32_t count;        // thread count
  volatile bool enabled; // switching enabled
  volatile bool init;    // `vrts_init` done
} VRTS_t;

static VRTS_t vrts;

// Landing pad for a handler that returns, the yield keeps the others running
static void task_finished(void)
{
  while(1) let();
}

bool vrts_thread(void (*handler)(void), uint32_t *stack, uint16_t size)
{
  if(vrts.count >= VRTS_THREAD_LIMIT) return false;
  VRTS_Task_t *thread = &vrts.threads[vrts.count];
  thread->handler = handler;
  #if defined(STM32WB)
  // M4 with FPU: 17 words = 8 hardware + 1 `EXC_RETURN` + 8 software-saved (r4-r11).
  // `EXC_RETURN` 0xFFFFFFFD: thread mode, PSP, no FPU context (FPCA bit 4 clear).
  // PendSV checks bit 4 to decide if lazy FPU state must be stacked on the next switch
  thread->stack = (uint32_t)(stack + size - 17);
  stack[size - 1] = (1 << 24); // XPSR: Thumb bit
  stack[size - 2] = (uint32_t)handler;
  stack[size - 3] = (uint32_t)&task_finished; // LR
  stack[size - 9] = 0xFFFFFFFD;
  for(int i = 10; i <= 17; i++) stack[size - i] = 0; // r4-r11
  #else
  // M0+: 16 words = 8 hardware + 8 software-saved, no FPU, no `EXC_RETURN` slot
  thread->stack = (uint32_t)(stack + size - 16);
  stack[size - 1] = (1 << 24); // XPSR: Thumb bit
  stack[size - 2] = (uint32_t)handler;
  stack[size - 3] = (uint32_t)&task_finished; // LR
  for(int i = 9; i <= 16; i++) stack[size - i] = 0; // r4-r11
  #endif
  vrts.count++;
  return true;
}

void vrts_init(void)
{
  NVIC_SetPriority(PendSV_IRQn, 3);
  vrts_now_thread = &vrts.threads[vrts.i];
  // PSP at the top of the first thread's frame
  #if defined(STM32WB)
  __set_PSP(vrts_now_thread->stack + 68);
  #else
  __set_PSP(vrts_now_thread->stack + 64);
  #endif
  __set_CONTROL(0x02); // PSP, privileged
  __ISB();
  vrts.enabled = true;
  vrts.init = true;
  vrts_now_thread->handler();
}

void vrts_lock(void)
{
  vrts.enabled = false;
}

bool vrts_unlock(void)
{
  if(!vrts.init) return false;
  vrts.enabled = true;
  return true;
}

void let(void)
{
  if(__get_IPSR() != 0) {
    vrts_panic("let() called from ISR");
    return;
  }
  if(!vrts.enabled) return;
  vrts_now_thread = &vrts.threads[vrts.i];
  vrts.i++;
  if(vrts.i >= vrts.count) vrts.i = 0;
  vrts_next_thread = &vrts.threads[vrts.i];
  #if(VRTS_THREAD_TIMEOUT_MS)
  hold_ticker = hold_timeout;
  #endif
  SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
  __DSB();
}

uint8_t vrts_active_thread(void)
{
  return vrts.i;
}

#else

void let(void)
{
  __WFI();
}

uint8_t vrts_active_thread(void)
{
  return 0;
}

#endif
//-------------------------------------------------------------------------------------------- Tick

/**
 * @brief Atomic 64-bit read of the ticker on a 32-bit core.
 * @return Current tick value
 */
static inline uint64_t ticker_get(void)
{
  uint32_t hi1, hi2, lo;
  do {
    hi1 = (uint32_t)(VrtsTicker >> 32);
    lo = (uint32_t)VrtsTicker;
    hi2 = (uint32_t)(VrtsTicker >> 32);
  } while(hi1 != hi2);
  return ((uint64_t)hi1 << 32) | lo;
}

uint64_t tick_keep(uint32_t offset_ms)
{
  if(!tick_ms) return ticker_get(); // before `systick_init`
  return ticker_get() + ((offset_ms + (tick_ms - 1)) / tick_ms);
}

uint64_t tick_now(void)
{
  return ticker_get();
}

bool tick_over(uint64_t *tick)
{
  if(!*tick || *tick > ticker_get()) return false;
  *tick = 0;
  return true;
}

bool tick_away(uint64_t *tick)
{
  if(!*tick) return false;
  if(*tick > ticker_get()) return true;
  *tick = 0;
  return false;
}

int32_t tick_diff(uint64_t tick)
{
  return (int32_t)(((int64_t)ticker_get() - tick) * tick_ms);
}

//------------------------------------------------------------------------------------------- Delay

void delay(uint32_t ms)
{
  uint64_t end = tick_keep(ms);
  while(end > ticker_get()) let();
}

void sleep(uint32_t ms)
{
  uint64_t end = tick_keep(ms);
  while(end > ticker_get()) __WFI();
}

bool timeout(uint32_t ms, bool (*Free)(void *), void *subject)
{
  uint64_t end = tick_keep(ms);
  while(end > ticker_get()) {
    if(Free(subject)) return false;
    let();
  }
  return true;
}

void delay_until(uint64_t *tick)
{
  if(!*tick) return;
  while(*tick > ticker_get()) let();
  *tick = 0;
}

void sleep_until(uint64_t *tick)
{
  if(!*tick) return;
  while(*tick > ticker_get()) __WFI();
  *tick = 0;
}

//----------------------------------------------------------------------------------------- SysTick

bool systick_init(uint32_t systick_ms)
{
  if(!systick_ms) return false;
  tick_ms = systick_ms;
  uint64_t reload = ((uint64_t)tick_ms * SystemCoreClock) / 1000;
  if(reload == 0 || reload > 0x00FFFFFF) return false; // 24-bit counter
  #if(VRTS_SWITCHING && VRTS_THREAD_TIMEOUT_MS)
  hold_timeout = VRTS_THREAD_TIMEOUT_MS / systick_ms;
  hold_ticker = hold_timeout;
  #endif
  if(SysTick_Config((uint32_t)reload)) return false;
  NVIC_SetPriority(SysTick_IRQn, 2); // above PendSV at 3
  return true;
}

void SysTick_Handler(void)
{
  VrtsTicker++;
  #if(VRTS_SWITCHING && VRTS_THREAD_TIMEOUT_MS)
  if(vrts.init) {
    hold_ticker--;
    if(!hold_ticker) vrts_panic("Thread overran core time limit");
  }
  #endif
}

//-------------------------------------------------------------------------------------------------

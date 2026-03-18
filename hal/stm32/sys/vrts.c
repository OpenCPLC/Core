// hal/stm32/sys/vrts.c

#include "vrts.h"
#include "log.h"

volatile uint64_t VrtsTicker;
static uint32_t tick_ms; // time in ms for a single ticker tick

volatile VRTS_Task_t *vrts_now_thread; // Current thread
volatile VRTS_Task_t *vrts_next_thread; // Next thread

#if(VRTS_SWITCHING)

#if(VRTS_THREAD_TIMEOUT_MS)
  #include "sys.h"
  static uint32_t hold_timeout;
  static volatile uint32_t hold_ticker;
#endif

// Structure to manage threads in the VRTS system
typedef struct {
  VRTS_Task_t threads[VRTS_THREAD_LIMIT];
  uint32_t i; // Active thread
  uint32_t count; // Thread count
  volatile bool enabled; // Switching VRTS enabled flag
  volatile bool init; // VRTS initialization flag
} VRTS_t;

static VRTS_t vrts;

/**
 * @brief Handles end of thread execution
 */
static void VRTS_TaskFinished(void)
{
  while(1) __WFI();
}

/**
 * @brief Adds a new thread to the VRTS system
 * @param handler Function pointer to the thread's main function
 * @param stack Pointer to the memory allocated for the thread's stack
 * @param size Size of the stack in 32-bit words (minimum: 80[M0+]/128[M4])
 * @return True if the thread was successfully added, false if the thread limit is reached
 */
bool vrts_thread(void (*handler)(void), uint32_t *stack, uint16_t size)
{
  if(vrts.count >= VRTS_THREAD_LIMIT - 1) return false;
  VRTS_Task_t *thread = &vrts.threads[vrts.count];
  thread->handler = handler;
  #if defined(STM32WB)
    thread->stack = (uint32_t)(stack + size - 17);
    stack[size - 1] = (1 << 24); // XPSR: Default value
    stack[size - 2] = (uint32_t)handler; // PC: Point to the handler function
    stack[size - 3] = (uint32_t)&VRTS_TaskFinished; // LR: handler return
    stack[size - 9] = 0xFFFFFFFD; // EXC_RETURN
    // Zero r4-r11 (software saved)
    for(int i = 10; i <= 17; i++) stack[size - i] = 0;
  #else
    thread->stack = (uint32_t)(stack + size - 16);
    stack[size - 1] = 0x01000000; // XPSR: Default value
    stack[size - 2] = (uint32_t)handler; // PC: Point to the handler function
    stack[size - 3] = (uint32_t)&VRTS_TaskFinished; // LR: handler return
    // Zero r4-r11 (software saved): positions size-9 to size-16
    for(int i = 9; i <= 16; i++) stack[size - i] = 0;
  #endif
  vrts.count++;
  return true;
}

/**
 * @brief Initializes the VRTS system and starts the first thread
 */
void vrts_init(void)
{
  NVIC_SetPriority(PendSV_IRQn, 3);
  vrts_now_thread = &vrts.threads[vrts.i];
  #if defined(STM32WB)
    __set_PSP(vrts_now_thread->stack + 68); // Set PSP to the top of thread's stack
  #else
    __set_PSP(vrts_now_thread->stack + 64); // Set PSP to the top of thread's stack
  #endif
  __set_CONTROL(0x02); // Switch to PSP, privileged mode
  __ISB(); // Exec. ISB after changing CONTROL (recommended)
  vrts.enabled = true;
  vrts.init = true;
  vrts_now_thread->handler();
}

/** @brief Disables thread switching by setting the enabled flag to false */
void vrts_lock(void)
{
  vrts.enabled = false;
}

/**
 * @brief Enables thread switching if VRTS is initialized
 * @return True if thread switching was enabled, false otherwise
 */
bool vrts_unlock(void)
{
  if(!vrts.init) return false;
  vrts.enabled = true;
  return true;
}

/**
 * @brief Yields control to the next thread in the schedule
 */
void let(void)
{
  // Guard: forbid let() from ISR context
  if(__get_IPSR() != 0) {
    panic("let() called from ISR" LOG_LIB("VRTS"));
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

#else
void let(void)
{
  __WFI();
}
#endif

/**
 * @brief Gets the index of the currently active thread
 * @return Index of the current thread
 */
uint8_t vrts_active_thread(void)
{
  #if(VRTS_SWITCHING)
    return vrts.i;
  #else
    return 0;
  #endif
}

/**
 * @brief Atomically reads 64-bit VrtsTicker on 32-bit core
 * @return Current tick value
 */
static inline uint64_t vrts_ticker_get(void)
{
  uint32_t hi1, hi2, lo;
  do {
    hi1 = (uint32_t)(VrtsTicker >> 32);
    lo  = (uint32_t)VrtsTicker;
    hi2 = (uint32_t)(VrtsTicker >> 32);
  } while(hi1 != hi2);
  return ((uint64_t)hi1 << 32) | lo;
}

/**
 * @brief Sets a deadline at current time + offset.
 * @param offset_ms Timeout in milliseconds.
 * @return Tick value representing the deadline.
 * @note Usage: `uint64_t deadline = tick_keep(100);` then check with `tick_over(&deadline)`.
 */
uint64_t tick_keep(uint32_t offset_ms)
{
  if(!tick_ms) return vrts_ticker_get(); // Guard: before systick_init
  return vrts_ticker_get() + ((offset_ms + (tick_ms - 1)) / tick_ms);
}

/**
 * @brief Returns current system tick.
 * @return Current tick value.
 */
uint64_t tick_now(void)
{
  return vrts_ticker_get();
}

/**
 * @brief Checks if deadline expired (one-shot trigger).
 * @param tick Pointer to deadline set by `tick_keep()`.
 * @return `true` once when deadline passes (auto-resets to 0), `false` otherwise.
 * @note Usage: `if(tick_over(&deadline)) { runs once when time's up }`
 */
bool tick_over(uint64_t *tick)
{
  if(!*tick || *tick > vrts_ticker_get()) return false;
  *tick = 0;
  return true;
}

/**
 * @brief Checks if deadline is still pending (continuous check).
 * @param tick Pointer to deadline set by `tick_keep()`.
 * @return `true` while waiting, `false` once when expired (auto-resets to 0).
 * @note Usage: `while(tick_away(&deadline)) { runs until time's up }`
 */
bool tick_away(uint64_t *tick)
{
  if(!*tick) return false;
  if(*tick > vrts_ticker_get()) return true;
  *tick = 0;
  return false;
}

/**
 * @brief Measures the time elapsed since a specified tick
 * @param tick Reference tick to measure from
 * @return Elapsed time in milliseconds
 */
int32_t tick_diff(uint64_t tick)
{
  return (int32_t)(((int64_t)vrts_ticker_get() - tick) * tick_ms);
}

/**
 * @brief Delays for the specified milliseconds.
 * Uses 'let()' to allow thread switching while waiting.
 * @param ms Milliseconds to delay
 */
void delay(uint32_t ms)
{
  uint64_t end = tick_keep(ms);
  while(end > vrts_ticker_get()) let();
}

/**
 * @brief Sleeps for the specified milliseconds.
 * Uses '__WFI()', so no thread switching occurs.
 * @param ms Milliseconds to sleep
 */
void sleep(uint32_t ms)
{
  uint64_t end = tick_keep(ms);
  while(end > vrts_ticker_get()) __WFI();
}

/**
 * @brief Checks a condition repeatedly until timeout or condition met
 * @param ms Timeout duration in milliseconds
 * @param Free Function pointer that checks the condition
 * @param subject Pointer to data for condition checking
 * @return True if timed out, false if condition met
 */
bool timeout(uint32_t ms, bool (*Free)(void *), void *subject)
{
  uint64_t end = tick_keep(ms);
  while(end > vrts_ticker_get()) {
    if(Free(subject)) return false;
    let();
  }
  return true;
}

/**
 * @brief Delays until the specified tick is reached.
 * Uses 'let()' to allow thread switching while waiting.
 * @param tick Pointer to the target tick count
 */
void delay_until(uint64_t *tick)
{
  if(!*tick) return;
  while(*tick > vrts_ticker_get()) let();
  *tick = 0;
}

/**
 * @brief Sleeps until the specified tick is reached
 * Uses '__WFI()', so no thread switching occurs
 * @param tick Pointer to the target tick count
 */
void sleep_until(uint64_t *tick)
{
  if(!*tick) return;
  while(*tick > vrts_ticker_get()) __WFI();
  *tick = 0;
}

/**
 * @brief Initializes SysTick with a specified interval
 * The accuracy of timing functions (e.g., 'sleep' and 'delay') will match this interval
 * @param systick_ms Interval duration in milliseconds
 * @return True if initialization is successful, false otherwise
 */
bool systick_init(uint32_t systick_ms)
{
  if(!systick_ms) return false;
  tick_ms = systick_ms;
  // Integer math: (tick_ms * SystemCoreClock) / 1000
  // Use uint64_t to avoid overflow for high clocks
  uint64_t reload = ((uint64_t)tick_ms * SystemCoreClock) / 1000;
  if(reload == 0 || reload > 0x00FFFFFF) return false; // SysTick 24-bit limit
  #if(VRTS_SWITCHING && VRTS_THREAD_TIMEOUT_MS)
    hold_timeout = VRTS_THREAD_TIMEOUT_MS / systick_ms;
    hold_ticker = hold_timeout;
  #endif
  if(SysTick_Config((uint32_t)reload)) return false;
  NVIC_SetPriority(SysTick_IRQn, 2); // Higher prio than PendSV(3)
  return true;
}

/** @brief SysTick interrupt handler to increment the global `VrtsTicker` */
void SysTick_Handler(void)
{
  VrtsTicker++;
  #if(VRTS_SWITCHING && VRTS_THREAD_TIMEOUT_MS)
    if(vrts.init) {
      hold_ticker--;
      if(!hold_ticker) panic("Thread overran core time limit" LOG_LIB("VRTS"));
    }
  #endif
}
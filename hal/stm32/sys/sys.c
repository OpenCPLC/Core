#include "sys.h"
#include "log.h"
#ifdef OpenCPLC
  #include "vrts.h"
#endif
#if(SYS_PANIC_RESET)
  #include "pwr.h"
#endif

/**
 * @brief Initialize system clock, SysTick and heap.
 */
void sys_init(void)
{
  clock_init();
  systick_init(SYS_BASETIME_ms);
  heap_init();
}

/**
 * @brief Initialize system clock according to `SYS_CLOCK_FREQ`.
 * Supported values:
 * `16000000`: internal HSI 16MHz,
 * `48000000`: internal HSI 48MHz,
 * `64000000`: internal HSI 64MHz,
 * `18432000`: external HSE 18.432MHz,
 * `59904000`: PLL (18.432MHz * 13 / 4).
 */
void clock_init(void)
{
  #if(SYS_CLOCK_FREQ == 16000000)
    RCC_16MHz();
  #elif(SYS_CLOCK_FREQ == 48000000)
    RCC_48MHz();
  #elif(SYS_CLOCK_FREQ == 64000000)
    RCC_64MHz();
  #elif(SYS_CLOCK_FREQ == 18432000)
    RCC_HSE(18432000);
  #elif(SYS_CLOCK_FREQ == 59904000)
    RCC_PLL(18432000, 2, 13, 2);
  #else
    #error "System clock frequency SYS_CLOCK_FREQ \
    is not defined or not supported!"
  #endif
}

static void (*panic_handler)(void);

/**
 * @brief Set custom panic handler.
 * @param handler Function called before system halt or reset.
 */
void panic_hook(void (*handler)(void))
{
  panic_handler = handler;
}

/**
 * @brief Trigger system panic.
 * @param message Panic message for log.
 * @note Function never returns. System is halted or reset if `SYS_PANIC_RESET` is enabled.
 */
void panic(const char *message)
{
  if(panic_handler) panic_handler();
  LOG_Panic(message);
  __disable_irq(); // Disable interrupts
  #if(SYS_PANIC_RESET)
    PWR_Reset();
  #endif
  volatile uint64_t i = 0;
  while(1) {
    i++; // Infinite loop to halt system
  }
}

volatile static uint32_t memory_guard_code = 0xA5A5DEAD;

/**
 * @brief Memory guard thread.
 * Periodically checks guard word and triggers `panic`
 * if memory corruption is detected.
 */
void memory_guard(void)
{
  while(1) {
    if(memory_guard_code != 0xA5A5DEAD) {
      panic("Memory corruption" LOG_LIB("SYS"));
    }
    delay(500);
  }
}

static TIM_t *sys_sleep_tim;

/**
 * @brief Initialize microsecond sleep using timer.
 * @param tim Pointer to timer instance used for delay.
 */
void sleep_us_init(TIM_t *tim)
{
  sys_sleep_tim = tim;
  DELAY_Init(tim, TIM_BaseTime_1us);
}

/**
 * @brief Busy-wait delay in microseconds.
 * @param us Delay time in microseconds.
 */
void sleep_us(uint32_t us)
{
  DELAY_Wait(sys_sleep_tim, us);
}

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
  // A fault inside the panic itself goes straight to the halt
  static bool panicking = false;
  __disable_irq(); // no interrupt cuts into the hook or the log, nor panics over them
  if(!panicking) {
    panicking = true;
    if(panic_handler) panic_handler();
    LOG_Panic(message);
  }
  #if(SYS_PANIC_RESET)
    PWR_Reset();
  #endif
  while(1);
}

void vrts_panic(const char *msg)
{
  panic(msg);
}

// An application definition of `HardFault_Handler` replaces the one here
#if(LOG_LEVEL <= LOG_LEVEL_PNC)

// `PC` of the faulting instruction, the seventh word of the frame, into the message
__attribute__((used)) static void hard_fault(const uint32_t *frame)
{
  static char message[] = "Hard fault at 0x00000000 " LOG_TAG("SYS");
  char *digit = message + sizeof("Hard fault at 0x") - 1;
  uint32_t pc = frame[6];
  for(uint8_t i = 0; i < 8; i++, pc <<= 4) *digit++ = "0123456789ABCDEF"[pc >> 28];
  panic(message);
}

// The frame the core stacked on entry: on `PSP` for a thread, on `MSP` for an interrupt
__attribute__((weak, naked)) void HardFault_Handler(void)
{
  __asm volatile(
    "movs r0, #4\n"
    "mov r1, lr\n"
    "tst r0, r1\n"
    "beq 1f\n"
    "mrs r0, psp\n"
    "ldr r1, =hard_fault\n"
    "bx r1\n"
    "1:\n"
    "mrs r0, msp\n"
    "ldr r1, =hard_fault\n"
    "bx r1\n"
  );
}

#else

// A log that prints no panic needs no address of the fault
__attribute__((weak)) void HardFault_Handler(void)
{
  panic("Hard fault");
}

#endif

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

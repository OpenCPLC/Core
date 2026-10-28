// .boot/main.c

#include <bld.h>
#if defined(BOOT_HOLD)
  #include <gpio.h>
#endif

#include "main.h"

int main(void)
{
  #if defined(BOOT_HOLD)
  // Power held first, before the clock and the signature, and never released:
  // a board kept alive by its processor would lose power on the way.
  GPIO_t hold = BOOT_HOLD;
  hold.mode = GPIO_Mode_Output;
  hold.set = true;
  GPIO_Init(&hold);
  #endif
  uint32_t app = BLD_Install();
  if(app) BLD_Jump(app);
  while(1) __WFI();
}

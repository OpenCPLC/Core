// hal/stm32g0/sys/pwr_g0.h

#ifndef PWR_G0_H_
#define PWR_G0_H_

#include <stdint.h>
#include <stdbool.h>

//------------------------------------------------------------------------------------------------- Platform-specific

void RAMP_PA11_PA12(void);

//------------------------------------------------------------------------------------------------- WakeupPin

typedef enum {
  PWR_Wakeup_PA0 = 0,   // WKUP1
  PWR_Wakeup_PA4 = 1,   // WKUP2 (25/28/32-pin)
  PWR_Wakeup_PC13 = 1,  // WKUP2 (48/64-pin, same bit)
  PWR_Wakeup_PA2 = 3,   // WKUP4
  PWR_Wakeup_PC5 = 4,   // WKUP5
  PWR_Wakeup_PB5 = 5    // WKUP6
} PWR_WakeupPin_t;

//-------------------------------------------------------------------------------------------------

#endif
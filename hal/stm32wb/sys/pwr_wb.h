// hal/stm32wb/sys/pwr_wb.h

#ifndef PWR_WB_H_
#define PWR_WB_H_

#include <stdint.h>
#include <stdbool.h>

//------------------------------------------------------------------------------------------------- WakeupPin

typedef enum {
  PWR_Wakeup_PA0 = 0,   // WKUP1
  PWR_Wakeup_PC13 = 1,  // WKUP2
  PWR_Wakeup_PC12 = 2,  // WKUP3
  PWR_Wakeup_PA2 = 3,   // WKUP4
  PWR_Wakeup_PC5 = 4    // WKUP5
} PWR_WakeupPin_t;

//-------------------------------------------------------------------------------------------------

#endif
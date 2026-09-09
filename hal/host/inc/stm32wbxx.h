// hal/host/inc/stm32wbxx.h

// Stand-in for the STM32WB device header, just enough for the WB libraries
// that carry no register access of their own (`ble_wb.c`).

#ifndef STM32WBXX_H_
#define STM32WBXX_H_

#include <stdint.h>

// Unique device number under test control, defined by the mailbox model
extern uint8_t HOST_UID64[8];
#define UID64_BASE ((uintptr_t)HOST_UID64)

//-------------------------------------------------------------------------------------------------
#endif

// hal/stm32g0/itf/usb_g0.h

#ifndef USB_G0_H_
#define USB_G0_H_

#include <stm32g0xx.h>

//-------------------------------------------------------------------------------------------------

// Only `G0B1`/`G0C1` line carries controller.
#ifdef USB_DRD_FS
  #define USB_PRESENT 1
#else
  #define USB_PRESENT 0
#endif

#define USB_PMA_SIZE 2048   // Packet memory [B], `32`-bit access
#define USB_PMA_TABLE 64    // Buffer descriptors at offset `0`: `2` words for `8` endpoints

//-------------------------------------------------------------------------------------------------

#endif

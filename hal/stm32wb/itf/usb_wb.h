// hal/stm32wb/itf/usb_wb.h

#ifndef USB_WB_H_
#define USB_WB_H_

#include <stm32wbxx.h>

//-------------------------------------------------------------------------------------------------

#define USB_PRESENT 1
#define USB_PMA_SIZE 1024   // Packet memory [B], `16`-bit access without gaps
#define USB_PMA_TABLE 64    // Buffer table at offset `0`: `4` halfwords for `8` endpoints

//-------------------------------------------------------------------------------------------------

#endif

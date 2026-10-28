// hal/host/sys/bld.c

// Bootloader module on the host flash model, so a console with `boot` builds in a test.
// The host `pwr.h` comes first: its guard keeps out the chip one beside `bld.c`.
#include "pwr.h"
#include "../../stm32/sys/bld.c"

// Report of the last start, in the RAM the linker script sets apart on the chip
BLD_Report_t _bld_report;

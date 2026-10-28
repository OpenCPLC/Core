// .tests/test_bld_dev.c

// Bootloader tests built again for the bootloader of builds: `BOOT_DEV_KEY` stands for every key
// of `BOOT_KEYS`, and the floors stay in the slot even with `BOOT_OTP_WORDS` set.

#include "data/forge.h"
#define BOOT_DEV_KEY FORGE_DEV_KEY
#define BOOT_OTP_WORDS 64
#include "test_bld_key.c"

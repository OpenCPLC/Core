// .tests/test_bld_key_slot.c

// Bootloader tests built again for a `key` bootloader on a chip without OTP:
// the floors come from the header of the image in the application slot.

#define BOOT_OTP_WORDS 0
#include "test_bld_key.c"

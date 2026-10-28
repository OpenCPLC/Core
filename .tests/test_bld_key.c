// .tests/test_bld_key.c

// Bootloader tests built again for the `key` bootloader with the three test keys of `forge.h`,
// the application side under it in the same file: every image carries a test signature.

#include "data/forge.h"
#define BOOT_KEYS FORGE_KEYS
#include "test_bld.c"

// .boot/main.h

/**
 * @name  Bootloader of Core
 * @brief Installs an image waiting in the staging slot,
 *        then hands the core to the application slot.
 *        The logic is `BLD_Install`; the program adds the power hold of `BOOT_HOLD`,
 *        and with no image to run it waits for the programmer.
 *        Forge compiles it under every application whose `main.h` includes `boot.h`,
 *        with the `BOOT_` settings of that file, so one program serves every product.
 *        What the program needs of Core is fixed here;
 *        what the product decides comes from `boot.h` of the project.
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <xdef.h>
#include "boot.h"

#endif

// Framework settings of the bootloader, applied on every include
#define LOG_LEVEL LOG_LEVEL_NIL
#define SYS_CLOCK_FREQ 16000000
#define VRTS_THREAD_TIMEOUT_MS 0
#define PDB_QUERY 0 // the mailbox takes init, insert, delete and a walk, no queries

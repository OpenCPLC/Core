// .tests/uno/main.h

/**
 * @name  Project: .tests/uno
 * @brief Project configuration, read by Forge on every load and by the framework at build.
 *        Edit values, keep definitions; `make` reloads the project after a change.
 * @date  2026-10-08
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <xdef.h>

#define PRO_BOARD_Uno
#define PRO_CHIP_STM32G0C1
#define PRO_PLC true
#define PRO_FLASH_kB 492
#define PRO_RAM_kB 144
#define PRO_BOOT false
#define PRO_OPT_LEVEL "Og"

#endif

// Framework settings this project overrides, applied on every include
#define LOG_LEVEL LOG_LEVEL_INF
#define SYS_CLOCK_FREQ 59904000

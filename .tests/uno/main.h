// .tests/uno/main.h

/**
 * @name  Project: .tests/uno
 * @brief Compile test of PLC layer on Uno: board, its drivers and `plc/` have to build and link.
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
#define PRO_OPT_LEVEL "Og"

#endif

// Framework settings this project overrides, applied on every include
#define LOG_LEVEL LOG_LEVEL_INF
#define SYS_CLOCK_FREQ 59904000

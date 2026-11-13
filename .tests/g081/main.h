// .tests/g081/main.h

/**
 * @name  Project: .tests/g081
 * @brief Test of Core for STM32G081:
 *        CI builds every file the chip pulls in, the board runs the cases of `main.c`.
 * @date  2026-10-08
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <xdef.h>

#define PRO_BOARD_None
#define PRO_CHIP_STM32G081
#define PRO_PLC false
#define PRO_FLASH_kB 128
#define PRO_RAM_kB 36
#define PRO_OPT_LEVEL "Og"

#endif

// Framework settings this project overrides, applied on every include
#define LOG_LEVEL LOG_LEVEL_INF
#define SYS_CLOCK_FREQ 16000000

// .tests/g0c1/main.h

/**
 * @name  Project: .tests/g0c1
 * @brief Compile test of Core for STM32G0C1: every file the chip pulls in has to build and link.
 * @date  2026-10-08
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <xdef.h>

#define PRO_BOARD_None
#define PRO_CHIP_STM32G0C1
#define PRO_PLC false
#define PRO_FLASH_kB 512
#define PRO_RAM_kB 144
#define PRO_OPT_LEVEL "Og"

#endif

// Framework settings this project overrides, applied on every include
#define LOG_LEVEL LOG_LEVEL_INF
#define SYS_CLOCK_FREQ 16000000

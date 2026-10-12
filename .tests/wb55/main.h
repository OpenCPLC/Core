// .tests/wb55/main.h

/**
 * @name  Project: .tests/wb55
 * @brief Compile test of Core for STM32WB55: every file the chip pulls in has to build and link.
 * @date  2026-10-08
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <xdef.h>

#define PRO_BOARD_None
#define PRO_CHIP_STM32WB55
#define PRO_PLC false
#define PRO_FLASH_kB 832
#define PRO_RAM_kB 192
#define PRO_BOOT false
#define PRO_OPT_LEVEL "Og"

#endif

// Framework settings this project overrides, applied on every include
#define LOG_LEVEL LOG_LEVEL_INF
#define SYS_CLOCK_FREQ 16000000

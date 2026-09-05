// hal/stm32wb/sys/hsem_wb.h

// Hardware semaphores shared with the CPU2 wireless firmware.
// The identifiers are fixed by the ST protocol, the CPU2 side counts on them.

#ifndef HSEM_WB_H_
#define HSEM_WB_H_

#include <stdbool.h>
#include <stdint.h>

#include "stm32wbxx.h"

//----------------------------------------------------------------------------------------- Presets

#define HSEM_RNG         0 // the peripheral itself, held around every generation
#define HSEM_PKA         1
#define HSEM_FLASH       2 // flash erase and write arbitration
#define HSEM_RCC         3
#define HSEM_STOP        4 // entry into stop mode
#define HSEM_CLK48       5 // CLK48 configuration: held means the CPU2 leaves HSI48 alone
#define HSEM_FLASH_CPU1  6 // CPU1 asks the CPU2 to pause its flash activity
#define HSEM_FLASH_CPU2  7 // and the other way around
#define HSEM_BLE_NVM     8 // CPU2 pauses its BLE persistent data updates in SRAM2
#define HSEM_PWR_STANDBY 10

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Try to take a semaphore (one-step lock).
 * @param[in] id Semaphore identifier
 * @return `true` when taken, also when this core holds it already
 */
bool HSEM_Take(uint8_t id);

/**
 * @brief Spin until the semaphore is taken.
 *   The CPU2 holds these for microseconds, so the wait is short by protocol.
 * @param[in] id Semaphore identifier
 */
void HSEM_Wait(uint8_t id);

/**
 * @brief Release a semaphore taken by this core.
 * @param[in] id Semaphore identifier
 */
void HSEM_Give(uint8_t id);

//-------------------------------------------------------------------------------------------------
#endif

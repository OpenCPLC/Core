// hal/stm32/per/flash.h

#ifndef FLASH_H_
#define FLASH_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "xdef.h"
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#elif defined(STM32G4)
  #include "stm32g4xx.h"
#endif

//------------------------------------------------------------------------------------------------- Compatibility Layer

#if defined(STM32G0)
  #define FLASH_PAGE_SIZE   ((uint32_t)0x00000800) // 2KB
  #ifdef STM32G081xx
    #define FLASH_PAGES     64
  #elif defined(STM32G0C1xx)
    #define FLASH_PAGES     256
  #endif
#elif defined(STM32WB)
  #define FLASH_PAGE_SIZE   ((uint32_t)0x00001000) // 4KB
  #define FLASH_PAGES       ((uint16_t)(FLASH_SIZE / FLASH_PAGE_SIZE))
#elif defined(STM32G4)
  #define FLASH_PAGE_SIZE   ((uint32_t)0x00000800) // 2KB
  #define FLASH_PAGES       ((uint16_t)(FLASH_SIZE / FLASH_PAGE_SIZE))
#endif

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Erase Flash page.
 * @param[in] page Page index
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Erase(uint16_t page);

/**
 * @brief Get Flash address for page with offset.
 * @param[in] page Page index
 * @param[in] offset Byte offset inside page
 * @return Address in Flash
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset);

/**
 * @brief Read 32-bit word from Flash.
 * @param[in] addr Flash address
 * @return Value at address
 */
uint32_t FLASH_Read(uint32_t addr);

/**
 * @brief Write double word (64-bit) to Flash.
 * @param[in] addr Flash address (must be 8-byte aligned)
 * @param[in] data1 Lower 32 bits
 * @param[in] data2 Upper 32 bits
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2);

/**
 * @brief Fast write 256 bytes to Flash (row programming).
 * @param[in] addr Block address (must be 256-byte aligned)
 * @param[in] data Pointer to 256-byte buffer
 * @return `OK` on success, `ERR` on error
 * @note Function runs from RAM (`.data#` section)
 */
status_t FLASH_WriteFast(uint32_t addr, uint32_t *data) __attribute__((section(".data#")));

/**
 * @brief Write full Flash page.
 * @param[in] page Page index
 * @param[in] data Pointer to buffer (`FLASH_PAGE_SIZE` bytes)
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_WritePage(uint16_t page, uint8_t *data);

/**
 * @brief Compare Flash page with buffer.
 * @param[in] page Page index
 * @param[in] data Pointer to data buffer
 * @param[in] size Buffer size in bytes
 * @return `true` if equal, `false` if different
 */
bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size);

/**
 * @brief Save data to Flash with size header.
 * @param[in] page Starting page index
 * @param[in] data Pointer to buffer
 * @param[in] size Buffer size in bytes
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Save(uint16_t page, uint8_t *data, uint16_t size);

/**
 * @brief Load data from Flash.
 * @param[in] page Page index
 * @param[out] data Pointer to buffer
 * @return Size in bytes, `0` if empty or error
 */
uint16_t FLASH_Load(uint16_t page, uint8_t *data);

//-------------------------------------------------------------------------------------------------

#endif
// hal/stm32/per/flash.h

#ifndef FLASH_H_
#define FLASH_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include <string.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif

//--------------------------------------------------------------------------------------- Constants

#if defined(STM32G0)
  #define FLASH_PAGE_SIZE 0x800u // 2KB
#elif defined(STM32WB)
  #define FLASH_PAGE_SIZE 0x1000u // 4KB
#endif
// Page count from the size register of the device, one image per family
#define FLASH_PAGES ((uint16_t)(FLASH_SIZE / FLASH_PAGE_SIZE))

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Erase a page, skipped when it reads erased already.
 * @param[in] page Page index
 * @return `OK` on success, `ERR` on a flash error or a page still holding data
 */
status_t FLASH_Erase(uint16_t page);

/**
 * @brief Address of a byte inside a page.
 * @param[in] page Page index
 * @param[in] offset Byte offset inside the page
 * @return Address in flash
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset);

/**
 * @brief Word at an address.
 * @param[in] addr Flash address
 * @return Value at the address
 */
uint32_t FLASH_Read(uint32_t addr);

/**
 * @brief Pointer to a flash address. Flash is memory mapped, the host twin resolves
 *   the same call into its image, so shared code reads flash through it.
 * @param[in] addr Flash address
 * @return Pointer to the byte
 */
static inline void *FLASH_Ref(uint32_t addr) { return (void *)addr; }

/**
 * @brief Program one doubleword, the unit flash accepts.
 * @param[in] addr Flash address, 8-byte aligned
 * @param[in] data1 Lower 32 bits
 * @param[in] data2 Upper 32 bits
 * @return `OK` when the words read back, `ERR` on a flash error or a misaligned `addr`
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2);

/**
 * @brief Program a 256-byte row. Runs from RAM: flash cannot be read while a row programs.
 * @param[in] addr Row address, 256-byte aligned
 * @param[in] data 64 words
 * @return `OK` when the row reads back, `ERR` on a flash error or a misaligned `addr`
 */
status_t FLASH_WriteFast(uint32_t addr, const uint32_t *data) __attribute__((section(".data#")));

/**
 * @brief Erase and program a whole page.
 * @param[in] page Page index
 * @param[in] data `FLASH_PAGE_SIZE` bytes
 * @return `OK` on success, `ERR` on a flash error
 */
status_t FLASH_WritePage(uint16_t page, const uint8_t *data);

/**
 * @brief Compare a record saved with `FLASH_Save` against a buffer.
 * @param[in] page Page index
 * @param[in] data Bytes to compare
 * @param[in] size Number of bytes
 * @return `true` when a record of `size` bytes with this content is stored
 */
bool FLASH_Compare(uint16_t page, const uint8_t *data, uint16_t size);

/**
 * @brief Save a record with its size, over as many pages as it needs.
 *   The size header is written last, so a torn save reads back as no record at all.
 * @param[in] page First page
 * @param[in] data Bytes to save
 * @param[in] size Number of bytes, `0` is refused
 * @return `OK` on success, `ERR` on a flash error or a record past the end of flash
 */
status_t FLASH_Save(uint16_t page, const uint8_t *data, uint16_t size);

/**
 * @brief Load a record saved with `FLASH_Save`.
 * @param[in] page First page
 * @param[out] data Room for the record
 * @return Record size [B], `0` when the page holds none
 */
uint16_t FLASH_Load(uint16_t page, uint8_t *data);

//-------------------------------------------------------------------------------------------------
#endif

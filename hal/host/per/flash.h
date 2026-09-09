// hal/host/per/flash.h

#ifndef FLASH_H_
#define FLASH_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "xdef.h"

//------------------------------------------------------------------------------------------ Config

#ifndef FLASH_PAGE_SIZE
  // Page size in bytes, matching the emulated target
  #define FLASH_PAGE_SIZE 2048
#endif

#ifndef FLASH_PAGES
  // Page count, sets total emulated size and the RAM image footprint
  #define FLASH_PAGES 256
#endif

#ifndef FLASH_DIR
  // Directory holding page images
  #define FLASH_DIR "."
#endif

#ifndef FLASH_PREFIX
  // Page image filename prefix
  #define FLASH_PREFIX "flash_page_"
#endif

//--------------------------------------------------------------------------------------------- API

/**
 * @brief NOR Flash emulation.
 * Memory is mirrored in RAM and every modified page is written back
 * to `<FLASH_DIR>/FLASH_PREFIX<page>.bin` as a raw `FLASH_PAGE_SIZE` image.
 * A page without a file is erased, so a fresh directory is blank Flash.
 * A file of any other size is not a page image and is ignored, never merged.
 * An address is virtual (see `FLASH_GetAddress`) and cannot be dereferenced:
 * use `FLASH_Read` or `FLASH_Ref`.
 * NOR rules hold, matching the target driver:
 * - a write only clears bits, an erase restores `0xFF`
 * - writes are doubleword (8B) aligned
 * - programming a location that is not erased fails (`PROGERR` on target)
 * Every write is flushed before it returns `OK`,
 * so a crash leaves the state a power loss would,
 * and `ERR` always means nothing was written.
 */

// Initialize Flash emulation, creating the directory if needed
void FLASH_Init(void);

/**
 * @brief Erase Flash page.
 * @param[in] page Page index
 * @return `OK` on success, `ERR` if the page image could not be dropped
 */
status_t FLASH_Erase(uint16_t page);

/**
 * @brief Get Flash address for page with offset.
 * @param[in] page Page index
 * @param[in] offset Byte offset inside page
 * @return Virtual address (`page * FLASH_PAGE_SIZE + offset`)
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset);

/**
 * @brief Read 32-bit word from Flash.
 * @param[in] addr Flash address
 * @return Value at address, `0xFFFFFFFF` when out of range
 */
uint32_t FLASH_Read(uint32_t addr);

/**
 * @brief Direct reference into emulated Flash image.
 * Replaces the pointer cast used on target, where Flash is memory-mapped.
 * Valid until next erase or write. Bounds-checks one byte, not a span.
 * @param[in] addr Flash address
 * @return Pointer to emulated cell, `NULL` when out of range
 */
void *FLASH_Ref(uint32_t addr);

/**
 * @brief Write double word (64-bit) to Flash.
 * @param[in] addr Flash address (must be 8-byte aligned)
 * @param[in] data1 Lower 32 bits
 * @param[in] data2 Upper 32 bits
 * @return `OK` on success, `ERR` on misaligned `addr` or non-erased target
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2);

/**
 * @brief Fast write 256 bytes to Flash (row programming).
 * @param[in] addr Block address (must be 256-byte aligned)
 * @param[in] data Pointer to 256-byte buffer
 * @return `OK` on success, `ERR` on error or misaligned `addr`
 */
status_t FLASH_WriteFast(uint32_t addr, uint32_t *data);

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

/**
 * @brief Set directory holding page images.
 * Call before first Flash access. Reloads the image from the new location.
 * @param[in] path Directory path (copied internally)
 */
void FLASH_SetDirectory(const char *path);

//-------------------------------------------------------------------------------------------------
#endif

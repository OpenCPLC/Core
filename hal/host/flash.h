// hal/host/flash.h

#ifndef FLASH_H_
#define FLASH_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "xdef.h"

//------------------------------------------------------------------------------------------------- Config

#ifndef FLASH_PAGE_SIZE
  #define FLASH_PAGE_SIZE 2048
#endif

#ifndef FLASH_PAGES
  #define FLASH_PAGES 256
#endif

#ifndef FLASH_DIR
  // directory for flash files
  #define FLASH_DIR "." 
#endif

#ifndef FLASH_PREFIX
  // filename prefix
  #define FLASH_PREFIX "flash_page_" 
#endif

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize flash emulation (create directory if needed).
 */
void FLASH_Init(void);

/**
 * @brief Erase flash page (delete file).
 * @param[in] page Page index
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Erase(uint16_t page);

/**
 * @brief Get virtual address for page (for compatibility).
 * @param[in] page Page index
 * @param[in] offset Byte offset inside page
 * @return Virtual address (page * PAGE_SIZE + offset)
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset);

/**
 * @brief Read 32-bit word from flash (not supported - use FLASH_Load).
 * @param[in] addr Virtual address
 * @return Always 0xFFFFFFFF (erased state)
 */
uint32_t FLASH_Read(uint32_t addr);

/**
 * @brief Write double word (compatibility stub).
 * @param[in] addr Virtual address
 * @param[in] data1 Lower 32 bits
 * @param[in] data2 Upper 32 bits
 * @return `ERR` (use FLASH_Save instead)
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2);

/**
 * @brief Fast write (compatibility stub).
 * @param[in] addr Virtual address
 * @param[in] data Data pointer
 * @return `ERR` (use FLASH_Save instead)
 */
status_t FLASH_WriteFast(uint32_t addr, uint32_t *data);

/**
 * @brief Write full page (compatibility stub).
 * @param[in] page Page index
 * @param[in] data Data pointer
 * @return `ERR` (use FLASH_Save instead)
 */
status_t FLASH_WritePage(uint16_t page, uint8_t *data);

/**
 * @brief Compare flash page with buffer.
 * @param[in] page Page index
 * @param[in] data Pointer to data buffer
 * @param[in] size Buffer size in bytes
 * @return `true` if equal, `false` if different
 */
bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size);

/**
 * @brief Save data to flash file.
 * @param[in] page Page index (determines filename)
 * @param[in] data Pointer to buffer
 * @param[in] size Buffer size in bytes
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Save(uint16_t page, uint8_t *data, uint16_t size);

/**
 * @brief Load data from flash file.
 * @param[in] page Page index
 * @param[out] data Pointer to buffer
 * @return Size in bytes, `0` if empty or error
 */
uint16_t FLASH_Load(uint16_t page, uint8_t *data);

/**
 * @brief Set flash directory path.
 * @param[in] path Directory path (copied internally)
 */
void FLASH_SetDirectory(const char *path);

//-------------------------------------------------------------------------------------------------

#endif
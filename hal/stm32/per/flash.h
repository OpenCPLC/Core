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
#endif

//--------------------------------------------------------------------------------------- Constants

#if defined(STM32G0)
  #define FLASH_PAGE_SIZE 0x800u // 2KB
  // Page count from the size register of the device, one image per family
  #define FLASH_PAGES ((uint16_t)(FLASH_SIZE / FLASH_PAGE_SIZE))
#elif defined(STM32WB)
  #define FLASH_PAGE_SIZE 0x1000u // 4KB
  // Pages of CPU1: they end where the secure flash of CPU2, its radio stack and FUS, begins,
  // at page `SFSA`, unless `FSD` turns that security off.
  #define FLASH_PAGES ((uint16_t)(FLASH->SFR & FLASH_SFR_FSD ? \
    FLASH_SIZE / FLASH_PAGE_SIZE : FLASH->SFR & FLASH_SFR_SFSA))
#endif

// OTP: doublewords `FLASH_Write` programs once and `FLASH_Copy` reads through ECC like flash,
// which no erase reaches, a mass erase of a readout regression included.
// The G0 header leaves its address out, RM0444 gives it.
#if defined(STM32G0)
  #define FLASH_OTP_BASE 0x1FFF7000u
#elif defined(STM32WB)
  #define FLASH_OTP_BASE OTP_AREA_BASE
#endif
#define FLASH_OTP_SIZE 1024u // 128 doublewords

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Erase a page, skipped when it reads erased already, ECC correcting none of it.
 * @param[in] page Page index
 * @return `OK` on success, `ERR` on a flash error or a page still holding data
 */
status_t FLASH_Erase(uint16_t page);

/**
 * @brief Erase a page through a whole cycle, also one reading blank:
 *   after an erase cut late it reads blank but holds no program until erased whole.
 * @param[in] page Page index
 * @return `OK` on success, `ERR` on a flash error or a page still holding data
 */
status_t FLASH_EraseFull(uint16_t page);

/**
 * @brief Address of a byte inside a page.
 * @param[in] page Page index
 * @param[in] offset Byte offset inside the page
 * @return Address in flash
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset);

/**
 * @brief Word at an address, for flash no cut can have torn: code, constants.
 *   Data a cut may have torn comes through `FLASH_Copy`.
 * @param[in] addr Flash address
 * @return Value at the address
 */
uint32_t FLASH_Read(uint32_t addr);

/**
 * @brief Read range once, checking every doubleword against ECC.
 *   A write cut by reset or power loss can leave a doubleword whose read raises NMI;
 *   here it gives `ERR` instead. A verdict only: a doubleword a cut tore can pass here
 *   and fail a later read (WB55, through its data cache), so data comes through `FLASH_Copy`.
 * @param[in] addr Flash address
 * @param[in] size Number of bytes
 * @param[out] corrected Set when ECC corrected a doubleword of the range, `NULL` to skip
 * @return `OK` when every doubleword reads, `ERR` at the first one ECC cannot correct
 */
status_t FLASH_Check(uint32_t addr, uint32_t size, bool *corrected);

/**
 * @brief Copy a range into RAM, each doubleword read once and checked as `FLASH_Check` does:
 *   what lands in `data` is what passed ECC. The read for data a cut may have torn.
 * @param[in] addr Flash address
 * @param[out] data Room for `size` bytes, part of them written when it fails
 * @param[in] size Number of bytes
 * @param[out] corrected Set when ECC corrected a doubleword of the range, `NULL` to skip
 * @return `OK` when every doubleword reads, `ERR` at the first one ECC cannot correct
 */
status_t FLASH_Copy(uint32_t addr, void *data, uint32_t size, bool *corrected);

/**
 * @brief Pointer to a flash address. Flash is memory mapped, the host twin resolves
 *   the same call into its image, so shared code reads flash through it.
 *   Data a cut may have torn comes through `FLASH_Copy` instead.
 * @param[in] addr Flash address
 * @return Pointer to the byte
 */
static inline void *FLASH_Ref(uint32_t addr) { return (void *)addr; }

/**
 * @brief Program one doubleword, the unit flash accepts. Ones program nothing.
 * @param[in] addr Flash address, 8-byte aligned
 * @param[in] data1 Lower 32 bits
 * @param[in] data2 Upper 32 bits
 * @return `OK` when the words read back, `ERR` on a flash error or a misaligned `addr`
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2);

#if defined(STM32G0)

/**
 * @brief Program a 256-byte row of G0, into the page erased last.
 *   Runs from RAM: flash cannot be read while a row programs.
 * @param[in] addr Row address, 256-byte aligned
 * @param[in] data 64 words in RAM
 * @return `OK` when the row reads back, `ERR` on a flash error or a misaligned `addr`
 */
status_t FLASH_WriteFast(uint32_t addr, const uint32_t *data) __attribute__((section(".data#")));

#endif

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
 * @param[out] data Room for the record, part of it written when it holds none
 * @return Record size [B], `0` when the page holds none
 */
uint16_t FLASH_Load(uint16_t page, uint8_t *data);

/**
 * @brief Write an option register and load the option bytes, which resets the chip.
 *   Not for RDP, whose change takes a power-on reset instead and which `opencplc --lock` sets.
 * @param[in] reg Option register of `FLASH`, such as `&FLASH->OPTR`
 * @param[in] value New value of the register
 * @return `ERR` when flash or its options stay locked or the write is refused;
 *   no return otherwise, the chip resets
 */
status_t FLASH_OptionWrite(volatile uint32_t *reg, uint32_t value);

/**
 * @brief Protect pages `first` to `last` against erase and write through area B of their bank,
 *   for code and programmer alike, from the reset that loads it.
 *   Area A stays for the bootloader, which `opencplc --lock` protects,
 *   and `opencplc --lock 0` takes both off.
 * @param[in] first First page
 * @param[in] last Last page, in the bank of `first`
 * @return `OK` when area B holds this range already, `ERR` on pages past flash or over two banks
 *   and on a refused write; no return otherwise, the chip resets
 */
status_t FLASH_Protect(uint16_t first, uint16_t last);

//-------------------------------------------------------------------------------------------------
#endif

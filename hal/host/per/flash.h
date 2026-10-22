// hal/host/per/flash.h

#ifndef FLASH_H_
#define FLASH_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef FLASH_PAGE_SIZE
  // Page of the chip the build stands for [B]
  #if defined(STM32WB55xx)
    #define FLASH_PAGE_SIZE 4096
  #else
    #define FLASH_PAGE_SIZE 2048
  #endif
#endif

#ifndef FLASH_PAGES
  // Pages of CPU1, as its driver reads them: the size register on G0,
  // on WB55 up to the radio stack, which the BLE stack of a Nucleo starts at page 208
  #if defined(STM32G081xx)
    #define FLASH_PAGES 64
  #elif defined(STM32WB55xx)
    #define FLASH_PAGES 208
  #else
    #define FLASH_PAGES 256
  #endif
#endif

#ifndef FLASH_DIR
  // Folder keeping the pages of a host application between its runs, until `FLASH_SetDirectory`
  #define FLASH_DIR "."
#endif

//--------------------------------------------------------------------------------------- Constants

#define FLASH_BASE 0x08000000u // where the chip maps its flash

//--------------------------------------------------------------------------------------------- API

// Flash of the chip, modelled in RAM at the addresses of the chip, as its driver acts on it:
// - an erase sets the page to `0xFF` and every doubleword in it fresh again
// - a doubleword takes one write after its erase, the way ECC flash holds it;
//   zeros may go over one not written with ones, anything else ends in `PROGERR`,
//   and zeros leave a doubleword ECC reads clean, whatever it held
// - ones program nothing, an erased doubleword reads them already
// - `FLASH_Erase` spares a page that reads blank, ones a programmer wrote and all,
//   but not one ECC corrects to ones, as an erase cut halfway leaves it;
//   `FLASH_EraseFull` erases it all the same
// - a row goes only into the page erased last, over fresh doublewords
// - `FLASH_WritePage` erases, blank or not, and programs in rows, on WB in doublewords
// - a cycle cut halfway, program or erase, leaves the doublewords it touched unreadable to ECC,
//   read back corrected to a wrong value, or read erased and unreadable by turns,
//   see `FLASH_Cut_t`; a plain read of one stops the model, as NMI the chip,
//   so data a cut may have torn comes through `FLASH_Copy`
// - an erase cut late leaves its page reading erased, its cells short of a full erase:
//   they hold no program until the page is erased whole
// - a bit flipped by age ECC corrects, two it cannot read, see `FLASH_HostAge`
// A host application keeps its pages in `FLASH_DIR` between runs.
// A test owns the chip in RAM and cuts the power where it wants, see `FLASH_HostCut`.

/**
 * @brief Erase a page, skipped when it reads erased already, ECC correcting none of it.
 * @param[in] page Page index
 * @return `OK` erased, `ERR` beyond the chip
 */
status_t FLASH_Erase(uint16_t page);

/**
 * @brief Erase a page through a whole cycle, blank or not:
 *   a page an erase cut late left reading blank comes out erased whole.
 * @param[in] page Page index
 * @return `OK` erased, `ERR` beyond the chip
 */
status_t FLASH_EraseFull(uint16_t page);

/**
 * @brief Address of a byte inside a page.
 * @param[in] page Page index
 * @param[in] offset Byte offset inside the page
 * @return Address on the chip, from `FLASH_BASE`
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset);

/**
 * @brief Word at an address, for flash no cut can have torn: on a doubleword a cut tore
 *   or ECC cannot read, the model stops as the chip does on NMI.
 * @param[in] addr Flash address
 * @return Value at the address, `0xFFFFFFFF` beyond the chip
 */
uint32_t FLASH_Read(uint32_t addr);

/**
 * @brief Read range once, checking every doubleword against ECC, as the chip does.
 *   The model fails and corrects the doublewords `FLASH_HostCut` and `FLASH_HostAge` leave so.
 * @param[in] addr Flash address
 * @param[in] size Number of bytes
 * @param[out] corrected Set when ECC corrected a doubleword of the range, `NULL` to skip
 * @return `OK` when every doubleword reads, `ERR` at the first one ECC cannot correct
 */
status_t FLASH_Check(uint32_t addr, uint32_t size, bool *corrected);

/**
 * @brief Copy a range into RAM through ECC, as the chip does: the read for data a cut may
 *   have torn. The model fails and corrects the doublewords `FLASH_Check` does.
 * @param[in] addr Flash address
 * @param[out] data Room for `size` bytes, part of them written when it fails
 * @param[in] size Number of bytes
 * @param[out] corrected Set when ECC corrected a doubleword of the range, `NULL` to skip
 * @return `OK` when every doubleword reads, `ERR` at the first one ECC cannot correct
 */
status_t FLASH_Copy(uint32_t addr, void *data, uint32_t size, bool *corrected);

/**
 * @brief Pointer to a flash address, the cast the chip makes on its mapped flash.
 *   Valid until the next erase or write, bounds checked for one byte.
 * @param[in] addr Flash address
 * @return Pointer into the model, `NULL` beyond the chip
 */
void *FLASH_Ref(uint32_t addr);

/**
 * @brief Program one doubleword, the unit flash accepts. Ones program nothing.
 * @param[in] addr Flash address, 8-byte aligned
 * @param[in] data1 Lower word
 * @param[in] data2 Upper word
 * @return `OK` when the doubleword holds the words, `ERR` misaligned, beyond the chip,
 *   over a written doubleword or on a page an erase cut late left short of a full erase
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2);

#if !defined(STM32WB55xx)

/**
 * @brief Program a 256-byte row, G0 only.
 * @param[in] addr Row address, 256-byte aligned, in the page erased last
 * @param[in] data 64 words
 * @return `OK` programmed, `ERR` misaligned, in another page, over a written doubleword
 *   or on a page an erase cut late left short of a full erase
 */
status_t FLASH_WriteFast(uint32_t addr, const uint32_t *data);

#endif

/**
 * @brief Erase and program a whole page.
 * @param[in] page Page index
 * @param[in] data `FLASH_PAGE_SIZE` bytes
 * @return `OK` programmed, `ERR` beyond the chip
 */
status_t FLASH_WritePage(uint16_t page, const uint8_t *data);

/**
 * @brief Compare a record saved with `FLASH_Save` against a buffer.
 * @param[in] page First page of the record
 * @param[in] data Bytes to compare
 * @param[in] size Byte count
 * @return `true` when the record holds exactly these bytes
 */
bool FLASH_Compare(uint16_t page, const uint8_t *data, uint16_t size);

/**
 * @brief Save a record with its size, over as many pages as it needs.
 *   The size header is written last, so a torn save reads back as no record at all.
 * @param[in] page First page of the record
 * @param[in] data Bytes to save
 * @param[in] size Byte count
 * @return `OK` saved, `ERR` empty, too long or not written
 */
status_t FLASH_Save(uint16_t page, const uint8_t *data, uint16_t size);

/**
 * @brief Load a record saved with `FLASH_Save`.
 * @param[in] page First page of the record
 * @param[out] data Buffer for the record, part of it written when it holds none
 * @return Byte count, `0` without a record
 */
uint16_t FLASH_Load(uint16_t page, uint8_t *data);

/**
 * @brief Folder of the page files of a host application, read from the next access on.
 *   A test keeps the chip in RAM and passes it by.
 * @param[in] path Folder path, created when missing
 */
void FLASH_SetDirectory(const char *path);

//-------------------------------------------------------------------------------------------- Host

// Where a power cut lands in its cycle
typedef enum {
  FLASH_Cut_Before,      // before the cycle, which never starts
  FLASH_Cut_Unreadable,  // halfway: the doublewords the cycle touched fail ECC
  FLASH_Cut_Corrected,   // halfway: ECC corrects them to a wrong value, ~5% of cuts on G071
  FLASH_Cut_Unstable,    // halfway: they read erased and fail ECC by turns, as on WB55
  FLASH_Cut_Late         // late: an erase leaves the page reading erased, a program lands whole
} FLASH_Cut_t;

#if defined(HOST_TEST)

// Whole flash erased and nothing written, a board fresh from the factory
void FLASH_HostBlank(void);

// What a programmer does: the pages under the bytes erased, every byte written, `0xFF` too
void FLASH_HostProgram(uint32_t addr, const uint8_t *data, uint32_t len);

// Bits of the written doubleword at `addr` flipped by age, until its erase:
// ECC corrects one and the doubleword reads as written, two it cannot read
void FLASH_HostAge(uint32_t addr, uint8_t bits);

// Erase and program cycles done since the start, the points a power cut lands at
uint32_t FLASH_HostCycles(void);

/**
 * @brief Run `Run` with the power going after `cycles` erase and program cycles,
 *   where `cut` says. A cycle cut halfway stays half done. The power is back on return.
 * @param[in] cycles Cycles done before the cut
 * @param[in] cut Where the cut lands in the next cycle
 * @param[in] Run Work the cut stops
 */
void FLASH_HostCut(int32_t cycles, FLASH_Cut_t cut, void (*Run)(void));

// The whole chip kept aside, and put back as it was
void FLASH_HostSave(void);
void FLASH_HostRestore(void);

#endif
//-------------------------------------------------------------------------------------------------
#endif

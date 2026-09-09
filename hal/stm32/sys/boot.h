// hal/stm32/sys/boot.h

#ifndef BOOT_H_
#define BOOT_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "flash.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef BOOT_PAGES
  // Pages the bootloader owns at the start of flash, `0` without one. Forge sets it per chip.
  #define BOOT_PAGES 0
#endif

#ifndef BOOT_SLOT_PAGES
  // Pages of the application slot and of the staging slot behind it,
  // `0` when the image runs from the start of flash. Forge sets it from `PRO_BOOT`.
  #define BOOT_SLOT_PAGES 0
#endif

//--------------------------------------------------------------------------------------- Constants

#define BOOT_MAGIC 0x4E45504Fu         // "OPEN", opens every image header
#define BOOT_MAILBOX_MAGIC 0x544F4F42u // "BOOT", opens a mailbox record
#define BOOT_HEADER_OFFSET 0x200u      // header position in an image, past every vector table
#define BOOT_TRAILER_SIZE 4u           // `crc32_iso` of the image, in the erased bytes behind it
#define BOOT_MAILBOX_PAGE (BOOT_PAGES - 1)
#define BOOT_APP_PAGE BOOT_PAGES
#define BOOT_STAGING_PAGE (BOOT_PAGES + BOOT_SLOT_PAGES)
#define BOOT_SLOT_SIZE ((uint32_t)BOOT_SLOT_PAGES * FLASH_PAGE_SIZE)

//------------------------------------------------------------------------------------------- Types

/**
 * @brief Image header, `BOOT_HEADER_OFFSET` bytes into every image. The linker leaves eight
 *   erased bytes behind the `size` bytes; `BOOT_End` writes the trailer there, a programmer
 *   leaves it erased.
 * @param[in] magic `BOOT_MAGIC`
 * @param[in] size Image bytes before the trailer, a linker constant
 */
typedef struct {
  uint32_t magic;
  uint32_t size;
} BOOT_Header_t;

/**
 * @brief Mailbox record on the last bootloader page: an image waits to be installed.
 *   Two doublewords, the one holding `magic` programmed last, so a torn write is no record.
 * @param[in] magic `BOOT_MAILBOX_MAGIC`
 * @param[in] size Image bytes before the trailer
 * @param[in] page First page of the waiting image
 * @param[in] crc Trailer of the waiting image
 */
typedef struct {
  uint32_t magic;
  uint32_t size;
  uint32_t page;
  uint32_t crc;
} BOOT_Mailbox_t;

/**
 * @brief What the running build knows about itself and about a transfer.
 * @param[out] app_addr Application slot address
 * @param[out] slot_size Bytes of one slot [B]
 * @param[out] image_size Running image bytes, `0` when its header is missing
 * @param[out] image_crc Trailer of the running image
 * @param[out] boot Built to run under the bootloader
 * @param[out] active A transfer is open
 */
typedef struct {
  uint32_t app_addr;
  uint32_t slot_size;
  uint32_t image_size;
  uint32_t image_crc;
  bool boot;
  bool active;
} BOOT_Status_t;

//---------------------------------------------------------------------------------------- Transfer

// An image comes in order, offset `0` first, and lands in the staging slot page by page.
// `BOOT_End` verifies it and leaves the mailbox record; the caller resets, the bootloader installs.

/**
 * @brief Open a transfer.
 * @param[in] size Image bytes before the trailer
 * @param[in] crc Trailer, `crc32_iso` of the image
 * @return `OK` when the image fits the slot, `ERR` otherwise or without a bootloader
 */
status_t BOOT_Begin(uint32_t size, uint32_t crc);

/**
 * @brief Take the next bytes of the image.
 * @param[in] offset Position of the bytes, the value `BOOT_Offset` reports
 * @param[in] data Bytes
 * @param[in] len Number of bytes
 * @return `OK` when taken, `ERR` on a gap, an overrun or a flash error, the transfer closes then
 */
status_t BOOT_Write(uint32_t offset, const uint8_t *data, uint16_t len);

/**
 * @brief Close the transfer: flush, verify the staged image, leave the mailbox record.
 * @return `OK` when the record is in place, `ERR` when the image is short, corrupt or unwritable
 */
status_t BOOT_End(void);

// Drop an open transfer
void BOOT_Abort(void);

// Offset the next `BOOT_Write` must carry
uint32_t BOOT_Offset(void);

/**
 * @brief Describe the running build and the transfer state.
 * @param[out] status Filled in
 */
void BOOT_Status(BOOT_Status_t *status);

//------------------------------------------------------------------------------------------- Image

/**
 * @brief Check an image: header in place and a trailer matching the bytes, or still
 *   erased, which is how an image lands over the programmer, verified on the way.
 * @param[in] page First page of the image
 * @param[out] size Image bytes before the trailer, `NULL` to skip
 * @param[out] crc `crc32_iso` of the image, `NULL` to skip
 * @return `true` when the image is whole
 */
bool BOOT_ImageValid(uint16_t page, uint32_t *size, uint32_t *crc);

//-------------------------------------------------------------------------------------- Bootloader

/**
 * @brief Bootloader step: install a waiting image, then judge the application slot.
 *   A record in the mailbox names an image; a whole one is copied into the slot,
 *   verified there and the record erased. Any interruption repeats the step on the next call.
 * @return Application address when the slot holds a whole image, `0` otherwise
 */
uint32_t BOOT_Install(void);

/**
 * @brief Hand the core to an image: its vector table, its stack, its reset handler.
 * @param[in] addr Image address
 */
void BOOT_Jump(uint32_t addr);

//------------------------------------------------------------------------------------------- Shell

/**
 * @brief Shell command `boot`, compiled into `CMD_Step` with `CMD_BOOT`.
 *   `boot info`, `boot begin <size> <crc>`, `boot data <offset> <hex>`, `boot end`, `boot abort`.
 *   Every reply is one `BOOT` line; `boot end` resets once the reply has left.
 * @param[in] argv Tokens
 * @param[in] argc Token count
 */
void BOOT_Bash(char **argv, uint16_t argc);

//-------------------------------------------------------------------------------------------------
#endif

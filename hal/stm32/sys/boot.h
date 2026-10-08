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

#ifndef BOOT_CHIP
  // `DEV_ID` of the chip the image is built for, from the chip table. Forge sets it.
  #define BOOT_CHIP 0
#endif

#ifndef PRO_BOOT_EPOCH
  // Security generation of the image, raised only by a release that closes a hole
  #define PRO_BOOT_EPOCH 0
#endif

#ifndef BOOT_KEY
  // Bootloader that starts only images signed with the key it carries, set by its project
  #define BOOT_KEY OFF
#endif

//--------------------------------------------------------------------------------------- Constants

#define BOOT_MAGIC 0x4E45504Fu          // "OPEN", opens every image header
#define BOOT_MAILBOX_MAGIC 0x544F4F42u  // "BOOT", opens a mailbox record
#define BOOT_REPORT_MAGIC 0x54525052u   // "RPRT", opens the report a bootloader leaves in RAM
#define BOOT_HEADER_OFFSET 0x200u       // header position in an image, past every vector table
#define BOOT_TRAILER_SIZE 72u           // erased bytes behind an image: CRC32, then the signature
#define BOOT_SIGNATURE_OFFSET 8u        // signature in the trailer, a doubleword past the CRC32
#define BOOT_SIGNATURE_SIZE 64u         // Ed25519
#define BOOT_KEY_SIZE 32u               // Ed25519, the last bytes of the bootloader code region
#define BOOT_MAILBOX_PAGE (BOOT_PAGES - 1)
#define BOOT_APP_PAGE BOOT_PAGES
#define BOOT_STAGING_PAGE (BOOT_PAGES + BOOT_SLOT_PAGES)
#define BOOT_SLOT_SIZE ((uint32_t)BOOT_SLOT_PAGES * FLASH_PAGE_SIZE)

//------------------------------------------------------------------------------------------- Types

/**
 * @brief Image header, `BOOT_HEADER_OFFSET` bytes into every image.
 * @details Fields only grow at the end: a bootloader in the field reads `magic` and `size` alone.
 *   The linker leaves `BOOT_TRAILER_SIZE` erased bytes behind the `size` bytes.
 *   Their CRC32 is written by Forge into a flashed image and by `BOOT_End` into a staged one.
 * @param[in] magic `BOOT_MAGIC`
 * @param[in] size Image bytes before the trailer, a linker constant
 * @param[in] origin Address the image is linked to, the start of its slot
 * @param[in] chip `DEV_ID` of the chip the image is built for, `BOOT_CHIP`
 * @param[in] epoch Security generation, `PRO_BOOT_EPOCH`
 */
typedef struct {
  uint32_t magic;
  uint32_t size;
  uint32_t origin;
  uint32_t chip;
  uint32_t epoch;
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
 * @brief What became of an image: no objection, or what turned it down.
 * @details A bootloader reports these to images built on other Core versions,
 *   so a value never changes and a new one goes at the end.
 */
typedef enum {
  BOOT_Result_None      = 0, // no objection; at a start, no image waited
  BOOT_Result_Installed = 1, // waiting image went into the application slot
  BOOT_Result_Crc       = 2, // header or bytes broken, not the image the trailer names
  BOOT_Result_Chip      = 3, // built for another chip
  BOOT_Result_Origin    = 4, // linked for another slot
  BOOT_Result_Signature = 5, // not signed with the key in the bootloader
  BOOT_Result_Epoch     = 6, // older security generation than the image in the slot
} BOOT_Result_t;

/**
 * @brief What a bootloader leaves at every start for the image it starts:
 *   the 8 bytes of RAM below the first one an image owns, `_boot_report` of the linker script.
 * @param[in] magic `BOOT_REPORT_MAGIC` once a bootloader wrote the report
 * @param[in] result `BOOT_Result_t` of the start
 */
typedef struct {
  uint32_t magic;
  uint32_t result;
} BOOT_Report_t;

/**
 * @brief What the running build knows about itself and about a transfer.
 * @param[out] app_addr Application slot address
 * @param[out] slot_size Bytes of one slot
 * @param[out] image_size Running image bytes, `0` when its header is missing
 * @param[out] image_crc Trailer of the running image
 * @param[out] result What the bootloader did at this start, `BOOT_Result_None` without its report
 * @param[out] key Key the bootloader checks signatures with, `NULL` under `plain` or when blank
 * @param[out] rdp Readout protection level: `0` open, `1` closed to the debugger, `2` for good
 * @param[out] boot Built to run under the bootloader
 * @param[out] active A transfer is open
 */
typedef struct {
  uint32_t app_addr;
  uint32_t slot_size;
  uint32_t image_size;
  uint32_t image_crc;
  BOOT_Result_t result;
  const uint8_t *key;
  uint8_t rdp;
  bool boot;
  bool active;
} BOOT_Status_t;

//---------------------------------------------------------------------------------------- Transfer

// An image comes in order, offset `0` first, and lands in the staging slot page by page.
// `BOOT_End` verifies it and leaves the mailbox record;
// the caller resets, the bootloader installs.

/**
 * @brief Open a transfer.
 * @param[in] size Image bytes before the trailer
 * @param[in] crc Trailer, `crc32_iso` of the image
 * @return `OK` when the image fits the slot, `ERR` otherwise or without a bootloader
 */
status_t BOOT_Begin(uint32_t size, uint32_t crc);

/**
 * @brief Give the open transfer its signature, which `BOOT_End` writes into the trailer.
 *   Without one the signature stays erased, and only a `key` bootloader turns that down.
 * @param[in] signature `BOOT_SIGNATURE_SIZE` bytes of Ed25519
 * @return `OK` with a transfer open, `ERR` otherwise
 */
status_t BOOT_Signature(const uint8_t *signature);

/**
 * @brief Take the next bytes of the image.
 * @param[in] offset Position of the bytes, the value `BOOT_Offset` reports
 * @param[in] data Bytes
 * @param[in] len Number of bytes
 * @return `OK` when taken, `ERR` on a gap, an overrun or a flash error, the transfer closes then
 */
status_t BOOT_Write(uint32_t offset, const uint8_t *data, uint16_t len);

/**
 * @brief Close the transfer: flush, judge the staged image, leave the mailbox record.
 * @return `OK` when the record is in place, `ERR` when the image is short, refused or unwritable
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
 * @brief Judge an image: header in place, linked for the application slot of this chip,
 *   and a trailer matching its bytes.
 * @param[in] page First page of the image
 * @param[out] size Image bytes before the trailer, set for a whole image, `NULL` to skip
 * @param[out] crc `crc32_iso` of the image, set for a whole image, `NULL` to skip
 * @return `BOOT_Result_None` for a whole image, otherwise what turns it down
 */
BOOT_Result_t BOOT_ImageCheck(uint16_t page, uint32_t *size, uint32_t *crc);

//-------------------------------------------------------------------------------------- Bootloader

/**
 * @brief Bootloader step: install a waiting image, then judge the application slot.
 *   A record in the mailbox names an image;
 *   a whole one is copied into the slot, verified there and the record erased.
 *   Any interruption repeats the step on the next call.
 *   What the step did goes into `BOOT_Report_t` for the image it hands the core to.
 * @details With `BOOT_KEY` an image also needs a signature by the bootloader key at every start,
 *   and an install takes no image of a lower `epoch` than the one in the slot.
 *   The checks run at 64MHz, and the step returns on `HSI16` with the PLL off:
 *   the reset state of a G0, and one the clock setup of a WB starts from as well as from `MSI`.
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
 *   `boot info`, `boot begin <size> <crc> [<signature>]`, `boot data <offset> <hex>`,
 *   `boot end`, `boot abort`.
 *   Every reply is one `BOOT` line; `boot end` resets once the reply has left.
 * @param[in,out] argv Tokens, hex arguments decoded in place
 * @param[in] argc Token count
 */
void BOOT_Bash(char **argv, uint16_t argc);

//-------------------------------------------------------------------------------------------------
#endif

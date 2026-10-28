// hal/stm32/sys/bld.h

#ifndef BLD_H_
#define BLD_H_

#include <stdbool.h>
#include <stdint.h>

#include "xdef.h"
#include "flash.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef BOOT_PAGES
  // Pages the bootloader owns at the start of flash, its mailbox the last of them,
  // `0` without one. Forge sets it per chip.
  #define BOOT_PAGES 0
#endif

#ifndef BOOT_SLOT_PAGES
  // Pages of the application slot and of the staging slot behind it,
  // `0` when the image runs from the start of flash.
  // Forge sets it when `main.h` includes `boot.h`.
  #define BOOT_SLOT_PAGES 0
#endif

#ifndef BOOT_CHIP
  // `DEV_ID` of the chip the image is built for, from the chip table. Forge sets it.
  #define BOOT_CHIP 0
#endif

// The three settings an image header carries take `0` to `65535`, 16 bits each

#ifndef BOOT_BOARD_VARIANT
  // Hardware versions on the chip that run one binary: an image carries the variant,
  // a bootloader takes only images of its own, so variants get their releases side by side.
  // Another value only for hardware that needs another binary.
  #define BOOT_BOARD_VARIANT 0
#endif

#ifndef PRO_SECURITY_EPOCH
  // Security generation of the image, raised only by a release that closes a hole
  #define PRO_SECURITY_EPOCH 0
#endif

#ifndef PRO_SIGNING_KEY
  // Number of the key the image is signed with, which revokes the keys below it
  #define PRO_SIGNING_KEY 0
#endif

// `BOOT_KEYS` in `boot.h` of the project makes its bootloader the `key` one:
// the public keys of the product as one string, 64 hex digits each, from key `0`.

// `BOOT_HOLD` in `boot.h` of the project is the output that holds the power of the board,
// a `GPIO_t` initializer: `{ .port = GPIOB, .pin = 5, .reverse = true }` holds it at low level.
// The bootloader sets it first after a reset and never releases it, the application finds it set.

// `BOOT_DEV_KEY` comes from Forge to the bootloader of builds alone:
// the development key of this computer stands for every key of `BOOT_KEYS`,
// and the floors stay in the slot, so a board on the desk keeps nothing for good.
#if defined(BOOT_DEV_KEY)
  #undef BOOT_OTP_WORDS
  #define BOOT_OTP_WORDS 0
#endif

#ifndef BOOT_OTP_WORDS
  // OTP doublewords the epoch and key floors of a `key` bootloader go into,
  // `0` takes the floors from the image in the application slot instead, as builds always do.
  #define BOOT_OTP_WORDS 64
#endif

#ifndef BOOT_OTP_ADDR
  // First of those words: the last of OTP, or a spare flash page a test build can erase
  #define BOOT_OTP_ADDR (FLASH_OTP_BASE + FLASH_OTP_SIZE - 8u * BOOT_OTP_WORDS)
#endif

//--------------------------------------------------------------------------------------- Constants

#define BLD_MAGIC 0x4E45504Fu         // "OPEN", opens every image header
#define BLD_REPORT_MAGIC 0x54525052u  // "RPRT", opens the report a bootloader leaves in RAM
#define BLD_MAILBOX_KIND 0x424Du      // "MB", the PDB kind of the mailbox page
#define BLD_HEADER_OFFSET 0x200u      // header position in an image, past every vector table
#define BLD_TRAILER_SIZE 72u          // erased bytes behind an image: CRC32, then the signature
#define BLD_SIGNATURE_OFFSET 8u       // signature in the trailer, a doubleword past the CRC32
#define BLD_SIGNATURE_SIZE 64u        // Ed25519
#define BLD_KEY_SIZE 32u              // Ed25519 public key, 64 hex digits in `BOOT_KEYS`
#define BLD_MAILBOX_PAGE (BOOT_PAGES - 1)
#define BLD_APP_PAGE BOOT_PAGES
#define BLD_STAGING_PAGE (BOOT_PAGES + BOOT_SLOT_PAGES)
#define BLD_SLOT_SIZE ((uint32_t)BOOT_SLOT_PAGES * FLASH_PAGE_SIZE)

//------------------------------------------------------------------------------------------- Types

/**
 * @brief Image header, `BLD_HEADER_OFFSET` bytes into every image.
 * @details Fields only grow at the end.
 *   The linker leaves `BLD_TRAILER_SIZE` erased bytes behind the `size` bytes.
 *   Their CRC32 is written by Forge into a flashed image and by `BLD_End` into a staged one.
 * @param[in] magic `BLD_MAGIC`
 * @param[in] size Image bytes before the trailer, a linker constant
 * @param[in] origin Address the image is linked to, the start of its slot
 * @param[in] chip `DEV_ID` of the chip the image is built for, `BOOT_CHIP`
 * @param[in] board Board variant the image is built for, `BOOT_BOARD_VARIANT`
 * @param[in] epoch Security generation, `PRO_SECURITY_EPOCH`
 * @param[in] key Key the image is signed with, `PRO_SIGNING_KEY`, which revokes the keys below it
 * @param[in] reserved `0`, room for a field to come, which bootloaders of today skip
 */
typedef struct {
  uint32_t magic;
  uint32_t size;
  uint32_t origin;
  uint16_t chip;
  uint16_t board;
  uint16_t epoch;
  uint16_t key;
  uint32_t reserved;
} BLD_Header_t;

/**
 * @brief What became of an image: no objection, or what turned it down.
 * @details Values in the order of the checks, fixed from the first bootloader in production:
 *   a bootloader reports them to images built on other Core versions.
 */
typedef enum {
  BLD_Result_None      = 0,  // no objection: a request waits, or no image waited at a start
  BLD_Result_Installed = 1,  // waiting image went into the application slot
  BLD_Result_Crc       = 2,  // header or bytes broken, not the image the request names
  BLD_Result_Size      = 3,  // larger than the slot
  BLD_Result_Origin    = 4,  // linked for another slot
  BLD_Result_Chip      = 5,  // built for another chip
  BLD_Result_Board     = 6,  // built for another board variant
  BLD_Result_Signature = 7,  // no key of its number, a revoked one, a bad signature
  BLD_Result_Epoch     = 8,  // older security generation than the floor
} BLD_Result_t;

/**
 * @brief Mailbox entry, a PDB record on the last bootloader page.
 *   The application formats the page and writes a request, the bootloader appends its result.
 * @param[in] number Given by PDB
 * @param[in] result `BLD_Result_None` in a request, what became of the image in a result
 * @param[in] page First page of the staging slot, which only the application knows
 * @param[in] crc Trailer of the image the entry is about
 * @param[in] check CRC-32 of the entry, written by PDB
 */
typedef struct {
  uint32_t number;
  uint16_t result;
  uint16_t page;
  uint32_t crc;
  uint32_t check;
} BLD_Entry_t;

/**
 * @brief What a bootloader leaves at every start for the image it starts:
 *   the 16 bytes of RAM below the first one an image owns, `_bld_report` of the linker script.
 * @details The floors come with it, so the image tells what its bootloader keeps,
 *   in OTP or in the slot, without knowing which of the two it runs under.
 * @param[in] magic `BLD_REPORT_MAGIC` once a bootloader wrote the report
 * @param[in] result `BLD_Result_t` of the start
 * @param[in] epoch Epoch floor the bootloader keeps
 * @param[in] key Key floor the bootloader keeps
 * @param[in] reserved `0`, room for a field to come
 */
typedef struct {
  uint32_t magic;
  uint32_t result;
  uint16_t epoch;
  uint16_t key;
  uint32_t reserved;
} BLD_Report_t;

/**
 * @brief What the running build knows about itself, its bootloader and a transfer.
 * @param[out] app_addr Application slot address
 * @param[out] slot_size Bytes of one slot
 * @param[out] image_size Running image bytes, `0` when its header is missing
 * @param[out] image_crc Trailer of the running image
 * @param[out] loader CRC-32 of the bootloader code region, its keys with it: which one it is
 * @param[out] result What the bootloader did at this start, `BLD_Result_None` without its report
 * @param[out] last What became of the last request in the mailbox,
 *   `BLD_Result_None` without one or while it waits
 * @param[out] epoch Epoch floor of the report: no image below it installs or starts
 * @param[out] key Key floor of the report: the keys below it are revoked
 * @param[out] rdp Readout protection level: `0` open, `1` closed to the debugger, `2` for good
 * @param[out] boot Built to run under the bootloader
 * @param[out] active A transfer is open
 */
typedef struct {
  uint32_t app_addr;
  uint32_t slot_size;
  uint32_t image_size;
  uint32_t image_crc;
  uint32_t loader;
  BLD_Result_t result;
  BLD_Result_t last;
  uint16_t epoch;
  uint16_t key;
  uint8_t rdp;
  bool boot;
  bool active;
} BLD_Status_t;

//---------------------------------------------------------------------------------------- Transfer

// An image comes in order, offset `0` first, and lands in the staging slot page by page.
// `BLD_End` seals it and leaves the request in the mailbox; the caller resets,
// and the bootloader alone judges the image and installs it.

/**
 * @brief Open a transfer.
 * @param[in] size Image bytes before the trailer
 * @param[in] crc Trailer, `crc32_iso` of the image
 * @return `OK` when the image runs past its header and fits the slot,
 *   `ERR` otherwise or without a bootloader
 */
status_t BLD_Begin(uint32_t size, uint32_t crc);

/**
 * @brief Give the open transfer its signature, which `BLD_End` writes into the trailer.
 *   Without one the signature stays erased, and only a `key` bootloader turns that down.
 * @param[in] signature `BLD_SIGNATURE_SIZE` bytes of Ed25519
 * @return `OK` with a transfer open, `ERR` otherwise
 */
status_t BLD_Signature(const uint8_t *signature);

/**
 * @brief Take the next bytes of the image.
 * @param[in] offset Position of the bytes, the value `BLD_Offset` reports
 * @param[in] data Bytes
 * @param[in] len Number of bytes
 * @return `OK` when taken, `ERR` on a gap, an overrun or a flash error, the transfer closes then
 */
status_t BLD_Write(uint32_t offset, const uint8_t *data, uint16_t len);

/**
 * @brief Close the transfer: the trailer behind the image, the last page flushed,
 *   the request written into the mailbox, formatted first.
 * @return `OK` when the request is in place, `ERR` when the image is short or unwritable
 */
status_t BLD_End(void);

// Drop an open transfer
void BLD_Abort(void);

// Offset the next `BLD_Write` must carry
uint32_t BLD_Offset(void);

/**
 * @brief Describe the running build, its bootloader and the transfer state.
 * @param[out] status Filled in
 */
void BLD_Status(BLD_Status_t *status);

//------------------------------------------------------------------------------------------- Image

/**
 * @brief Judge an image in the order of the checks: header and size within `pages`,
 *   then its bytes against the trailer, then the slot, chip and board it is built for.
 * @param[in] page First page of the image
 * @param[in] pages Pages the image may take, its trailer included
 * @param[out] size Image bytes before the trailer, set for a whole image, `NULL` to skip
 * @param[out] crc `crc32_iso` of the image, set for a whole image, `NULL` to skip
 * @return `BLD_Result_None` for a whole image, otherwise what turns it down
 */
BLD_Result_t BLD_ImageCheck(uint16_t page, uint16_t pages, uint32_t *size, uint32_t *crc);

//-------------------------------------------------------------------------------------- Bootloader

/**
 * @brief Bootloader step: install a waiting image, then judge the application slot.
 *   The newest mailbox entry being a request, the image in the staging slot it names
 *   is judged; a whole one is copied into the slot and verified there,
 *   the floors raised in OTP and the result appended last.
 *   Any interruption before the result repeats the step on the next call.
 *   An image in the slot above the floors raises them before it starts,
 *   so one a programmer wrote counts as installed.
 *   What the step did goes into `BLD_Report_t`, the floors with it, for the image it starts.
 * @details With `BOOT_KEYS` an image also needs a signature by a key it names, one not revoked,
 *   and an epoch not below the floor, at every start.
 *   The checks run at 64MHz, and the step returns on `HSI16` with the PLL off:
 *   the reset state of a G0, and one the clock setup of a WB starts from as well as from `MSI`.
 * @return Application address when the slot holds a whole image, `0` otherwise
 */
uint32_t BLD_Install(void);

/**
 * @brief Hand the core to an image: its vector table, its stack, its reset handler.
 * @param[in] addr Image address
 */
void BLD_Jump(uint32_t addr);

//------------------------------------------------------------------------------------------- Shell

/**
 * @brief Shell command `boot`, compiled into `CMD_Step` with `CMD_BOOT`.
 *   `boot info`, `boot begin <size> <crc> [<signature>]`, `boot data <offset> <hex>`,
 *   `boot end`, `boot abort`.
 *   Every reply is one `BOOT` line; `boot end` resets once the reply has left.
 * @param[in,out] argv Tokens, hex arguments decoded in place
 * @param[in] argc Token count
 */
void BLD_Bash(char **argv, uint16_t argc);

//-------------------------------------------------------------------------------------------------
#endif

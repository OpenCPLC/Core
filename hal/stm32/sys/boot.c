// hal/stm32/sys/boot.c

#include "boot.h"

#include <string.h>

#if(BOOT_KEY)
  #include "monocypher-ed25519.h"
#endif

#include "crc.h"
#include "pwr.h"

#if defined(STM32)
  #include "cmd.h"
  #include "dbg.h"
  #include "log.h"
  #include "xstring.h"
#endif

//------------------------------------------------------------------------------------------ Config

// Image bytes the signature hash takes between two watchdog refreshes
#define BOOT_HASH_CHUNK ((uint32_t)FLASH_PAGE_SIZE)

#define BOOT_KEY_SHOWN 4u // key bytes `boot info` prints as the fingerprint

//---------------------------------------------------------------------------------------- Internal

// Open transfer
static struct {
  uint32_t size;          // image bytes before the trailer
  uint32_t crc;           // expected trailer
  uint32_t offset;        // bytes taken so far, the trailer included at the end
  BOOT_Result_t verdict;  // what turned the image down at `BOOT_End`
  // Erased unless `BOOT_Signature` gave one
  uint8_t signature[BOOT_SIGNATURE_SIZE];
  bool active;
} boot;

// Report of this start, placed by the linker script in the RAM below what an image owns
extern BOOT_Report_t _boot_report;

// Key of a `key` bootloader: the last bytes of its code region, in front of the mailbox page
static inline const uint8_t *bootloader_key(void) {
  return FLASH_Ref(FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0) - BOOT_KEY_SIZE);
}

static inline const BOOT_Header_t *header_of(uint16_t page) {
  return FLASH_Ref(FLASH_GetAddress(page, 0) + BOOT_HEADER_OFFSET);
}

static inline bool erased(const uint8_t *data, uint32_t len)
{
  while(len--) {
    if(*data++ != 0xFF) return false;
  }
  return true;
}

// One page assembled before it is programmed; also the copy buffer of the bootloader.
// `FLASH_WritePage` reads it as words and from RAM while a row programs.
static uint8_t page_buffer[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

// Program from `page_buffer` the page that ends at `end` [bytes into the image]
static status_t flush_page(uint32_t end)
{
  uint16_t page = (uint16_t)(BOOT_STAGING_PAGE + (end - 1u) / FLASH_PAGE_SIZE);
  return FLASH_WritePage(page, page_buffer);
}

// Mailbox record. The doubleword without the magic goes first, the one with it commits.
static status_t mailbox_write(uint32_t page, uint32_t size, uint32_t crc)
{
  uint32_t addr = FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0);
  if(FLASH_Erase(BOOT_MAILBOX_PAGE)) return ERR;
  if(FLASH_Write(addr + 8u, page, crc)) return ERR;
  return FLASH_Write(addr, BOOT_MAILBOX_MAGIC, size);
}

// Bytes behind the ones taken so far, a page programmed as soon as it fills
static status_t stage(const uint8_t *data, uint32_t len)
{
  while(len) {
    uint32_t pos = boot.offset % FLASH_PAGE_SIZE;
    uint32_t take = minv(FLASH_PAGE_SIZE - pos, len);
    if(!pos) memset(page_buffer, 0xFF, FLASH_PAGE_SIZE);
    memcpy(page_buffer + pos, data, take);
    boot.offset += take;
    data += take;
    len -= take;
    if(pos + take == FLASH_PAGE_SIZE && flush_page(boot.offset)) return ERR;
  }
  return OK;
}

// `DEV_ID` of the chip running this code; the host plays the chip its build names
static uint32_t chip_id(void)
{
  #if defined(STM32G0)
  // `DBG` sits behind the `DBGEN` clock gate, off after reset
  bool gated = !(RCC->APBENR1 & RCC_APBENR1_DBGEN);
  RCC->APBENR1 |= RCC_APBENR1_DBGEN;
  uint32_t id = DBG->IDCODE & DBG_IDCODE_DEV_ID;
  if(gated) RCC->APBENR1 &= ~RCC_APBENR1_DBGEN;
  return id;
  #elif defined(STM32WB)
  return DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID;
  #else
  return BOOT_CHIP;
  #endif
}

//---------------------------------------------------------------------------------------- Transfer

status_t BOOT_Begin(uint32_t size, uint32_t crc)
{
  boot.active = false;
  if(!BOOT_SLOT_PAGES) return ERR;
  if(size <= BOOT_HEADER_OFFSET + sizeof(BOOT_Header_t)) return ERR;
  if(size > BOOT_SLOT_SIZE - BOOT_TRAILER_SIZE) return ERR;
  boot.size = size;
  boot.crc = crc;
  boot.offset = 0;
  memset(boot.signature, 0xFF, sizeof(boot.signature));
  boot.active = true;
  return OK;
}

status_t BOOT_Signature(const uint8_t *signature)
{
  if(!boot.active) return ERR;
  memcpy(boot.signature, signature, sizeof(boot.signature));
  return OK;
}

status_t BOOT_Write(uint32_t offset, const uint8_t *data, uint16_t len)
{
  if(!boot.active) return ERR;
  if(offset != boot.offset || len > boot.size - boot.offset || stage(data, len)) {
    boot.active = false;
    return ERR;
  }
  return OK;
}

status_t BOOT_End(void)
{
  if(!boot.active) return ERR;
  boot.active = false;
  boot.verdict = BOOT_Result_None;
  if(boot.offset != boot.size) return ERR;
  // CRC32 first, then the signature, erased unless the transfer carried one
  uint8_t trailer[BOOT_TRAILER_SIZE];
  memset(trailer, 0xFF, sizeof(trailer));
  memcpy(trailer, &boot.crc, sizeof(boot.crc));
  memcpy(trailer + BOOT_SIGNATURE_OFFSET, boot.signature, sizeof(boot.signature));
  if(stage(trailer, sizeof(trailer))) return ERR;
  if(boot.offset % FLASH_PAGE_SIZE && flush_page(boot.offset)) return ERR;
  uint32_t size, crc;
  boot.verdict = BOOT_ImageCheck(BOOT_STAGING_PAGE, &size, &crc);
  if(!boot.verdict && (size != boot.size || crc != boot.crc)) boot.verdict = BOOT_Result_Crc;
  if(boot.verdict) return ERR;
  return mailbox_write(BOOT_STAGING_PAGE, boot.size, boot.crc);
}

void BOOT_Abort(void)
{
  boot.active = false;
}

uint32_t BOOT_Offset(void)
{
  return boot.offset;
}

//------------------------------------------------------------------------------------------- Image

BOOT_Result_t BOOT_ImageCheck(uint16_t page, uint32_t *size, uint32_t *crc)
{
  if(page >= FLASH_PAGES) return BOOT_Result_Crc;
  uint32_t base = FLASH_GetAddress(page, 0);
  uint32_t room = (uint32_t)(FLASH_PAGES - page) * FLASH_PAGE_SIZE;
  const BOOT_Header_t *header = FLASH_Ref(base + BOOT_HEADER_OFFSET);
  if(header->magic != BOOT_MAGIC) return BOOT_Result_Crc;
  // An erased size is `0xFFFFFFFF`, so the bound is tested without adding to it;
  // the trailer is read as a word, so the size has to keep it aligned.
  if(header->size <= BOOT_HEADER_OFFSET + sizeof(BOOT_Header_t)) return BOOT_Result_Crc;
  if(header->size > room - BOOT_TRAILER_SIZE || header->size % 4u) return BOOT_Result_Crc;
  if(header->origin != FLASH_GetAddress(BOOT_APP_PAGE, 0)) return BOOT_Result_Origin;
  if(header->chip != chip_id()) return BOOT_Result_Chip;
  uint32_t code = CRC_Run(&crc32_iso, FLASH_Ref(base), header->size);
  if(FLASH_Read(base + header->size) != code) return BOOT_Result_Crc;
  if(size) *size = header->size;
  if(crc) *crc = code;
  return BOOT_Result_None;
}

//-------------------------------------------------------------------------------------- Bootloader
#if(BOOT_PAGES)

// Copy `bytes` from the page `src` into the application slot, page by page
static status_t install(uint16_t src, uint32_t bytes)
{
  uint16_t pages = (uint16_t)((bytes + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE);
  if(BOOT_APP_PAGE + pages > src) return ERR; // the copy would run into its source
  // The header page is erased first and written last:
  // a copy cut by a power loss never leaves a whole header, old or new,
  // over a slot that is half copied.
  if(FLASH_Erase(BOOT_APP_PAGE)) return ERR;
  for(uint16_t i = pages; i-- > 0;) {
    IWDG_Refresh();
    memcpy(page_buffer, FLASH_Ref(FLASH_GetAddress(src + i, 0)), FLASH_PAGE_SIZE);
    if(FLASH_WritePage(BOOT_APP_PAGE + i, page_buffer)) return ERR;
  }
  return OK;
}

#if(BOOT_KEY)

// Image signed with the bootloader key; an erased key signs nothing, so nothing starts
static bool signed_by_key(uint32_t base, uint32_t size)
{
  const uint8_t *key = bootloader_key();
  const uint8_t *signature = FLASH_Ref(base + size + BOOT_SIGNATURE_OFFSET);
  if(erased(key, BOOT_KEY_SIZE)) return false;
  // Ed25519 of RFC 8032: SHA-512 over `R`, the key and the image, reduced to the scalar `h`
  crypto_sha512_ctx ctx;
  crypto_sha512_init(&ctx);
  crypto_sha512_update(&ctx, signature, BOOT_SIGNATURE_SIZE / 2);
  crypto_sha512_update(&ctx, key, BOOT_KEY_SIZE);
  for(uint32_t done = 0; done < size; done += BOOT_HASH_CHUNK) {
    IWDG_Refresh();
    crypto_sha512_update(&ctx, FLASH_Ref(base + done), minv(size - done, BOOT_HASH_CHUNK));
  }
  uint8_t hash[64], h[32];
  crypto_sha512_final(&ctx, hash);
  crypto_eddsa_reduce(h, hash);
  return !crypto_eddsa_check_equation(signature, key, h);
}

#endif

// Verdict of the bootloader: `BOOT_ImageCheck`, and with `BOOT_KEY` the signature on top
static BOOT_Result_t boot_check(uint16_t page, uint32_t *size, uint32_t *crc)
{
  uint32_t bytes = 0;
  BOOT_Result_t result = BOOT_ImageCheck(page, &bytes, crc);
  #if(BOOT_KEY)
  if(!result && !signed_by_key(FLASH_GetAddress(page, 0), bytes)) result = BOOT_Result_Signature;
  #endif
  if(size) *size = bytes;
  return result;
}

// Verdict on the image a record names, which has to be whole and the one recorded
static BOOT_Result_t waiting_check(const BOOT_Mailbox_t *box)
{
  if(box->page <= BOOT_MAILBOX_PAGE || box->page >= FLASH_PAGES) return BOOT_Result_Crc;
  uint32_t size, crc;
  BOOT_Result_t result = boot_check((uint16_t)box->page, &size, &crc);
  if(result) return result;
  if(size != box->size || crc != box->crc) return BOOT_Result_Crc;
  #if(BOOT_KEY)
  // The image in the slot sets the floor; an empty or broken slot takes any generation
  bool slot_whole = !boot_check(BOOT_APP_PAGE, NULL, NULL);
  if(slot_whole && header_of((uint16_t)box->page)->epoch < header_of(BOOT_APP_PAGE)->epoch) {
    return BOOT_Result_Epoch;
  }
  #endif
  return BOOT_Result_None;
}

// Slot holds the image the record names, copied before a power loss cut the step short
static bool slot_holds(const BOOT_Mailbox_t *box)
{
  uint32_t size, crc;
  return !BOOT_ImageCheck(BOOT_APP_PAGE, &size, &crc) && size == box->size && crc == box->crc;
}

uint32_t BOOT_Install(void)
{
  #if(BOOT_KEY && defined(STM32))
  RCC_BootClock(); // a signature takes seconds at the reset clock
  #endif
  BOOT_Result_t result = BOOT_Result_None;
  const BOOT_Mailbox_t *box = FLASH_Ref(FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0));
  if(box->magic == BOOT_MAILBOX_MAGIC) {
    result = waiting_check(box);
    // The record outlives the copy: a power loss in between repeats it from the start.
    // A slot already holding the image is done, a refused image takes its record along.
    bool installed = slot_holds(box);
    if(!result && !installed) {
      installed = !install((uint16_t)box->page, box->size + BOOT_TRAILER_SIZE) &&
        slot_holds(box);
    }
    if(installed) result = BOOT_Result_Installed;
    if(result) FLASH_Erase(BOOT_MAILBOX_PAGE);
  }
  _boot_report = (BOOT_Report_t){ BOOT_REPORT_MAGIC, result };
  uint32_t app = boot_check(BOOT_APP_PAGE, NULL, NULL) ? 0 : FLASH_GetAddress(BOOT_APP_PAGE, 0);
  #if(BOOT_KEY && defined(STM32))
  RCC_ResetClock(); // the image starts as after a reset, whichever bootloader it runs under
  #endif
  return app;
}

#else

uint32_t BOOT_Install(void)
{
  return 0;
}

#endif
//-------------------------------------------------------------------------------------------------
// On the chip alone: the image header, the jump and the shell; the rest builds for a host too
#if defined(STM32)

// Header of this image, placed at `BOOT_HEADER_OFFSET` by the linker script,
// which also measures `size` and leaves the erased trailer bytes behind the image.
extern uint8_t _image_size[];
__attribute__((section(".app_header"), used))
const BOOT_Header_t BootHeader = {
  .magic = BOOT_MAGIC,
  .size = (uint32_t)_image_size,
  .origin = (uint32_t)&BootHeader - BOOT_HEADER_OFFSET,
  .chip = BOOT_CHIP,
  .epoch = PRO_BOOT_EPOCH,
};

#if(BOOT_KEY)
// Key the images have to be signed with, erased until Forge writes it into the flash image.
// It is read back through its address alone, as the compiler would fold these erased bytes.
__attribute__((section(".boot_key"), used))
const uint8_t BootKey[BOOT_KEY_SIZE] = { [0 ... BOOT_KEY_SIZE - 1] = 0xFF };
#endif

void BOOT_Status(BOOT_Status_t *status)
{
  memset(status, 0, sizeof(*status));
  status->app_addr = FLASH_GetAddress(BOOT_APP_PAGE, 0);
  status->slot_size = BOOT_SLOT_SIZE;
  status->boot = BOOT_SLOT_PAGES > 0;
  status->active = boot.active;
  status->image_size = BootHeader.size;
  status->image_crc = FLASH_Read(BootHeader.origin + BootHeader.size);
  // Magic tells a report of this start from RAM no bootloader wrote
  if(status->boot && _boot_report.magic == BOOT_REPORT_MAGIC) {
    status->result = _boot_report.result;
  }
  // Only under the `key` bootloader does a key sit there: under `plain` it is bootloader code,
  // or what a `key` bootloader flashed before left behind, as a programmer writes no more pages.
  #if defined(PRO_BOOT_KEY)
  if(status->boot && !erased(bootloader_key(), BOOT_KEY_SIZE)) status->key = bootloader_key();
  #endif
  // `RDP` byte: `0xAA` open, `0xCC` locked for good, any other value the debugger shut out
  uint32_t rdp = FLASH->OPTR & FLASH_OPTR_RDP;
  status->rdp = rdp == 0xAAu ? 0 : (rdp == 0xCCu ? 2 : 1);
}

void BOOT_Jump(uint32_t addr)
{
  const uint32_t *vector = (const uint32_t *)addr;
  uint32_t sp = vector[0];
  uint32_t pc = vector[1];
  SCB->VTOR = addr;
  // Stack and jump in one piece: nothing may touch the old stack once `msp` moved
  __asm volatile("msr msp, %0\n\tbx %1" : : "r"(sp), "r"(pc) : "memory");
  while(1);
}

//------------------------------------------------------------------------------------------- Shell

// Bootloader the image is built for: one under `plain` does not start under `key`, and back
#if defined(PRO_BOOT_KEY)
static const char MODE[] = "key";
#else
static const char MODE[] = "plain";
#endif

// Result names `boot info` and `boot end` print, in the order of `BOOT_Result_t`
static const char *const RESULT_NAMES[] = {
  "none", "installed", "crc", "chip", "origin", "signature", "epoch",
};

// Name of a result, `unknown` for a value only a newer bootloader knows
static const char *result_name(uint32_t result) {
  return result < array_len(RESULT_NAMES) ? RESULT_NAMES[result] : "unknown";
}

// Fingerprint `boot info` prints: the first key bytes in hex, `none` under `plain`,
// `missing` in a build for the `key` bootloader whose key slot is blank.
static const char *key_name(const uint8_t *key, char hex[2 * BOOT_KEY_SHOWN + 1])
{
  #if defined(PRO_BOOT_KEY)
  if(!key) return "missing";
  #else
  if(!key) return "none";
  #endif
  static const char DIGITS[] = "0123456789abcdef";
  for(uint8_t i = 0; i < BOOT_KEY_SHOWN; i++) {
    hex[2 * i] = DIGITS[key[i] >> 4];
    hex[2 * i + 1] = DIGITS[key[i] & 0x0F];
  }
  hex[2 * BOOT_KEY_SHOWN] = '\0';
  return hex;
}

// Hex text decoded in place, `-1` on a stray character or an odd length
static int32_t hex_decode_this(char *str)
{
  int32_t len = 0;
  uint8_t *out = (uint8_t *)str;
  for(; str[0] && str[1]; str += 2) {
    uint8_t byte = 0;
    for(uint8_t i = 0; i < 2; i++) {
      char c = str[i];
      byte <<= 4;
      if(in_range(c, '0', '9')) byte |= (uint8_t)(c - '0');
      else if(in_range(c, 'a', 'f')) byte |= (uint8_t)(c - 'a' + 10);
      else if(in_range(c, 'A', 'F')) byte |= (uint8_t)(c - 'A' + 10);
      else return -1;
    }
    out[len++] = byte;
  }
  return *str ? -1 : len;
}

void BOOT_Bash(char **argv, uint16_t argc)
{
  CMD_Argc(2, 5);
  switch(hash_djb2_ci(argv[1])) {
    case HASH_Info: {
      CMD_Argc(2);
      BOOT_Status_t status;
      BOOT_Status(&status);
      char hex[2 * BOOT_KEY_SHOWN + 1];
      LOG_Bash("BOOT info boot:%u app:%08x slot:%u page:%u line:%u "
        "image:%u crc:%08x active:%u mode:%s result:%s key:%s rdp:%u",
        status.boot, status.app_addr, status.slot_size, (uint32_t)FLASH_PAGE_SIZE,
        (uint32_t)DBG_RX_SIZE - 1u, status.image_size, status.image_crc, status.active,
        MODE, result_name(status.result), key_name(status.key, hex), status.rdp);
      return;
    }
    case HASH_Begin: {
      CMD_Argc(4, 5);
      if(!str_is_u32(argv[2])) CMD_ArgvExit(2);
      if(!str_is_u32(argv[3])) CMD_ArgvExit(3);
      // Decoded first, so a malformed signature leaves no transfer open
      if(argc == 5 && hex_decode_this(argv[4]) != (int32_t)BOOT_SIGNATURE_SIZE) CMD_ArgvExit(4);
      uint32_t size = str_to_int(argv[2]);
      uint32_t crc = str_to_int(argv[3]);
      if(BOOT_Begin(size, crc)) {
        LOG_Error("BOOT image of " ANSI_LIME "%u" ANSI_END " bytes does not fit", size);
        return;
      }
      if(argc == 5) BOOT_Signature((const uint8_t *)argv[4]);
      LOG_Bash("BOOT begin size:%u crc:%08x", size, crc);
      return;
    }
    case HASH_Data: {
      CMD_Argc(4);
      if(!str_is_u32(argv[2])) CMD_ArgvExit(2);
      int32_t len = hex_decode_this(argv[3]);
      if(len < 0) CMD_ArgvExit(3);
      uint32_t offset = str_to_int(argv[2]);
      if(BOOT_Write(offset, (const uint8_t *)argv[3], (uint16_t)len)) {
        LOG_Error("BOOT data at " ANSI_LIME "%u" ANSI_END " refused, expected "
          ANSI_LIME "%u" ANSI_END, offset, BOOT_Offset());
        return;
      }
      LOG_Bash("BOOT data offset:%u", BOOT_Offset());
      return;
    }
    case HASH_End: {
      CMD_Argc(2);
      bool complete = boot.active && boot.offset == boot.size;
      if(BOOT_End()) {
        if(!complete) LOG_Error("BOOT image incomplete");
        else if(boot.verdict) LOG_Error("BOOT image refused: %s", result_name(boot.verdict));
        else LOG_Error("BOOT image not staged, a flash write failed");
        return;
      }
      LOG_Bash("BOOT end crc:%08x reset", boot.crc);
      DbgReset = true;
      return;
    }
    case HASH_Abort:
      CMD_Argc(2);
      BOOT_Abort();
      LOG_Bash("BOOT abort");
      return;
    default:
      CMD_ArgvExit(1);
  }
}

#endif
//-------------------------------------------------------------------------------------------------

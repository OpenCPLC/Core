// hal/stm32/sys/boot.c

#include "boot.h"

#include <string.h>

#include "crc.h"
#include "pwr.h"
#include "cmd.h"
#include "dbg.h"
#include "log.h"
#include "xstring.h"

//---------------------------------------------------------------------------------------- Internal

// Open transfer
static struct {
  uint32_t size;   // image bytes before the trailer
  uint32_t crc;    // expected trailer
  uint32_t offset; // bytes taken so far, the trailer included at the end
  bool active;
} boot;

// One page assembled before it is programmed; also the copy buffer of the bootloader.
// `FLASH_WritePage` reads it as words and from RAM while a row programs.
static uint8_t page_buffer[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

// Header of this image, placed at `BOOT_HEADER_OFFSET` by the linker script, which also
// measures `size` and leaves the erased trailer bytes behind the image.
extern uint8_t _image_size[];
__attribute__((section(".app_header"), used))
const BOOT_Header_t BootHeader = { .magic = BOOT_MAGIC, .size = (uint32_t)_image_size };

// Program the page that ends at `end` [bytes into the image] from `page_buffer`
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
  boot.active = true;
  return OK;
}

status_t BOOT_Write(uint32_t offset, const uint8_t *data, uint16_t len)
{
  if(!boot.active) return ERR;
  if(offset != boot.offset || len > boot.size - boot.offset) {
    boot.active = false;
    return ERR;
  }
  while(len) {
    uint32_t pos = boot.offset % FLASH_PAGE_SIZE;
    uint32_t take = FLASH_PAGE_SIZE - pos;
    if(take > len) take = len;
    if(!pos) memset(page_buffer, 0xFF, FLASH_PAGE_SIZE);
    memcpy(page_buffer + pos, data, take);
    boot.offset += take;
    data += take;
    len -= (uint16_t)take;
    if(pos + take == FLASH_PAGE_SIZE && flush_page(boot.offset)) {
      boot.active = false;
      return ERR;
    }
  }
  return OK;
}

status_t BOOT_End(void)
{
  if(!boot.active) return ERR;
  boot.active = false;
  if(boot.offset != boot.size) return ERR;
  uint32_t pos = boot.offset % FLASH_PAGE_SIZE;
  if(!pos) memset(page_buffer, 0xFF, FLASH_PAGE_SIZE);
  memcpy(page_buffer + pos, &boot.crc, BOOT_TRAILER_SIZE);
  boot.offset += BOOT_TRAILER_SIZE;
  if(flush_page(boot.offset)) return ERR;
  uint32_t size, crc;
  if(!BOOT_ImageValid(BOOT_STAGING_PAGE, &size, &crc)) return ERR;
  if(size != boot.size || crc != boot.crc) return ERR;
  return mailbox_write(BOOT_STAGING_PAGE, boot.size, boot.crc);
}

void BOOT_Abort(void) { boot.active = false; }

uint32_t BOOT_Offset(void) { return boot.offset; }

void BOOT_Status(BOOT_Status_t *status)
{
  memset(status, 0, sizeof(*status));
  status->app_addr = FLASH_GetAddress(BOOT_APP_PAGE, 0);
  status->slot_size = BOOT_SLOT_SIZE;
  status->boot = BOOT_SLOT_PAGES > 0;
  status->active = boot.active;
  uint32_t base = (uint32_t)&BootHeader - BOOT_HEADER_OFFSET;
  status->image_size = BootHeader.size;
  status->image_crc = FLASH_Read(base + BootHeader.size);
}

//------------------------------------------------------------------------------------------- Image

bool BOOT_ImageValid(uint16_t page, uint32_t *size, uint32_t *crc)
{
  if(page >= FLASH_PAGES) return false;
  uint32_t base = FLASH_GetAddress(page, 0);
  uint32_t room = (uint32_t)(FLASH_PAGES - page) * FLASH_PAGE_SIZE;
  const BOOT_Header_t *header = FLASH_Ref(base + BOOT_HEADER_OFFSET);
  if(header->magic != BOOT_MAGIC) return false;
  // An erased size is `0xFFFFFFFF`, so the bound is tested without adding to it;
  // the trailer is read as a word, so the size has to keep it aligned.
  if(header->size <= BOOT_HEADER_OFFSET + sizeof(BOOT_Header_t)) return false;
  if(header->size > room - BOOT_TRAILER_SIZE || header->size % 4u) return false;
  uint32_t code = CRC_Run(&crc32_iso, FLASH_Ref(base), header->size);
  uint32_t trailer = FLASH_Read(base + header->size);
  if(trailer != 0xFFFFFFFFu && trailer != code) return false;
  if(size) *size = header->size;
  if(crc) *crc = code;
  return true;
}

//-------------------------------------------------------------------------------------- Bootloader

#if(BOOT_PAGES)

// Copy `bytes` from the page `src` into the application slot, page by page
static status_t install(uint16_t src, uint32_t bytes)
{
  uint16_t pages = (uint16_t)((bytes + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE);
  if(BOOT_APP_PAGE + pages > src) return ERR; // the copy would run into its source
  // The header page is erased first and written last: a copy cut by a power loss
  // never leaves a whole header, old or new, over a slot that is half copied.
  if(FLASH_Erase(BOOT_APP_PAGE)) return ERR;
  for(uint16_t i = pages; i-- > 0;) {
    IWDG_Refresh();
    memcpy(page_buffer, FLASH_Ref(FLASH_GetAddress(src + i, 0)), FLASH_PAGE_SIZE);
    if(FLASH_WritePage(BOOT_APP_PAGE + i, page_buffer)) return ERR;
  }
  return OK;
}

uint32_t BOOT_Install(void)
{
  const BOOT_Mailbox_t *box = FLASH_Ref(FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0));
  if(box->magic == BOOT_MAILBOX_MAGIC) {
    uint32_t size, crc;
    bool waiting = box->page > BOOT_MAILBOX_PAGE && box->page < FLASH_PAGES &&
      BOOT_ImageValid((uint16_t)box->page, &size, &crc) &&
      size == box->size && crc == box->crc;
    // The record outlives the copy: a power loss in between repeats it from the start.
    // A slot already holding the image is done, an unusable record is dropped.
    bool installed = BOOT_ImageValid(BOOT_APP_PAGE, &size, &crc) &&
      size == box->size && crc == box->crc;
    if(waiting && !installed) {
      installed = install((uint16_t)box->page, box->size + BOOT_TRAILER_SIZE) == OK &&
        BOOT_ImageValid(BOOT_APP_PAGE, &size, &crc) && crc == box->crc;
    }
    if(installed || !waiting) FLASH_Erase(BOOT_MAILBOX_PAGE);
  }
  return BOOT_ImageValid(BOOT_APP_PAGE, NULL, NULL) ? FLASH_GetAddress(BOOT_APP_PAGE, 0) : 0;
}

#else

uint32_t BOOT_Install(void) { return 0; }

#endif

void BOOT_Jump(uint32_t addr)
{
  const uint32_t *vector = (const uint32_t *)addr;
  uint32_t sp = vector[0];
  uint32_t pc = vector[1];
  SCB->VTOR = addr;
  // Stack and jump in one piece: nothing may touch the old stack once `msp` moved.
  __asm volatile("msr msp, %0\n\tbx %1" : : "r"(sp), "r"(pc) : "memory");
  while(1);
}

//------------------------------------------------------------------------------------------- Shell

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
      if(c >= '0' && c <= '9') byte |= (uint8_t)(c - '0');
      else if(c >= 'a' && c <= 'f') byte |= (uint8_t)(c - 'a' + 10);
      else if(c >= 'A' && c <= 'F') byte |= (uint8_t)(c - 'A' + 10);
      else return -1;
    }
    out[len++] = byte;
  }
  return *str ? -1 : len;
}

void BOOT_Bash(char **argv, uint16_t argc)
{
  CMD_Argc(2, 4);
  switch(hash_djb2_ci(argv[1])) {
    case HASH_Info: {
      CMD_Argc(2);
      BOOT_Status_t status;
      BOOT_Status(&status);
      LOG_Bash("BOOT info boot:%u app:%08x slot:%u page:%u line:%u image:%u crc:%08x active:%u",
        status.boot, status.app_addr, status.slot_size, (uint32_t)FLASH_PAGE_SIZE,
        (uint32_t)DBG_RX_SIZE - 1u, status.image_size, status.image_crc, status.active);
      return;
    }
    case HASH_Begin: {
      CMD_Argc(4);
      if(!str_is_u32(argv[2])) CMD_ArgvExit(2);
      if(!str_is_u32(argv[3])) CMD_ArgvExit(3);
      uint32_t size = str_to_int(argv[2]);
      uint32_t crc = str_to_int(argv[3]);
      if(BOOT_Begin(size, crc)) {
        LOG_Error("BOOT image of " ANSI_LIME "%u" ANSI_END " bytes does not fit", size);
        return;
      }
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
      if(BOOT_End()) {
        LOG_Error("BOOT image incomplete or corrupt");
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

//-------------------------------------------------------------------------------------------------

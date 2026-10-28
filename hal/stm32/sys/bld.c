// hal/stm32/sys/bld.c

#include "bld.h"

#include <stddef.h>
#include <string.h>

#if defined(BOOT_KEYS)
  #include "monocypher-ed25519.h"
#endif

#include "cmd.h"
#include "crc.h"
#include "dbg.h"
#include "log.h"
#include "pdb.h"
#include "pwr.h"
#include "xstring.h"

//------------------------------------------------------------------------------------------ Config

// Image bytes the signature hash takes between two watchdog refreshes
#define BLD_HASH_CHUNK ((uint32_t)FLASH_PAGE_SIZE)

//---------------------------------------------------------------------------------------- Internal

// Open transfer
static struct {
  uint32_t size;    // image bytes before the trailer
  uint32_t crc;     // expected trailer
  uint32_t offset;  // bytes taken so far, the trailer included at the end
  // Erased unless `BLD_Signature` gave one
  uint8_t signature[BLD_SIGNATURE_SIZE];
  bool active;
} boot;

// Report of this start, placed by the linker script in the RAM below what an image owns
extern BLD_Report_t _bld_report;

// Floors of a `key` bootloader: no image below `epoch` starts, the keys below `key` are revoked
typedef struct {
  uint16_t epoch;
  uint16_t key;
} floors_t;

static inline bool erased(const uint8_t *data, uint32_t len)
{
  while(len--) {
    if(*data++ != 0xFF) return false;
  }
  return true;
}

// Value of hex digit `c`, `-1` for any other character
static inline int8_t hex_digit(char c)
{
  if(in_range(c, '0', '9')) return (int8_t)(c - '0');
  if(in_range(c, 'a', 'f')) return (int8_t)(c - 'a' + 10);
  if(in_range(c, 'A', 'F')) return (int8_t)(c - 'A' + 10);
  return -1;
}

// Byte of the two hex digits at `hex`, `-1` when either is another character
static inline int16_t hex_byte(const char *hex)
{
  int8_t high = hex_digit(hex[0]), low = hex_digit(hex[1]);
  return high < 0 || low < 0 ? -1 : (int16_t)(high << 4 | low);
}

// Header of the image on `page`, copied through ECC
static status_t header_copy(uint16_t page, BLD_Header_t *header)
{
  uint32_t addr = FLASH_GetAddress(page, 0) + BLD_HEADER_OFFSET;
  return FLASH_Copy(addr, header, sizeof(*header), NULL);
}

// One page assembled before it is programmed; also the copy buffer of the bootloader.
// `FLASH_WritePage` reads it as words and from RAM while a row programs.
static uint8_t page_buffer[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

// Program from `page_buffer` the page that ends at `end` [bytes into the image]
static status_t flush_page(uint32_t end)
{
  uint16_t page = (uint16_t)(BLD_STAGING_PAGE + (end - 1u) / FLASH_PAGE_SIZE);
  return FLASH_WritePage(page, page_buffer);
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
static uint16_t chip_id(void)
{
  #if defined(STM32G0)
  // `DBG` sits behind the `DBGEN` clock gate, off after reset
  bool gated = !(RCC->APBENR1 & RCC_APBENR1_DBGEN);
  RCC->APBENR1 |= RCC_APBENR1_DBGEN;
  uint32_t id = DBG->IDCODE & DBG_IDCODE_DEV_ID;
  if(gated) RCC->APBENR1 &= ~RCC_APBENR1_DBGEN;
  return (uint16_t)id;
  #elif defined(STM32WB)
  return (uint16_t)(DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID);
  #else
  return BOOT_CHIP;
  #endif
}

//------------------------------------------------------------------------------------------- Image

_Static_assert(BOOT_BOARD_VARIANT <= 0xFFFFu && PRO_SECURITY_EPOCH <= 0xFFFFu &&
  PRO_SIGNING_KEY <= 0xFFFFu, "board variant, security epoch and signing key take 16 bits each");

// Header in place, of an image that takes `room` bytes at most with its trailer.
// An erased size is `0xFFFFFFFF`, so the bound is tested without adding to it;
// the trailer is read as a word, so the size has to keep it aligned.
static BLD_Result_t header_whole(const BLD_Header_t *header, uint32_t room)
{
  if(header->magic != BLD_MAGIC) return BLD_Result_Crc;
  if(header->size <= BLD_HEADER_OFFSET + sizeof(BLD_Header_t) || header->size % 4u) {
    return BLD_Result_Crc;
  }
  return header->size > room - BLD_TRAILER_SIZE ? BLD_Result_Size : BLD_Result_None;
}

// Header of an image for the application slot of this chip and this board
static BLD_Result_t header_fits(const BLD_Header_t *header)
{
  if(header->origin != FLASH_GetAddress(BLD_APP_PAGE, 0)) return BLD_Result_Origin;
  if(header->chip != chip_id()) return BLD_Result_Chip;
  if(header->board != BOOT_BOARD_VARIANT) return BLD_Result_Board;
  return BLD_Result_None;
}

// Image on `page`, `pages` long at most: its header, its bytes against the trailer
// and against `named`, the CRC32 of a request if any, then the slot, chip and board,
// so a broken image, or another than the request names, never reads as foreign.
// An image cut mid write holds doublewords ECC cannot read: no image.
// So every byte comes through ECC, a plain read of one could raise NMI.
static BLD_Result_t image_check(uint16_t page, uint16_t pages, const uint32_t *named,
  BLD_Header_t *header, uint32_t *crc)
{
  if(!pages || page >= FLASH_PAGES || pages > FLASH_PAGES - page) return BLD_Result_Crc;
  if(header_copy(page, header)) return BLD_Result_Crc;
  BLD_Result_t result = header_whole(header, (uint32_t)pages * FLASH_PAGE_SIZE);
  if(result) return result;
  uint32_t base = FLASH_GetAddress(page, 0);
  uint8_t piece[128];
  CRC_Start(&crc32_iso);
  for(uint32_t done = 0; done < header->size; done += sizeof(piece)) {
    uint32_t take = minv(header->size - done, sizeof(piece));
    if(FLASH_Copy(base + done, piece, take, NULL)) return BLD_Result_Crc;
    CRC_Next(&crc32_iso, piece, take);
  }
  *crc = CRC_End(&crc32_iso);
  uint32_t trailer;
  if(FLASH_Copy(base + header->size, &trailer, sizeof(trailer), NULL)) return BLD_Result_Crc;
  if(trailer != *crc || (named && *crc != *named)) return BLD_Result_Crc;
  return header_fits(header);
}

BLD_Result_t BLD_ImageCheck(uint16_t page, uint16_t pages, uint32_t *size, uint32_t *crc)
{
  BLD_Header_t header;
  uint32_t code;
  BLD_Result_t result = image_check(page, pages, NULL, &header, &code);
  if(result) return result;
  if(size) *size = header.size;
  if(crc) *crc = code;
  return BLD_Result_None;
}

// Pages the image in the application slot may take: the slot, or for the bootloader,
// which knows no slot until a request names one, the flash up to its end.
static inline uint16_t slot_pages(void) {
  return BOOT_SLOT_PAGES ? BOOT_SLOT_PAGES : (uint16_t)(FLASH_PAGES - BLD_APP_PAGE);
}

//-------------------------------------------------------------------------------------------- Keys
#if defined(BOOT_KEYS)

// Public keys of the product, `BOOT_KEYS` as written: 64 hex digits a key, from key `0`
static const char KEYS[] = BOOT_KEYS;
#define BLD_KEY_COUNT ((sizeof(KEYS) - 1) / (2 * BLD_KEY_SIZE))
_Static_assert(BLD_KEY_COUNT && (sizeof(KEYS) - 1) % (2 * BLD_KEY_SIZE) == 0,
  "BOOT_KEYS takes whole keys, 64 hex digits each");
#if defined(BOOT_DEV_KEY)
_Static_assert(sizeof(BOOT_DEV_KEY) - 1 == 2 * BLD_KEY_SIZE, "BOOT_DEV_KEY takes one key");
#endif

// Key the header names, decoded into `key`: `false` past the keys of the bootloader,
// below the key floor, or on a mistyped digit of `BOOT_KEYS`.
// The bootloader of builds decodes `BOOT_DEV_KEY` for every number instead.
static bool key_named(const BLD_Header_t *header, const floors_t *floors, uint8_t *key)
{
  if(header->key >= BLD_KEY_COUNT || header->key < floors->key) return false;
  #if defined(BOOT_DEV_KEY)
  const char *hex = BOOT_DEV_KEY;
  #else
  const char *hex = KEYS + header->key * 2u * BLD_KEY_SIZE;
  #endif
  for(uint32_t i = 0; i < BLD_KEY_SIZE; i++, hex += 2) {
    int16_t byte = hex_byte(hex);
    if(byte < 0) return false;
    key[i] = (uint8_t)byte;
  }
  return true;
}

// Image of `size` bytes at `base` signed with `key`
static bool signed_by(uint32_t base, uint32_t size, const uint8_t *key)
{
  uint8_t signature[BLD_SIGNATURE_SIZE];
  uint32_t at = base + size + BLD_SIGNATURE_OFFSET;
  if(FLASH_Copy(at, signature, sizeof(signature), NULL)) return false;
  // Ed25519 of RFC 8032: SHA-512 over `R`, the key and the image, reduced to the scalar `h`
  crypto_sha512_ctx ctx;
  crypto_sha512_init(&ctx);
  crypto_sha512_update(&ctx, signature, BLD_SIGNATURE_SIZE / 2);
  crypto_sha512_update(&ctx, key, BLD_KEY_SIZE);
  for(uint32_t done = 0; done < size; done += BLD_HASH_CHUNK) {
    IWDG_Refresh();
    uint32_t take = minv(size - done, BLD_HASH_CHUNK);
    if(FLASH_Copy(base + done, page_buffer, take, NULL)) return false;
    crypto_sha512_update(&ctx, page_buffer, take);
  }
  uint8_t hash[64], h[32];
  crypto_sha512_final(&ctx, hash);
  crypto_eddsa_reduce(h, hash);
  return !crypto_eddsa_check_equation(signature, key, h);
}

#endif
//------------------------------------------------------------------------------------------ Floors
#if defined(BOOT_KEYS) && BOOT_OTP_WORDS

_Static_assert(8u * BOOT_OTP_WORDS <= FLASH_OTP_SIZE, "BOOT_OTP_WORDS past the words of OTP");

// Word of the floors, a doubleword of OTP written once: what the floors became, and its check
typedef struct {
  uint16_t epoch;
  uint16_t key;
  uint32_t crc; // CRC-32 of the bytes before it
} otp_word_t;

static uint32_t otp_addr(uint32_t index) { return BOOT_OTP_ADDR + 8u * index; }

static uint32_t otp_crc(const otp_word_t *word) {
  return CRC_Run(&crc32_iso, word, offsetof(otp_word_t, crc));
}

// Word `index` read clean through ECC into `word`: `false` torn, or corrected by ECC
static bool otp_read(uint32_t index, otp_word_t *word)
{
  bool corrected;
  return !FLASH_Copy(otp_addr(index), word, sizeof(*word), &corrected) && !corrected;
}

// Floors: the highest of the words that read clean with a matching check.
// An erased word holds nothing, one a power cut tore is skipped.
static floors_t floors_read(void)
{
  floors_t floors = { 0, 0 };
  for(uint32_t i = 0; i < BOOT_OTP_WORDS; i++) {
    otp_word_t word;
    if(!otp_read(i, &word) || erased((const uint8_t *)&word, sizeof(word))) continue;
    if(word.crc != otp_crc(&word)) continue;
    floors.epoch = maxv(floors.epoch, word.epoch);
    floors.key = maxv(floors.key, word.key);
  }
  return floors;
}

#endif
//----------------------------------------------------------------------------------------- Mailbox

// Mailbox: one PDB page without a ring, the last of the bootloader, formatted before a request
static PDB_t mailbox = {
  .page_start = (uint16_t)BLD_MAILBOX_PAGE, .page_count = 1,
  .record_size = sizeof(BLD_Entry_t), .kind = BLD_MAILBOX_KIND, .no_wrap = true,
};

// Newest entry of the mailbox, `false` without one
static bool mailbox_newest(BLD_Entry_t *entry)
{
  if(PDB_Init(&mailbox)) return false;
  PDB_Iter_t iter;
  PDB_Query_t query = { .dir = PDB_Desc };
  PDB_IterInit(&mailbox, &iter, &query);
  return !PDB_IterNext(&iter, entry);
}

// Entry behind the newest one: a request from the application, a result from the bootloader
static status_t mailbox_append(BLD_Result_t result, uint16_t page, uint32_t crc)
{
  BLD_Entry_t entry = { .result = (uint16_t)result, .page = page, .crc = crc };
  return PDB_Insert(&mailbox, &entry);
}

//---------------------------------------------------------------------------------------- Transfer

// What stays put while an image runs: its bootloader and the last result.
// Read at the first `BLD_Status`, and again once `BLD_End` wrote a request.
static struct {
  bool known;
  uint32_t loader;
  BLD_Result_t last;
} facts;

status_t BLD_Begin(uint32_t size, uint32_t crc)
{
  boot.active = false;
  if(!BOOT_SLOT_PAGES) return ERR;
  if(size <= BLD_HEADER_OFFSET + sizeof(BLD_Header_t)) return ERR;
  if(size > BLD_SLOT_SIZE - BLD_TRAILER_SIZE) return ERR;
  boot.size = size;
  boot.crc = crc;
  boot.offset = 0;
  memset(boot.signature, 0xFF, sizeof(boot.signature));
  boot.active = true;
  return OK;
}

status_t BLD_Signature(const uint8_t *signature)
{
  if(!boot.active) return ERR;
  memcpy(boot.signature, signature, sizeof(boot.signature));
  return OK;
}

status_t BLD_Write(uint32_t offset, const uint8_t *data, uint16_t len)
{
  if(!boot.active) return ERR;
  if(offset != boot.offset || len > boot.size - boot.offset || stage(data, len)) {
    boot.active = false;
    return ERR;
  }
  return OK;
}

status_t BLD_End(void)
{
  if(!boot.active) return ERR;
  boot.active = false;
  if(boot.offset != boot.size) return ERR;
  // CRC32 first, then the signature, erased unless the transfer carried one
  uint8_t trailer[BLD_TRAILER_SIZE];
  memset(trailer, 0xFF, sizeof(trailer));
  memcpy(trailer, &boot.crc, sizeof(boot.crc));
  memcpy(trailer + BLD_SIGNATURE_OFFSET, boot.signature, sizeof(boot.signature));
  if(stage(trailer, sizeof(trailer))) return ERR;
  if(boot.offset % FLASH_PAGE_SIZE && flush_page(boot.offset)) return ERR;
  // The bootloader alone judges the image, the request only names it
  facts.known = false; // the request is the newest entry from here on
  if(PDB_Delete(&mailbox)) return ERR;
  return mailbox_append(BLD_Result_None, BLD_STAGING_PAGE, boot.crc);
}

void BLD_Abort(void)
{
  boot.active = false;
}

uint32_t BLD_Offset(void)
{
  return boot.offset;
}

//------------------------------------------------------------------------------------------ Status
#if defined(STM32)

// Header of this image, placed at `BLD_HEADER_OFFSET` by the linker script,
// which also measures `size` and leaves the erased trailer bytes behind the image.
extern uint8_t _image_size[];
__attribute__((section(".app_header"), used))
const BLD_Header_t BootHeader = {
  .magic = BLD_MAGIC,
  .size = (uint32_t)_image_size,
  .origin = (uint32_t)&BootHeader - BLD_HEADER_OFFSET,
  .chip = BOOT_CHIP,
  .board = BOOT_BOARD_VARIANT,
  .epoch = PRO_SECURITY_EPOCH,
  .key = PRO_SIGNING_KEY,
};

#endif

// What `BLD_Status` reads once: the bootloader code with its keys as a CRC-32
// and the last result in the mailbox.
static void facts_read(void)
{
  if(facts.known) return;
  facts.known = true;
  uint32_t code = FLASH_GetAddress(BLD_MAILBOX_PAGE, 0) - FLASH_GetAddress(0, 0);
  facts.loader = CRC_Run(&crc32_iso, FLASH_Ref(FLASH_GetAddress(0, 0)), code);
  BLD_Entry_t entry;
  facts.last = mailbox_newest(&entry) ? (BLD_Result_t)entry.result : BLD_Result_None;
}

void BLD_Status(BLD_Status_t *status)
{
  memset(status, 0, sizeof(*status));
  status->app_addr = FLASH_GetAddress(BLD_APP_PAGE, 0);
  status->slot_size = BLD_SLOT_SIZE;
  status->boot = BOOT_SLOT_PAGES > 0;
  status->active = boot.active;
  #if defined(STM32)
  status->image_size = BootHeader.size;
  status->image_crc = FLASH_Read(BootHeader.origin + BootHeader.size);
  // `RDP` byte: `0xAA` open, `0xCC` locked for good, any other value the debugger shut out
  uint32_t rdp = FLASH->OPTR & FLASH_OPTR_RDP;
  status->rdp = rdp == 0xAAu ? 0 : (rdp == 0xCCu ? 2 : 1);
  #endif
  // The rest is what a bootloader leaves: without one its pages hold the application
  if(!status->boot) return;
  // Magic tells a report of this start from RAM no bootloader wrote
  if(_bld_report.magic == BLD_REPORT_MAGIC) {
    status->result = _bld_report.result;
    status->epoch = _bld_report.epoch;
    status->key = _bld_report.key;
  }
  facts_read();
  status->loader = facts.loader;
  status->last = facts.last;
}

//-------------------------------------------------------------------------------------- Bootloader
#if(BOOT_PAGES)

// Copy `bytes` from the page `src` into the application slot, page by page.
// They fit in front of `src`: the check of the request bounds them to the pages between.
static status_t slot_copy(uint16_t src, uint32_t bytes)
{
  uint16_t pages = (uint16_t)((bytes + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE);
  // The header page is erased first and written last: a copy cut by a power loss
  // leaves no header until that last page, and the CRC32 of the image catches a cut in it.
  if(FLASH_Erase(BLD_APP_PAGE)) return ERR;
  for(uint16_t i = pages; i-- > 0;) {
    IWDG_Refresh();
    if(FLASH_Copy(FLASH_GetAddress(src + i, 0), page_buffer, FLASH_PAGE_SIZE, NULL)) return ERR;
    if(FLASH_WritePage(BLD_APP_PAGE + i, page_buffer)) return ERR;
  }
  return OK;
}

// Slot holds the image `header` and `crc` describe, copied before a power loss cut the step short
static bool slot_holds(const BLD_Header_t *header, uint32_t crc, uint16_t pages)
{
  uint32_t size, code;
  if(BLD_ImageCheck(BLD_APP_PAGE, pages, &size, &code)) return false;
  return size == header->size && code == crc;
}

// Verdict of the bootloader on the image on `page`: the checks of any image and of `named`,
// then with `BOOT_KEYS` the key it names, its signature and its epoch against the floors.
static BLD_Result_t image_verdict(uint16_t page, uint16_t pages, const uint32_t *named,
  const floors_t *floors, BLD_Header_t *header, uint32_t *crc)
{
  BLD_Result_t result = image_check(page, pages, named, header, crc);
  if(result) return result;
  #if defined(BOOT_KEYS)
  uint8_t key[BLD_KEY_SIZE];
  if(!key_named(header, floors, key) || !signed_by(FLASH_GetAddress(page, 0), header->size, key)) {
    return BLD_Result_Signature;
  }
  if(header->epoch < floors->epoch) return BLD_Result_Epoch;
  #else
  unused(floors);
  #endif
  return BLD_Result_None;
}

// Floors where the chip keeps them: OTP, or without it the header of the image
// in the application slot, an empty or broken slot keeping none; `plain` keeps none at all.
// Whoever writes the slot can break it anyway, so its header goes unsigned here.
static floors_t floors_get(void)
{
  #if defined(BOOT_KEYS) && BOOT_OTP_WORDS
  return floors_read();
  #else
  floors_t floors = { 0, 0 };
  #if defined(BOOT_KEYS)
  BLD_Header_t header;
  uint32_t crc;
  if(!image_check(BLD_APP_PAGE, slot_pages(), NULL, &header, &crc)) {
    floors = (floors_t){ header.epoch, header.key };
  }
  #endif
  return floors;
  #endif
}

// Floors raised to the image of `header` where it goes past them, `floors` following:
// a new word in the first free one, the next after a word that does not read back.
// With no free word left the floors stay as they are.
// Kept in the slot, the floors are the image there, the one of `header` once it starts.
static void floors_raise(floors_t *floors, const BLD_Header_t *header)
{
  #if defined(BOOT_KEYS) && BOOT_OTP_WORDS
  if(header->epoch <= floors->epoch && header->key <= floors->key) return;
  otp_word_t word = { maxv(header->epoch, floors->epoch), maxv(header->key, floors->key), 0 };
  word.crc = otp_crc(&word);
  uint32_t pair[2];
  memcpy(pair, &word, sizeof(pair));
  for(uint32_t i = 0; i < BOOT_OTP_WORDS; i++) {
    otp_word_t blank;
    if(!otp_read(i, &blank) || !erased((const uint8_t *)&blank, sizeof(blank))) continue;
    if(FLASH_Write(otp_addr(i), pair[0], pair[1])) continue;
    *floors = (floors_t){ word.epoch, word.key };
    return;
  }
  #elif defined(BOOT_KEYS)
  *floors = (floors_t){ header->epoch, header->key };
  #else
  unused(floors);
  unused(header);
  #endif
}

// The request carried out: the image it names judged, copied unless the slot already holds it,
// the floors raised and the result appended last. A step cut before the result repeats
// on the next start, the request still the newest entry. Returns what this start reports.
static BLD_Result_t request_run(const BLD_Entry_t *request, floors_t *floors)
{
  // The staging slot lies behind the application slot and is as long as it.
  // A page on the application slot or past the chip leaves `image_check` no room.
  uint16_t page = request->page;
  uint16_t pages = (uint16_t)(page - BLD_APP_PAGE);
  BLD_Header_t header;
  uint32_t crc;
  BLD_Result_t result = image_verdict(page, pages, &request->crc, floors, &header, &crc);
  if(!result) {
    bool held = slot_holds(&header, crc, pages);
    if(!held && slot_copy(page, header.size + BLD_TRAILER_SIZE)) return BLD_Result_None;
    if(!held && !slot_holds(&header, crc, pages)) return BLD_Result_None;
    floors_raise(floors, &header);
    result = BLD_Result_Installed;
  }
  mailbox_append(result, page, request->crc);
  return result;
}

uint32_t BLD_Install(void)
{
  #if defined(BOOT_KEYS) && defined(STM32)
  RCC_BootClock(); // a signature takes seconds at the reset clock
  #endif
  floors_t floors = floors_get();
  BLD_Result_t result = BLD_Result_None;
  // A request is an entry without a result; a result answers it, so nothing runs twice
  BLD_Entry_t entry;
  if(mailbox_newest(&entry) && entry.result == BLD_Result_None) {
    result = request_run(&entry, &floors);
  }
  BLD_Header_t header;
  uint32_t crc;
  bool runs = !image_verdict(BLD_APP_PAGE, slot_pages(), NULL, &floors, &header, &crc);
  // An image a programmer wrote past the floors raises them before it starts,
  // so a board fresh from the factory takes no older image and no revoked key either.
  if(runs) floors_raise(&floors, &header);
  _bld_report = (BLD_Report_t){ BLD_REPORT_MAGIC, result, floors.epoch, floors.key, 0 };
  #if defined(BOOT_KEYS) && defined(STM32)
  RCC_ResetClock(); // the image starts as after a reset, whichever bootloader it runs under
  #endif
  return runs ? FLASH_GetAddress(BLD_APP_PAGE, 0) : 0;
}

#else

uint32_t BLD_Install(void)
{
  return 0;
}

#endif
//-------------------------------------------------------------------------------------------------
// On the chip alone: the jump
#if defined(STM32)

void BLD_Jump(uint32_t addr)
{
  const uint32_t *vector = (const uint32_t *)addr;
  uint32_t sp = vector[0];
  uint32_t pc = vector[1];
  SCB->VTOR = addr;
  // Stack and jump in one piece: nothing may touch the old stack once `msp` moved
  __asm volatile("msr msp, %0\n\tbx %1" : : "r"(sp), "r"(pc) : "memory");
  while(1);
}

#endif
//------------------------------------------------------------------------------------------- Shell

// Bootloader the image is built for: one under `plain` does not start under `key`, and back
#if defined(BOOT_KEYS)
static const char MODE[] = "key";
#else
static const char MODE[] = "plain";
#endif

// Result names `boot info`, `boot data` and `boot end` print, in the order of `BLD_Result_t`
static const char *const RESULT_NAMES[] = {
  "none", "installed", "crc", "size", "origin", "chip", "board", "signature", "epoch",
};

// Name of a result, `unknown` for a value only a newer bootloader knows
static const char *result_name(uint32_t result) {
  return result < array_len(RESULT_NAMES) ? RESULT_NAMES[result] : "unknown";
}

// Hex text decoded in place, `-1` on a stray character or an odd length
static int32_t hex_decode_this(char *str)
{
  int32_t len = 0;
  uint8_t *out = (uint8_t *)str;
  for(; str[0] && str[1]; str += 2) {
    int16_t byte = hex_byte(str);
    if(byte < 0) return -1;
    out[len++] = (uint8_t)byte;
  }
  return *str ? -1 : len;
}

void BLD_Bash(char **argv, uint16_t argc)
{
  CMD_Argc(2, 5);
  switch(hash_djb2_ci(argv[1])) {
    case HASH_Info: {
      CMD_Argc(2);
      BLD_Status_t status;
      BLD_Status(&status);
      LOG_Bash("BOOT info boot:%u app:%08x slot:%u page:%u line:%u image:%u crc:%08x "
        "active:%u mode:%s result:%s last:%s epoch:%u revoked:%u loader:%08x rdp:%u",
        status.boot, status.app_addr, status.slot_size, (uint32_t)FLASH_PAGE_SIZE,
        (uint32_t)DBG_RX_SIZE - 1u, status.image_size, status.image_crc, status.active,
        MODE, result_name(status.result), result_name(status.last), status.epoch,
        status.key, status.loader, status.rdp);
      return;
    }
    case HASH_Begin: {
      CMD_Argc(4, 5);
      if(!str_is_u32(argv[2])) CMD_ArgvExit(2);
      if(!str_is_u32(argv[3])) CMD_ArgvExit(3);
      // Decoded first, so a malformed signature leaves no transfer open
      if(argc == 5 && hex_decode_this(argv[4]) != (int32_t)BLD_SIGNATURE_SIZE) CMD_ArgvExit(4);
      uint32_t size = str_to_int(argv[2]);
      uint32_t crc = str_to_int(argv[3]);
      if(BLD_Begin(size, crc)) {
        LOG_Error("BOOT image of " ANSI_LIME "%u" ANSI_END " bytes does not fit", size);
        return;
      }
      if(argc == 5) BLD_Signature((const uint8_t *)argv[4]);
      LOG_Bash("BOOT begin size:%u crc:%08x", size, crc);
      return;
    }
    case HASH_Data: {
      CMD_Argc(4);
      if(!str_is_u32(argv[2])) CMD_ArgvExit(2);
      int32_t len = hex_decode_this(argv[3]);
      if(len < 0) CMD_ArgvExit(3);
      uint32_t offset = str_to_int(argv[2]);
      if(BLD_Write(offset, (const uint8_t *)argv[3], (uint16_t)len)) {
        LOG_Error("BOOT data at " ANSI_LIME "%u" ANSI_END " refused, expected "
          ANSI_LIME "%u" ANSI_END, offset, BLD_Offset());
        return;
      }
      LOG_Bash("BOOT data offset:%u", BLD_Offset());
      return;
    }
    case HASH_End: {
      CMD_Argc(2);
      bool complete = boot.active && boot.offset == boot.size;
      if(BLD_End()) {
        if(!complete) LOG_Error("BOOT image incomplete");
        else LOG_Error("BOOT image not staged, a flash write failed");
        return;
      }
      LOG_Bash("BOOT end crc:%08x reset", boot.crc);
      DbgReset = true;
      return;
    }
    case HASH_Abort:
      CMD_Argc(2);
      BLD_Abort();
      LOG_Bash("BOOT abort");
      return;
    default:
      CMD_ArgvExit(1);
  }
}

//-------------------------------------------------------------------------------------------------

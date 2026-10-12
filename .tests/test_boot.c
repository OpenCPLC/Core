// .tests/test_boot.c
// Bootloader on the host: transfer, checks and install,
// with a power cut before and halfway through every flash operation.
// The slot may hold the old image or the new one, never half of either.
// Built here for the `plain` bootloader and again with `BOOT_KEY` for the `key` one,
// where every image carries the signature of a test key.

// A G081 under its bootloader: 128kB of flash in pages of 2kB, as Forge sets it up
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64
#define BOOT_PAGES 4
#define BOOT_SLOT_PAGES 30
#define BOOT_CHIP 0x460

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "test.h"
// Host flash model and `pwr.h` first: their guards keep the chip headers of `boot.c` out
#include "flash.c"
#include "pwr.h"
#include "hal/stm32/sys/boot.c"

#if(BOOT_KEY)
  #include "data/forge.h"
#endif

#define APP_ADDR (FLASH_BASE + BOOT_PAGES * FLASH_PAGE_SIZE)
#define STAGING_ADDR (APP_ADDR + BOOT_SLOT_SIZE)
#define KEY_ADDR (FLASH_BASE + BOOT_MAILBOX_PAGE * FLASH_PAGE_SIZE - BOOT_KEY_SIZE)
#define LINE_SIZE 200u     // image bytes in one `boot data` line, out of step with the pages
#define OTHER_CHIP 0x467u  // G0C1, which shares the G081 bootloader and its slot address

// RAM below the image, where the bootloader leaves its report
BOOT_Report_t _boot_report;

static uint8_t old_image[BOOT_SLOT_SIZE];
static uint8_t new_image[BOOT_SLOT_SIZE];

#if(BOOT_KEY)

static uint8_t test_secret[64]; // signs every image, its public half sits in the key slot
static uint8_t test_key[BOOT_KEY_SIZE];

// Key the bootloader checks with, written where its build keeps it
static void key_program(const uint8_t *key) { FLASH_HostProgram(KEY_ADDR, key, BOOT_KEY_SIZE); }

// Signature of an image in its own trailer, the way Forge writes it
static void image_sign(uint8_t *image, uint32_t size, const uint8_t *secret) {
  crypto_ed25519_sign(image + size + BOOT_SIGNATURE_OFFSET, secret, image, size);
}

#endif

// Board fresh from the factory, with the test key in a `key` bootloader
static void flash_erase_all(void)
{
  FLASH_HostBlank();
  #if(BOOT_KEY)
  key_program(test_key);
  #endif
}

// Image laid out as linked: code, the header at `0x200`, the trailer erased or signed.
// Returns its CRC32.
static uint32_t image_make(uint8_t *image, uint32_t size, uint8_t seed)
{
  memset(image, 0xFF, size + BOOT_TRAILER_SIZE);
  for(uint32_t i = 0; i < size; i++) image[i] = (uint8_t)(i * 7u + seed);
  BOOT_Header_t header = { BOOT_MAGIC, size, APP_ADDR, BOOT_CHIP, PRO_BOOT_EPOCH };
  memcpy(image + BOOT_HEADER_OFFSET, &header, sizeof(header));
  #if(BOOT_KEY)
  image_sign(image, size, test_secret);
  #endif
  return CRC_Run(&crc32_iso, image, size);
}

// Header field `at` bytes into the header set to `value`, signed anew. Returns the new CRC32.
static uint32_t image_patch(uint8_t *image, uint32_t size, uint32_t at, uint32_t value)
{
  memcpy(image + BOOT_HEADER_OFFSET + at, &value, sizeof(value));
  #if(BOOT_KEY)
  image_sign(image, size, test_secret);
  #endif
  return CRC_Run(&crc32_iso, image, size);
}

// Image flashed by a programmer as `-dist.hex` holds it: the CRC32 in front of its trailer
static void slot_program(const uint8_t *image, uint32_t size, uint32_t crc)
{
  static uint8_t flashed[BOOT_SLOT_SIZE];
  memcpy(flashed, image, size + BOOT_TRAILER_SIZE);
  memcpy(flashed + size, &crc, sizeof(crc));
  FLASH_HostProgram(APP_ADDR, flashed, size + BOOT_TRAILER_SIZE);
}

// Image sent the way the console sends it, line by line, with the signature of its trailer
static status_t transfer(const uint8_t *image, uint32_t size, uint32_t crc)
{
  if(BOOT_Begin(size, crc)) return ERR;
  if(BOOT_Signature(image + size + BOOT_SIGNATURE_OFFSET)) return ERR;
  for(uint32_t at = 0; at < size; at += LINE_SIZE) {
    uint16_t len = (uint16_t)minv(size - at, LINE_SIZE);
    if(BOOT_Write(at, image + at, len)) return ERR;
  }
  return BOOT_End();
}

// Staged image and its record, as an application without the checks of this Core leaves them
static void stage_unchecked(const uint8_t *image, uint32_t size, uint32_t crc)
{
  static uint8_t staged[BOOT_SLOT_SIZE];
  memcpy(staged, image, size + BOOT_TRAILER_SIZE);
  memcpy(staged + size, &crc, sizeof(crc));
  FLASH_HostProgram(STAGING_ADDR, staged, size + BOOT_TRAILER_SIZE);
  BOOT_Mailbox_t record = { BOOT_MAILBOX_MAGIC, size, BOOT_STAGING_PAGE, crc };
  FLASH_HostProgram(FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0),
    (const uint8_t *)&record, sizeof(record));
}

// CRC32 of the whole image in the application slot, `0` when there is none
static uint32_t slot_crc(void)
{
  uint32_t crc;
  return BOOT_ImageCheck(BOOT_APP_PAGE, NULL, &crc) ? 0 : crc;
}

// Mailbox read as the bootloader reads it, through ECC: a record a cut tore holds nothing
static bool mailbox_read(BOOT_Mailbox_t *box)
{
  uint32_t addr = FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0);
  return !FLASH_Copy(addr, box, sizeof(*box), NULL) && box->magic == BOOT_MAILBOX_MAGIC;
}

static bool mailbox_has_record(void)
{
  BOOT_Mailbox_t box;
  return mailbox_read(&box);
}

// Mailbox holds the whole record of a staged image, its magic alone is no record
static bool mailbox_records(uint32_t size, uint32_t crc)
{
  BOOT_Mailbox_t box;
  return mailbox_read(&box) && box.size == size && box.page == BOOT_STAGING_PAGE &&
    box.crc == crc;
}

// Bootloader start over a cleared report. Returns the report it leaves, `-1` for none.
static int32_t start(uint32_t *app)
{
  memset(&_boot_report, 0, sizeof(_boot_report));
  uint32_t addr = BOOT_Install();
  if(app) *app = addr;
  return _boot_report.magic == BOOT_REPORT_MAGIC ? (int32_t)_boot_report.result : -1;
}

//-------------------------------------------------------------------------------------- Both modes

// Trailer lands whole, wherever the page boundary falls inside it
static void trailer_crosses_pages(void)
{
  const uint32_t sizes[] = {
    3 * FLASH_PAGE_SIZE - 8, 3 * FLASH_PAGE_SIZE - 40, 3 * FLASH_PAGE_SIZE, 5000,
  };
  for(uint32_t n = 0; n < array_len(sizes); n++) {
    uint32_t size = sizes[n];
    flash_erase_all();
    uint32_t crc = image_make(new_image, size, 3);
    TEST_Check(transfer(new_image, size, crc) == OK, "size:%u", size);
    uint8_t trailer[BOOT_TRAILER_SIZE];
    memcpy(trailer, new_image + size, sizeof(trailer));
    memcpy(trailer, &crc, sizeof(crc));
    TEST_Check(!memcmp(FLASH_Ref(STAGING_ADDR + size), trailer, sizeof(trailer)), "size:%u", size);
    TEST_Check(mailbox_has_record(), "size:%u", size);
    TEST_Check(BOOT_Install() == APP_ADDR, "size:%u", size);
    uint32_t copied = size + BOOT_TRAILER_SIZE;
    TEST_Check(!memcmp(FLASH_Ref(APP_ADDR), FLASH_Ref(STAGING_ADDR), copied), "size:%u", size);
    TEST_Check(!mailbox_has_record(), "size:%u", size);
  }
}

static void install_staged(void) { BOOT_Install(); }

// An install cut anywhere and repeated on the next start ends with the new image
static void install_survives_a_cut(void)
{
  uint32_t old_size = 3000, new_size = 5000; // three pages: each cut costs a whole install
  flash_erase_all();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  uint32_t new_crc = image_make(new_image, new_size, 2);
  slot_program(old_image, old_size, old_crc);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged");
  FLASH_HostSave();
  uint32_t before = FLASH_HostCycles();
  install_staged();
  uint32_t total = FLASH_HostCycles() - before;
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Corrected; where++) {
    for(uint32_t cut = 0; cut <= total; cut++) {
      FLASH_HostRestore();
      FLASH_HostCut((int32_t)cut, where, install_staged);
      uint32_t crc = slot_crc();
      TEST_Check(crc == 0 || crc == old_crc || crc == new_crc, "where:%d cut:%u", where, cut);
      bool recorded = mailbox_has_record();
      uint32_t app;
      int32_t result = start(&app);
      TEST_Check(app == APP_ADDR && slot_crc() == new_crc, "where:%d cut:%u", where, cut);
      // A record the cut left makes this start finish the install and report it
      int32_t expected = recorded ? BOOT_Result_Installed : BOOT_Result_None;
      TEST_Check(result == expected, "where:%d cut:%u", where, cut);
      TEST_Check(!mailbox_has_record(), "where:%d cut:%u", where, cut);
    }
  }
}

// Image `transfer_new` sends: `new_image` of this size, with this CRC
static uint32_t sent_size, sent_crc;
static void transfer_new(void) { transfer(new_image, sent_size, sent_crc); }

// A transfer cut anywhere leaves the old image running, or the new one once the record stands
static void transfer_survives_a_cut(void)
{
  uint32_t old_size = 3000;
  sent_size = 5000; // three pages: each cut costs a whole install
  flash_erase_all();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  sent_crc = image_make(new_image, sent_size, 2);
  slot_program(old_image, old_size, old_crc);
  FLASH_HostSave();
  uint32_t before = FLASH_HostCycles();
  transfer_new();
  uint32_t total = FLASH_HostCycles() - before;
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Corrected; where++) {
    for(uint32_t cut = 0; cut <= total; cut++) {
      FLASH_HostRestore();
      FLASH_HostCut((int32_t)cut, where, transfer_new);
      bool recorded = mailbox_records(sent_size, sent_crc);
      TEST_Check(BOOT_Install() == APP_ADDR, "where:%d cut:%u", where, cut);
      TEST_Check(slot_crc() == (recorded ? sent_crc : old_crc), "where:%d cut:%u", where, cut);
    }
  }
}

// Ones a programmer writes over the mailbox take no record,
// and `FLASH_Erase` spares a page that reads blank:
// the reason `-dist.hex` leaves the rest of the bootloader region out.
static void ones_from_a_programmer_block_the_mailbox(void)
{
  uint32_t size = 9000;
  flash_erase_all();
  uint32_t crc = image_make(new_image, size, 2);
  static uint8_t ones[FLASH_PAGE_SIZE];
  memset(ones, 0xFF, sizeof(ones));
  FLASH_HostProgram(FLASH_GetAddress(BOOT_MAILBOX_PAGE, 0), ones, sizeof(ones));
  TEST_Check(transfer(new_image, size, crc) == ERR, "record over written ones");
  TEST_Check(!mailbox_has_record(), "record over written ones");
}

// An image for another chip or another slot leaves no record, and the check names the reason
static void application_refuses_a_foreign_image(void)
{
  const struct { uint32_t at, value; BOOT_Result_t result; } cases[] = {
    { offsetof(BOOT_Header_t, chip),   OTHER_CHIP,   BOOT_Result_Chip   },
    { offsetof(BOOT_Header_t, origin), STAGING_ADDR, BOOT_Result_Origin },
  };
  uint32_t size = 9000;
  for(uint32_t n = 0; n < array_len(cases); n++) {
    flash_erase_all();
    image_make(new_image, size, 2);
    uint32_t crc = image_patch(new_image, size, cases[n].at, cases[n].value);
    TEST_Check(transfer(new_image, size, crc) == ERR, "case:%u", n);
    TEST_Check(!mailbox_has_record(), "case:%u", n);
    TEST_Check(BOOT_ImageCheck(BOOT_STAGING_PAGE, NULL, NULL) == cases[n].result, "case:%u", n);
  }
}

// Staged for another chip past the check of `BOOT_End`, an image is refused at start:
// the record goes, the old image keeps running, and the report says why.
static void bootloader_refuses_a_foreign_image(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  flash_erase_all();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  slot_program(old_image, old_size, old_crc);
  image_make(new_image, new_size, 2);
  uint32_t crc = image_patch(new_image, new_size, offsetof(BOOT_Header_t, chip), OTHER_CHIP);
  stage_unchecked(new_image, new_size, crc);
  uint32_t app;
  TEST_Check(start(&app) == BOOT_Result_Chip, "reported");
  TEST_Check(app == APP_ADDR && slot_crc() == old_crc, "old image runs");
  TEST_Check(!mailbox_has_record(), "record dropped");
  TEST_Check(start(NULL) == BOOT_Result_None, "next start");
}

// Every start reports, so no result outlives the start that made it
static void every_start_reports(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  flash_erase_all();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  slot_program(old_image, old_size, old_crc);
  TEST_Check(start(NULL) == BOOT_Result_None, "nothing waits");
  uint32_t new_crc = image_make(new_image, new_size, 2);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged");
  TEST_Check(start(NULL) == BOOT_Result_Installed, "installed");
  TEST_Check(start(NULL) == BOOT_Result_None, "next start");
  flash_erase_all();
  TEST_Check(start(NULL) == BOOT_Result_None, "empty slot");
}

// The trailer has to match the bytes: an image with its CRC32 still erased never starts
static void erased_crc_never_starts(void)
{
  uint32_t size = 9000;
  flash_erase_all();
  image_make(old_image, size, 1);
  FLASH_HostProgram(APP_ADDR, old_image, size + BOOT_TRAILER_SIZE);
  uint32_t app;
  start(&app);
  TEST_Check(app == 0, "erased trailer");
}

//-------------------------------------------------------------------------------------- Plain mode
#if(!BOOT_KEY)

// A signature given to the transfer lands behind the CRC32 and goes into the slot with the image
static void signature_rides_in_the_trailer(void)
{
  uint32_t size = 9000;
  flash_erase_all();
  uint32_t crc = image_make(new_image, size, 2);
  uint8_t *signature = new_image + size + BOOT_SIGNATURE_OFFSET;
  for(uint32_t i = 0; i < BOOT_SIGNATURE_SIZE; i++) signature[i] = (uint8_t)(i + 1);
  TEST_Check(BOOT_Signature(signature) == ERR, "no transfer open");
  TEST_Check(transfer(new_image, size, crc) == OK, "staged");
  uint32_t at = size + BOOT_SIGNATURE_OFFSET;
  TEST_Check(!memcmp(FLASH_Ref(STAGING_ADDR + at), signature, BOOT_SIGNATURE_SIZE), "staged");
  TEST_Check(BOOT_Install() == APP_ADDR, "installed");
  TEST_Check(!memcmp(FLASH_Ref(APP_ADDR + at), signature, BOOT_SIGNATURE_SIZE), "installed");
}

#endif
//---------------------------------------------------------------------------------------- Key mode
#if(BOOT_KEY)

// An image signed with another key installs nowhere: the record goes and the report says why
static void foreign_signature_never_installs(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  flash_erase_all();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  slot_program(old_image, old_size, old_crc);
  uint32_t new_crc = image_make(new_image, new_size, 2);
  uint8_t seed[32] = { 9 }, foreign_secret[64], foreign_key[BOOT_KEY_SIZE];
  crypto_ed25519_key_pair(foreign_secret, foreign_key, seed);
  image_sign(new_image, new_size, foreign_secret);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged: an application holds no key");
  uint32_t app;
  TEST_Check(start(&app) == BOOT_Result_Signature, "reported");
  TEST_Check(app == APP_ADDR && slot_crc() == old_crc, "old image runs");
  TEST_Check(!mailbox_has_record(), "record dropped");
}

// A byte changed under a matching CRC32 still breaks the signature: the image never starts
static void changed_byte_never_starts(void)
{
  uint32_t size = 9000;
  flash_erase_all();
  image_make(old_image, size, 1);
  old_image[size / 2] ^= 0x01;
  slot_program(old_image, size, CRC_Run(&crc32_iso, old_image, size));
  uint32_t app;
  start(&app);
  TEST_Check(app == 0, "changed byte");
}

// A bootloader nobody gave a key starts nothing, a signed image included
static void key_less_bootloader_starts_nothing(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  slot_program(old_image, size, image_make(old_image, size, 1));
  uint32_t app;
  start(&app);
  TEST_Check(app == 0, "no key");
}

// An install never lowers the epoch of the slot, an equal or higher one goes in,
// and an empty slot takes any.
static void epoch_never_drops(void)
{
  const struct { uint32_t slot, image; BOOT_Result_t result; } cases[] = {
    { 2, 1, BOOT_Result_Epoch     },
    { 2, 2, BOOT_Result_Installed },
    { 2, 3, BOOT_Result_Installed },
    { 0, 0, BOOT_Result_Installed }, // no image in the slot
  };
  uint32_t old_size = 9000, new_size = 21000, at = offsetof(BOOT_Header_t, epoch);
  for(uint32_t n = 0; n < array_len(cases); n++) {
    flash_erase_all();
    uint32_t old_crc = 0;
    if(cases[n].slot) {
      image_make(old_image, old_size, 1);
      old_crc = image_patch(old_image, old_size, at, cases[n].slot);
      slot_program(old_image, old_size, old_crc);
    }
    image_make(new_image, new_size, 2);
    uint32_t new_crc = image_patch(new_image, new_size, at, cases[n].image);
    TEST_Check(transfer(new_image, new_size, new_crc) == OK, "case:%u", n);
    TEST_Check(start(NULL) == (int32_t)cases[n].result, "case:%u", n);
    uint32_t running = cases[n].result == BOOT_Result_Epoch ? old_crc : new_crc;
    TEST_Check(slot_crc() == running, "case:%u", n);
  }
}

// Monocypher signs as Forge does: the same key from the seed, the same signature on the image
static void monocypher_signs_as_forge(void)
{
  uint8_t seed[32], secret[64], key[BOOT_KEY_SIZE], signature[BOOT_SIGNATURE_SIZE];
  memcpy(seed, FORGE_KEY_SEED, sizeof(seed));
  crypto_ed25519_key_pair(secret, key, seed);
  TEST_Check(!memcmp(key, FORGE_KEY, sizeof(key)), "key");
  image_make(new_image, FORGE_SIZE, FORGE_IMAGE_SEED);
  crypto_ed25519_sign(signature, secret, new_image, FORGE_SIZE);
  TEST_Check(!memcmp(signature, FORGE_SIGNATURE, sizeof(signature)), "signature");
}

// What Forge signs, the bootloader starts
static void forge_signature_starts(void)
{
  flash_erase_all();
  key_program(FORGE_KEY);
  uint32_t crc = image_make(new_image, FORGE_SIZE, FORGE_IMAGE_SEED);
  memcpy(new_image + FORGE_SIZE + BOOT_SIGNATURE_OFFSET, FORGE_SIGNATURE, BOOT_SIGNATURE_SIZE);
  slot_program(new_image, FORGE_SIZE, crc);
  uint32_t app;
  start(&app);
  TEST_Check(app == APP_ADDR, "started");
}

#endif
//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  #if(BOOT_KEY)
  uint8_t seed[32] = { 1 };
  crypto_ed25519_key_pair(test_secret, test_key, seed);
  #endif
  TEST(trailer_crosses_pages);
  TEST(install_survives_a_cut);
  TEST(transfer_survives_a_cut);
  TEST(ones_from_a_programmer_block_the_mailbox);
  TEST(application_refuses_a_foreign_image);
  TEST(bootloader_refuses_a_foreign_image);
  TEST(every_start_reports);
  TEST(erased_crc_never_starts);
  #if(!BOOT_KEY)
  TEST(signature_rides_in_the_trailer);
  #else
  TEST(foreign_signature_never_installs);
  TEST(changed_byte_never_starts);
  TEST(key_less_bootloader_starts_nothing);
  TEST(epoch_never_drops);
  TEST(monocypher_signs_as_forge);
  TEST(forge_signature_starts);
  #endif
}

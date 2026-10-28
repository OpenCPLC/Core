// .tests/test_bld.c

// Bootloader on the host: transfer, mailbox, checks and install,
// with a power cut before and halfway through every flash operation.
// The slot may hold the old image or the new one, never half of either.
// Built here for the `plain` bootloader and again in `test_bld_key.c` for the `key` one,
// where every image carries the signature of one of three test keys,
// and in `test_bld_key_slot.c` for a `key` one that keeps its floors in the slot, not in OTP.

// A G081 under its bootloader: 128kB of flash in pages of 2kB, as Forge sets it up
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64
#define BOOT_PAGES 6
#define BOOT_SLOT_PAGES 29
#define BOOT_CHIP 0x460
#define BOOT_BOARD_VARIANT 1 // a variant of its own, so a header left at `0` reads foreign

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "test.h"
// Host flash model and `pwr.h` first: their guards keep the chip headers of `bld.c` out
#include "flash.c"
#include "pwr.h"
#include "hal/stm32/sys/bld.c"

#if defined(BOOT_KEYS)
  #include "data/forge.h"
#endif

#define APP_ADDR (FLASH_BASE + BOOT_PAGES * FLASH_PAGE_SIZE)
#define STAGING_ADDR (APP_ADDR + BLD_SLOT_SIZE)
#define LINE_SIZE 200u     // image bytes in one `boot data` line, out of step with the pages
#define OTHER_CHIP 0x467u  // G0C1, which shares the G081 bootloader and its slot address
#define OTHER_BOARD 2u     // the next hardware, its binary another

// RAM below the image, where the bootloader leaves its report
BLD_Report_t _bld_report;

static uint8_t old_image[BLD_SLOT_SIZE];
static uint8_t new_image[BLD_SLOT_SIZE];

#if defined(BOOT_KEYS)

// Secret halves of the test keys, whose public halves `BOOT_KEYS` holds
static uint8_t test_secret[BLD_KEY_COUNT][64];

// Signature of an image in its own trailer, with the key its header names, as Forge writes it.
// For the bootloader of builds Forge signs with the development key, key `0` of the tests.
static void image_sign(uint8_t *image, uint32_t size)
{
  BLD_Header_t header;
  memcpy(&header, image + BLD_HEADER_OFFSET, sizeof(header));
  #if defined(BOOT_DEV_KEY)
  const uint8_t *secret = test_secret[0];
  #else
  const uint8_t *secret = test_secret[header.key < BLD_KEY_COUNT ? header.key : 0];
  #endif
  crypto_ed25519_sign(image + size + BLD_SIGNATURE_OFFSET, secret, image, size);
}

#endif

// Image laid out as linked: code, the header at `0x200`, the trailer erased or signed.
// Returns its CRC32.
static uint32_t image_make(uint8_t *image, uint32_t size, uint8_t seed)
{
  memset(image, 0xFF, size + BLD_TRAILER_SIZE);
  for(uint32_t i = 0; i < size; i++) image[i] = (uint8_t)(i * 7u + seed);
  BLD_Header_t header = {
    .magic = BLD_MAGIC, .size = size, .origin = APP_ADDR, .chip = BOOT_CHIP,
    .board = BOOT_BOARD_VARIANT, .epoch = PRO_SECURITY_EPOCH, .key = PRO_SIGNING_KEY,
  };
  memcpy(image + BLD_HEADER_OFFSET, &header, sizeof(header));
  #if defined(BOOT_KEYS)
  image_sign(image, size);
  #endif
  return CRC_Run(&crc32_iso, image, size);
}

// Header field `at` bytes into the header, `width` bytes wide, set to `value`, signed anew.
// Returns the new CRC32.
static uint32_t image_patch(uint8_t *image, uint32_t size, size_t at, size_t width,
  uint32_t value)
{
  memcpy(image + BLD_HEADER_OFFSET + at, &value, width);
  #if defined(BOOT_KEYS)
  image_sign(image, size);
  #endif
  return CRC_Run(&crc32_iso, image, size);
}

// Image flashed by a programmer as `-dist.hex` holds it: the CRC32 in front of its trailer
static void slot_program(const uint8_t *image, uint32_t size, uint32_t crc)
{
  static uint8_t flashed[BLD_SLOT_SIZE];
  memcpy(flashed, image, size + BLD_TRAILER_SIZE);
  memcpy(flashed + size, &crc, sizeof(crc));
  FLASH_HostProgram(APP_ADDR, flashed, size + BLD_TRAILER_SIZE);
}

// Image sent the way the console sends it, line by line, with the signature of its trailer
static status_t transfer(const uint8_t *image, uint32_t size, uint32_t crc)
{
  if(BLD_Begin(size, crc)) return ERR;
  if(BLD_Signature(image + size + BLD_SIGNATURE_OFFSET)) return ERR;
  for(uint32_t at = 0; at < size; at += LINE_SIZE) {
    uint16_t len = (uint16_t)minv(size - at, LINE_SIZE);
    if(BLD_Write(at, image + at, len)) return ERR;
  }
  return BLD_End();
}

// Image and its request put straight into flash, for a size no transfer takes:
// the header goes along with one that ends before it.
static void stage_unchecked(const uint8_t *image, uint32_t size, uint32_t crc)
{
  static uint8_t staged[BLD_SLOT_SIZE];
  uint32_t len = maxv(size + BLD_TRAILER_SIZE, BLD_HEADER_OFFSET + sizeof(BLD_Header_t));
  memcpy(staged, image, len);
  memcpy(staged + size, &crc, sizeof(crc));
  FLASH_HostProgram(STAGING_ADDR, staged, len);
  PDB_Delete(&mailbox);
  mailbox_append(BLD_Result_None, BLD_STAGING_PAGE, crc);
}

// CRC32 of the whole image in the application slot, `0` when there is none
static uint32_t slot_crc(void)
{
  uint32_t crc;
  return BLD_ImageCheck(BLD_APP_PAGE, BOOT_SLOT_PAGES, NULL, &crc) ? 0 : crc;
}

// Newest mailbox entry is a request for the image of `crc`
static bool request_waits(uint32_t crc)
{
  BLD_Entry_t entry;
  return mailbox_newest(&entry) && entry.result == BLD_Result_None && entry.crc == crc &&
    entry.page == BLD_STAGING_PAGE;
}

// Result of the newest mailbox entry, `-1` without one
static int32_t mailbox_result(void)
{
  BLD_Entry_t entry;
  return mailbox_newest(&entry) ? entry.result : -1;
}

// Bootloader start over a cleared report. Returns the report it leaves, `-1` for none.
static int32_t start(uint32_t *app)
{
  memset(&_bld_report, 0, sizeof(_bld_report));
  uint32_t addr = BLD_Install();
  if(app) *app = addr;
  return _bld_report.magic == BLD_REPORT_MAGIC ? (int32_t)_bld_report.result : -1;
}

// Status the way a fresh image reads it, nothing kept from a read before
static void status_read(BLD_Status_t *status)
{
  facts.known = false;
  BLD_Status(status);
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
    FLASH_HostBlank();
    uint32_t crc = image_make(new_image, size, 3);
    TEST_Check(transfer(new_image, size, crc) == OK, "size:%u", size);
    uint8_t trailer[BLD_TRAILER_SIZE];
    memcpy(trailer, new_image + size, sizeof(trailer));
    memcpy(trailer, &crc, sizeof(crc));
    TEST_Check(!memcmp(FLASH_Ref(STAGING_ADDR + size), trailer, sizeof(trailer)), "size:%u", size);
    TEST_Check(request_waits(crc), "size:%u", size);
    TEST_Check(BLD_Install() == APP_ADDR, "size:%u", size);
    uint32_t copied = size + BLD_TRAILER_SIZE;
    TEST_Check(!memcmp(FLASH_Ref(APP_ADDR), FLASH_Ref(STAGING_ADDR), copied), "size:%u", size);
    TEST_Check(mailbox_result() == BLD_Result_Installed, "size:%u", size);
  }
}

static void install_staged(void) { BLD_Install(); }

// An install cut anywhere and repeated on the next start ends with the new image.
// Under `key` the new image raises the epoch, so the cuts land in its OTP word as well.
static void install_survives_a_cut(void)
{
  uint32_t old_size = 3000, new_size = 5000; // three pages: each cut costs a whole install
  FLASH_HostBlank();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  image_make(new_image, new_size, 2);
  uint32_t new_crc = image_patch(new_image, new_size, offsetof(BLD_Header_t, epoch), 2, 1);
  slot_program(old_image, old_size, old_crc);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged");
  FLASH_HostSave();
  uint32_t before = FLASH_HostCycles();
  install_staged();
  uint32_t total = FLASH_HostCycles() - before;
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Late; where++) {
    for(uint32_t cut = 0; cut <= total; cut++) {
      FLASH_HostRestore();
      FLASH_HostCut((int32_t)cut, where, install_staged);
      uint32_t crc = slot_crc();
      TEST_Check(crc == 0 || crc == old_crc || crc == new_crc, "where:%d cut:%u", where, cut);
      bool waits = request_waits(new_crc);
      uint32_t app;
      int32_t result = start(&app);
      TEST_Check(app == APP_ADDR && slot_crc() == new_crc, "where:%d cut:%u", where, cut);
      // A request the cut left waiting makes this start finish the install and report it
      int32_t expected = waits ? BLD_Result_Installed : BLD_Result_None;
      TEST_Check(result == expected, "where:%d cut:%u", where, cut);
      TEST_Check(mailbox_result() == BLD_Result_Installed, "where:%d cut:%u", where, cut);
      #if defined(BOOT_KEYS)
      TEST_Check(floors_get().epoch == 1, "where:%d cut:%u", where, cut);
      #endif
    }
  }
}

// An install whose result a power cut took is answered on the next start without a second copy:
// fewer flash cycles than the image has pages.
static void held_image_is_not_copied_again(void)
{
  uint32_t size = 21000;
  FLASH_HostBlank();
  uint32_t crc = image_make(new_image, size, 2);
  TEST_Check(transfer(new_image, size, crc) == OK && start(NULL) == BLD_Result_Installed,
    "installed");
  PDB_Delete(&mailbox);
  mailbox_append(BLD_Result_None, BLD_STAGING_PAGE, crc); // the request, its result gone
  uint32_t before = FLASH_HostCycles();
  TEST_Check(start(NULL) == BLD_Result_Installed, "answered");
  TEST_Check(FLASH_HostCycles() - before < size / FLASH_PAGE_SIZE, "not copied again");
}

// Image `transfer_new` sends: `new_image` of this size, with this CRC
static uint32_t sent_size, sent_crc;
static void transfer_new(void) { transfer(new_image, sent_size, sent_crc); }

// A transfer cut anywhere leaves the old image running, or the new one once the request stands
static void transfer_survives_a_cut(void)
{
  uint32_t old_size = 3000;
  sent_size = 5000; // three pages: each cut costs a whole install
  FLASH_HostBlank();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  sent_crc = image_make(new_image, sent_size, 2);
  slot_program(old_image, old_size, old_crc);
  FLASH_HostSave();
  uint32_t before = FLASH_HostCycles();
  transfer_new();
  uint32_t total = FLASH_HostCycles() - before;
  for(FLASH_Cut_t where = FLASH_Cut_Before; where <= FLASH_Cut_Late; where++) {
    for(uint32_t cut = 0; cut <= total; cut++) {
      FLASH_HostRestore();
      FLASH_HostCut((int32_t)cut, where, transfer_new);
      bool waits = request_waits(sent_crc);
      TEST_Check(BLD_Install() == APP_ADDR, "where:%d cut:%u", where, cut);
      TEST_Check(slot_crc() == (waits ? sent_crc : old_crc), "where:%d cut:%u", where, cut);
    }
  }
}

// Ones a programmer writes over the mailbox block nothing: `BLD_End` formats the page whole
static void programmer_ones_do_not_block_the_mailbox(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  uint32_t crc = image_make(new_image, size, 2);
  static uint8_t ones[FLASH_PAGE_SIZE];
  memset(ones, 0xFF, sizeof(ones));
  FLASH_HostProgram(FLASH_GetAddress(BLD_MAILBOX_PAGE, 0), ones, sizeof(ones));
  TEST_Check(transfer(new_image, size, crc) == OK, "request over written ones");
  TEST_Check(request_waits(crc), "request over written ones");
}

// Bytes come in order and up to the size: a gap or an overrun closes the transfer,
// as an abort does, and a short image never becomes a request.
static void transfer_takes_bytes_in_order(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  uint32_t crc = image_make(new_image, size, 2);
  TEST_Check(BLD_Begin(size, crc) == OK && BLD_Write(0, new_image, LINE_SIZE) == OK, "first");
  TEST_Check(BLD_Write(2 * LINE_SIZE, new_image + 2 * LINE_SIZE, LINE_SIZE) == ERR, "gap");
  TEST_Check(BLD_Write(LINE_SIZE, new_image + LINE_SIZE, LINE_SIZE) == ERR, "closed by the gap");
  TEST_Check(BLD_Begin(size, crc) == OK, "overrun");
  TEST_Check(BLD_Write(0, new_image, (uint16_t)(size + 4)) == ERR, "overrun");
  TEST_Check(BLD_Write(0, new_image, LINE_SIZE) == ERR, "closed by the overrun");
  TEST_Check(BLD_Begin(size, crc) == OK && BLD_Write(0, new_image, LINE_SIZE) == OK, "short");
  TEST_Check(BLD_End() == ERR && mailbox_result() == -1, "short");
  TEST_Check(BLD_Begin(size, crc) == OK, "aborted");
  BLD_Abort();
  TEST_Check(BLD_Write(0, new_image, LINE_SIZE) == ERR, "aborted");
}

// A transfer opens for an image that runs past its header and fits the slot with its trailer.
// The largest one installs whole, its trailer the last bytes of the slot.
static void largest_image_fills_the_slot(void)
{
  uint32_t largest = BLD_SLOT_SIZE - BLD_TRAILER_SIZE;
  TEST_Check(BLD_Begin(largest + 1u, 0) == ERR, "past the slot");
  TEST_Check(BLD_Begin(BLD_HEADER_OFFSET + sizeof(BLD_Header_t), 0) == ERR, "header alone");
  FLASH_HostBlank();
  uint32_t crc = image_make(new_image, largest, 2);
  TEST_Check(transfer(new_image, largest, crc) == OK && request_waits(crc), "largest staged");
  TEST_Check(start(NULL) == BLD_Result_Installed && slot_crc() == crc, "largest installed");
}

// The application judges no image: one for another slot, chip or board, or of another size
// than the transfer, goes in whole and becomes a request, and the bootloader refuses it at start.
// The old image keeps running, the result answers the request, and the report says why.
static void bootloader_refuses_a_foreign_image(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  const struct { size_t at, width; uint32_t value; BLD_Result_t result; } cases[] = {
    { offsetof(BLD_Header_t, origin), 4, STAGING_ADDR, BLD_Result_Origin },
    { offsetof(BLD_Header_t, chip),   2, OTHER_CHIP,   BLD_Result_Chip   },
    { offsetof(BLD_Header_t, board),  2, OTHER_BOARD,  BLD_Result_Board  },
    { offsetof(BLD_Header_t, size),   4, new_size - 4, BLD_Result_Crc    },
  };
  for(uint32_t n = 0; n < array_len(cases); n++) {
    FLASH_HostBlank();
    uint32_t old_crc = image_make(old_image, old_size, 1);
    slot_program(old_image, old_size, old_crc);
    image_make(new_image, new_size, 2);
    uint32_t crc = image_patch(new_image, new_size, cases[n].at, cases[n].width, cases[n].value);
    TEST_Check(transfer(new_image, new_size, crc) == OK && request_waits(crc),
      "case:%u staged", n);
    uint32_t app;
    TEST_Check(start(&app) == (int32_t)cases[n].result, "case:%u reported", n);
    TEST_Check(app == APP_ADDR && slot_crc() == old_crc, "case:%u old image runs", n);
    TEST_Check(mailbox_result() == (int32_t)cases[n].result, "case:%u answered", n);
    TEST_Check(start(NULL) == BLD_Result_None, "case:%u next start", n);
  }
}

// The bytes come before the header fields: a broken image reads broken, never foreign
static void broken_image_reads_broken(void)
{
  uint32_t size = 21000;
  FLASH_HostBlank();
  image_make(new_image, size, 2);
  uint32_t crc = image_patch(new_image, size, offsetof(BLD_Header_t, chip), 2, OTHER_CHIP);
  new_image[size / 2] ^= 0x01;
  TEST_Check(transfer(new_image, size, crc) == OK, "staged");
  TEST_Check(start(NULL) == BLD_Result_Crc, "broken before foreign");
}

// A size past the slot is told apart from a broken image, before a byte past it is read
static void oversize_image_is_refused_as_size(void)
{
  uint32_t size = 21000;
  FLASH_HostBlank();
  image_make(new_image, size, 2);
  uint32_t crc = image_patch(new_image, size, offsetof(BLD_Header_t, size), 4, BLD_SLOT_SIZE);
  TEST_Check(transfer(new_image, size, crc) == OK, "staged");
  TEST_Check(start(NULL) == BLD_Result_Size, "size");
}

// A header without its mark reads broken, as does a size that leaves the header out,
// so that neither the CRC32 nor a signature covers it,
// or one off the word grid the running image reads its trailer from.
static void malformed_header_reads_broken(void)
{
  const struct { uint32_t size, magic; } cases[] = {
    { 9000, ~BLD_MAGIC }, { BLD_HEADER_OFFSET / 2, BLD_MAGIC }, { 9002, BLD_MAGIC },
  };
  for(uint32_t n = 0; n < array_len(cases); n++) {
    FLASH_HostBlank();
    memset(new_image, 0xFF, sizeof(new_image));
    image_make(new_image, cases[n].size, 2);
    uint32_t crc = image_patch(new_image, cases[n].size, offsetof(BLD_Header_t, magic), 4,
      cases[n].magic);
    stage_unchecked(new_image, cases[n].size, crc);
    TEST_Check(start(NULL) == BLD_Result_Crc, "case:%u", n);
  }
}

// A request is refused when it names a staging slot past the chip, on the application slot
// or running past the chip, the image it names whole on that page.
static void request_names_no_slot(void)
{
  const uint16_t pages[] = { FLASH_PAGES, BLD_APP_PAGE, BLD_STAGING_PAGE + 1 };
  uint32_t size = 9000;
  for(uint32_t n = 0; n < array_len(pages); n++) {
    FLASH_HostBlank();
    uint32_t crc = image_make(new_image, size, 2);
    memcpy(new_image + size, &crc, sizeof(crc));
    if(pages[n] < FLASH_PAGES) {
      FLASH_HostProgram(FLASH_GetAddress(pages[n], 0), new_image, size + BLD_TRAILER_SIZE);
    }
    PDB_Delete(&mailbox);
    mailbox_append(BLD_Result_None, pages[n], crc);
    TEST_Check(start(NULL) == BLD_Result_Crc, "page:%u", pages[n]);
    TEST_Check(mailbox_result() == BLD_Result_Crc, "page:%u", pages[n]);
  }
}

// A request names its image by the CRC32: another image in the staging slot,
// as a transfer cut before its own request leaves it, is refused and the old image keeps running.
// The CRC32 comes before any header field, so a foreign one reads as another image too.
static void request_names_its_image(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  const uint32_t boards[] = { BOOT_BOARD_VARIANT, OTHER_BOARD }; // its own, then a foreign one
  for(uint32_t n = 0; n < array_len(boards); n++) {
    FLASH_HostBlank();
    uint32_t old_crc = image_make(old_image, old_size, 1);
    slot_program(old_image, old_size, old_crc);
    image_make(new_image, new_size, 2);
    uint32_t crc = image_patch(new_image, new_size, offsetof(BLD_Header_t, board), 2, boards[n]);
    TEST_Check(transfer(new_image, new_size, crc) == OK, "board:%u staged", boards[n]);
    PDB_Delete(&mailbox);
    mailbox_append(BLD_Result_None, BLD_STAGING_PAGE, crc ^ 1u); // CRC32 of another image
    uint32_t app;
    TEST_Check(start(&app) == BLD_Result_Crc, "board:%u another image", boards[n]);
    TEST_Check(app == APP_ADDR && slot_crc() == old_crc, "board:%u old image runs", boards[n]);
  }
}

// Every start reports, so no result outlives the start that made it
static void every_start_reports(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  FLASH_HostBlank();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  slot_program(old_image, old_size, old_crc);
  TEST_Check(start(NULL) == BLD_Result_None, "nothing waits");
  uint32_t new_crc = image_make(new_image, new_size, 2);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged");
  TEST_Check(start(NULL) == BLD_Result_Installed, "installed");
  TEST_Check(start(NULL) == BLD_Result_None, "next start");
  FLASH_HostBlank();
  TEST_Check(start(NULL) == BLD_Result_None, "empty slot");
}

// The trailer has to match the bytes: an image with its CRC32 still erased never starts
static void erased_crc_never_starts(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  image_make(old_image, size, 1);
  FLASH_HostProgram(APP_ADDR, old_image, size + BLD_TRAILER_SIZE);
  uint32_t app;
  start(&app);
  TEST_Check(app == 0, "erased trailer");
}

// The application reads what became of its last request until the next one,
// and which bootloader it runs under.
static void status_tells_the_last_result(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  FLASH_HostBlank();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  slot_program(old_image, old_size, old_crc);
  BLD_Status_t status;
  status_read(&status);
  TEST_Check(status.boot && status.last == BLD_Result_None, "empty mailbox");
  uint32_t code = BLD_MAILBOX_PAGE * FLASH_PAGE_SIZE;
  TEST_Check(status.loader == CRC_Run(&crc32_iso, FLASH_Ref(FLASH_BASE), code), "loader");
  uint32_t new_crc = image_make(new_image, new_size, 2);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged");
  status_read(&status);
  TEST_Check(status.last == BLD_Result_None, "waits");
  start(NULL);
  status_read(&status);
  TEST_Check(status.last == BLD_Result_Installed, "installed");
  TEST_Check(status.result == BLD_Result_Installed, "reported");
  start(NULL);
  status_read(&status);
  TEST_Check(status.last == BLD_Result_Installed, "kept till the next update");
  TEST_Check(status.result == BLD_Result_None, "the report is of one start");
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged again");
  BLD_Status(&status);
  TEST_Check(status.last == BLD_Result_None, "the next request reads at once");
  image_make(old_image, old_size, 1);
  uint32_t crc = image_patch(old_image, old_size, offsetof(BLD_Header_t, board), 2, OTHER_BOARD);
  TEST_Check(transfer(old_image, old_size, crc) == OK, "foreign staged");
  start(NULL);
  status_read(&status);
  TEST_Check(status.last == BLD_Result_Board && status.result == BLD_Result_Board, "refused");
}

//-------------------------------------------------------------------------------------- Plain mode
#if !defined(BOOT_KEYS)

// A signature given to the transfer lands behind the CRC32 and goes into the slot with the image
static void signature_rides_in_the_trailer(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  uint32_t crc = image_make(new_image, size, 2);
  uint8_t *signature = new_image + size + BLD_SIGNATURE_OFFSET;
  for(uint32_t i = 0; i < BLD_SIGNATURE_SIZE; i++) signature[i] = (uint8_t)(i + 1);
  TEST_Check(BLD_Signature(signature) == ERR, "no transfer open");
  TEST_Check(transfer(new_image, size, crc) == OK, "staged");
  uint32_t at = size + BLD_SIGNATURE_OFFSET;
  TEST_Check(!memcmp(FLASH_Ref(STAGING_ADDR + at), signature, BLD_SIGNATURE_SIZE), "staged");
  TEST_Check(BLD_Install() == APP_ADDR, "installed");
  TEST_Check(!memcmp(FLASH_Ref(APP_ADDR + at), signature, BLD_SIGNATURE_SIZE), "installed");
}

#endif
//---------------------------------------------------------------------------------------- Key mode
#if defined(BOOT_KEYS)

// Signature of an image in its own trailer with a key the bootloader does not hold
static void image_sign_foreign(uint8_t *image, uint32_t size)
{
  uint8_t seed[32] = { 9 }, secret[64], key[BLD_KEY_SIZE];
  crypto_ed25519_key_pair(secret, key, seed);
  crypto_ed25519_sign(image + size + BLD_SIGNATURE_OFFSET, secret, image, size);
}

// An image signed with another key installs nowhere: the report says why
static void foreign_signature_never_installs(void)
{
  uint32_t old_size = 9000, new_size = 21000;
  FLASH_HostBlank();
  uint32_t old_crc = image_make(old_image, old_size, 1);
  slot_program(old_image, old_size, old_crc);
  uint32_t new_crc = image_make(new_image, new_size, 2);
  image_sign_foreign(new_image, new_size);
  TEST_Check(transfer(new_image, new_size, new_crc) == OK, "staged: an application holds no key");
  uint32_t app;
  TEST_Check(start(&app) == BLD_Result_Signature, "reported");
  TEST_Check(app == APP_ADDR && slot_crc() == old_crc, "old image runs");
  TEST_Check(mailbox_result() == BLD_Result_Signature, "answered");
}

// A byte changed under a matching CRC32 still breaks the signature: the image never starts
static void changed_byte_never_starts(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  image_make(old_image, size, 1);
  old_image[size / 2] ^= 0x01;
  slot_program(old_image, size, CRC_Run(&crc32_iso, old_image, size));
  uint32_t app;
  start(&app);
  TEST_Check(app == 0, "changed byte");
}

// An installed image raises the epoch floor: the bootloader refuses an older image,
// which raises no floor even with a higher key, and an equal one goes in.
static void epoch_floor_holds(void)
{
  uint32_t old_size = 9000, new_size = 21000, at = offsetof(BLD_Header_t, epoch);
  FLASH_HostBlank();
  image_make(new_image, new_size, 2);
  uint32_t crc = image_patch(new_image, new_size, at, 2, 2);
  TEST_Check(transfer(new_image, new_size, crc) == OK && start(NULL) == BLD_Result_Installed,
    "epoch 2");
  TEST_Check(floors_get().epoch == 2, "floor");
  image_make(old_image, old_size, 1);
  image_patch(old_image, old_size, offsetof(BLD_Header_t, key), 2, 1);
  uint32_t older = image_patch(old_image, old_size, at, 2, 1);
  TEST_Check(transfer(old_image, old_size, older) == OK, "older staged");
  TEST_Check(start(NULL) == BLD_Result_Epoch && slot_crc() == crc, "older refused");
  TEST_Check(floors_get().key == 0, "no key floor from the refused image");
  uint32_t equal = image_patch(old_image, old_size, at, 2, 2);
  TEST_Check(transfer(old_image, old_size, equal) == OK, "equal epoch");
  TEST_Check(start(NULL) == BLD_Result_Installed && slot_crc() == equal, "equal epoch");
}

// An image signed with a higher key revokes the keys below it:
// from then on an image of a lower key neither installs nor starts, one of a higher key does.
static void higher_key_revokes_lower(void)
{
  uint32_t size = 9000, at = offsetof(BLD_Header_t, key);
  FLASH_HostBlank();
  image_make(new_image, size, 2);
  uint32_t crc = image_patch(new_image, size, at, 2, 1);
  TEST_Check(transfer(new_image, size, crc) == OK && start(NULL) == BLD_Result_Installed,
    "key 1");
  TEST_Check(floors_get().key == 1, "key floor");
  uint32_t lower = image_make(old_image, size, 3);
  TEST_Check(transfer(old_image, size, lower) == OK, "key 0 staged");
  TEST_Check(start(NULL) == BLD_Result_Signature, "key 0 refused");
  uint32_t app;
  #if(BOOT_OTP_WORDS)
  // A floor in OTP outlives the image: one of a lower key a programmer writes never starts
  slot_program(old_image, size, lower);
  start(&app);
  TEST_Check(app == 0, "key 0 never starts");
  #endif
  uint32_t higher = image_patch(old_image, size, at, 2, 2);
  TEST_Check(transfer(old_image, size, higher) == OK, "key 2");
  TEST_Check(start(&app) == BLD_Result_Installed && app == APP_ADDR, "key 2");
  TEST_Check(floors_get().key == 2, "key floor raised again");
}

// A key past the keys of the bootloader signs nothing.
// Far past the keys a key would lie outside `BOOT_KEYS`, so nothing reads it there.
static void missing_key_signs_nothing(void)
{
  uint32_t size = 9000, at = offsetof(BLD_Header_t, key);
  const uint32_t past[] = { BLD_KEY_COUNT, 0xFFFF };
  for(uint32_t n = 0; n < array_len(past); n++) {
    FLASH_HostBlank();
    image_make(old_image, size, 1);
    uint32_t crc = image_patch(old_image, size, at, 2, past[n]);
    TEST_Check(transfer(old_image, size, crc) == OK, "key:%u staged", past[n]);
    TEST_Check(start(NULL) == BLD_Result_Signature, "key:%u", past[n]);
  }
}

#if defined(BOOT_DEV_KEY)

// The bootloader of builds takes the development key for every number `BOOT_KEYS` has,
// refuses a product key and keeps its floors in the slot, whatever `BOOT_OTP_WORDS` said.
static void dev_key_stands_for_every_key(void)
{
  uint32_t size = 9000, at = offsetof(BLD_Header_t, key);
  FLASH_HostBlank();
  image_make(new_image, size, 2);
  uint32_t crc = image_patch(new_image, size, at, 2, 2);
  TEST_Check(transfer(new_image, size, crc) == OK && start(NULL) == BLD_Result_Installed,
    "key 2 signed with the development key");
  crypto_ed25519_sign(new_image + size + BLD_SIGNATURE_OFFSET, test_secret[2], new_image, size);
  TEST_Check(transfer(new_image, size, crc) == OK && start(NULL) == BLD_Result_Signature,
    "key 2 signed with itself");
  TEST_Check(BOOT_OTP_WORDS == 0, "floors in the slot");
}

#endif
#if(BOOT_OTP_WORDS)

// OTP word `index` of the bootloader written as a test sets it up
static void otp_program(uint32_t index, uint16_t epoch, uint16_t key, bool valid)
{
  otp_word_t word = { epoch, key, 0 };
  word.crc = otp_crc(&word) ^ (valid ? 0u : 1u);
  FLASH_HostProgram(otp_addr(index), (const uint8_t *)&word, sizeof(word));
}

// OTP words of the bootloader that are no longer blank
static uint32_t otp_taken(void)
{
  uint32_t taken = 0;
  for(uint32_t i = 0; i < BOOT_OTP_WORDS; i++) {
    otp_word_t word;
    taken += !otp_read(i, &word) || !erased((const uint8_t *)&word, sizeof(word));
  }
  return taken;
}

// A word failing its check, one ECC corrects or cannot read, as a power cut leaves them,
// holds no floor; the raise passes them by, and ones a programmer wrote, which take no program.
static void bad_otp_words_hold_no_floor(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  otp_program(0, 7, 2, false);
  otp_program(1, 3, 1, true);
  FLASH_HostAge(otp_addr(1), 1);
  otp_program(2, 4, 1, true);
  FLASH_HostAge(otp_addr(2), 2);
  uint8_t ones[8];
  memset(ones, 0xFF, sizeof(ones));
  FLASH_HostProgram(otp_addr(3), ones, sizeof(ones));
  TEST_Check(floors_read().epoch == 0 && floors_read().key == 0, "no floor");
  image_make(new_image, size, 2);
  uint32_t crc = image_patch(new_image, size, offsetof(BLD_Header_t, epoch), 2, 1);
  TEST_Check(transfer(new_image, size, crc) == OK && start(NULL) == BLD_Result_Installed,
    "installed");
  otp_word_t word;
  TEST_Check(otp_read(4, &word) && word.epoch == 1 && floors_read().epoch == 1, "next free word");
}

// With every word taken the floors stay where they are, the highest word holding wherever it lies,
// and the image still installs.
static void full_otp_keeps_the_floors(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  for(uint32_t i = 0; i < BOOT_OTP_WORDS; i++) otp_program(i, i ? 1 : 5, i ? 0 : 1, true);
  image_make(new_image, size, 2);
  image_patch(new_image, size, offsetof(BLD_Header_t, key), 2, 1);
  uint32_t crc = image_patch(new_image, size, offsetof(BLD_Header_t, epoch), 2, 6);
  TEST_Check(transfer(new_image, size, crc) == OK, "staged");
  uint32_t app;
  TEST_Check(start(&app) == BLD_Result_Installed && app == APP_ADDR, "installed");
  floors_t floors = floors_read();
  TEST_Check(floors.epoch == 5 && floors.key == 1, "floors unchanged");
  TEST_Check(_bld_report.epoch == 5 && _bld_report.key == 1, "the report tells what OTP keeps");
}

// An image signed with another key raises no floor, however high its epoch and key,
// whether a request names it or a programmer wrote it.
static void foreign_image_raises_no_floor(void)
{
  uint32_t size = 9000;
  FLASH_HostBlank();
  image_make(new_image, size, 2);
  image_patch(new_image, size, offsetof(BLD_Header_t, key), 2, 1);
  uint32_t crc = image_patch(new_image, size, offsetof(BLD_Header_t, epoch), 2, 5);
  image_sign_foreign(new_image, size);
  TEST_Check(transfer(new_image, size, crc) == OK && start(NULL) == BLD_Result_Signature,
    "request refused");
  floors_t floors = floors_read();
  TEST_Check(floors.epoch == 0 && floors.key == 0, "no floor from the request");
  slot_program(new_image, size, crc);
  uint32_t app;
  start(&app);
  floors = floors_read();
  TEST_Check(app == 0 && floors.epoch == 0 && floors.key == 0, "no floor from the slot");
}

#endif

// An image a programmer wrote past the floors raises them at its first start, and only then:
// a board fresh from the factory takes no older image and no revoked key.
// An install takes one word of OTP, the start after it none.
static void flashed_image_raises_the_floors(void)
{
  uint32_t size = 9000;
  size_t epoch_at = offsetof(BLD_Header_t, epoch), key_at = offsetof(BLD_Header_t, key);
  FLASH_HostBlank();
  image_make(old_image, size, 1);
  image_patch(old_image, size, key_at, 2, 1);
  uint32_t crc = image_patch(old_image, size, epoch_at, 2, 2);
  slot_program(old_image, size, crc);
  uint32_t app;
  TEST_Check(start(&app) == BLD_Result_None && app == APP_ADDR, "flashed image starts");
  floors_t floors = floors_get();
  TEST_Check(floors.epoch == 2 && floors.key == 1, "floors of the flashed image");
  BLD_Status_t status;
  status_read(&status);
  TEST_Check(status.epoch == 2 && status.key == 1, "the image reads the floors in the report");
  #if(BOOT_OTP_WORDS)
  start(NULL);
  TEST_Check(otp_taken() == 1, "one word, the next start takes none");
  #endif
  image_make(new_image, size, 2);
  image_patch(new_image, size, key_at, 2, 1);
  uint32_t older = image_patch(new_image, size, epoch_at, 2, 1);
  TEST_Check(transfer(new_image, size, older) == OK && start(NULL) == BLD_Result_Epoch,
    "older epoch refused");
  image_patch(new_image, size, epoch_at, 2, 2);
  uint32_t revoked = image_patch(new_image, size, key_at, 2, 0);
  TEST_Check(transfer(new_image, size, revoked) == OK && start(NULL) == BLD_Result_Signature,
    "revoked key refused");
  image_patch(new_image, size, key_at, 2, 1);
  uint32_t higher = image_patch(new_image, size, epoch_at, 2, 3);
  TEST_Check(transfer(new_image, size, higher) == OK && start(NULL) == BLD_Result_Installed,
    "higher epoch installed");
  TEST_Check(floors_get().epoch == 3 && _bld_report.epoch == 3, "floor of the install");
  #if(BOOT_OTP_WORDS)
  TEST_Check(otp_taken() == 2, "the install takes one word, its start none");
  #endif
}

// Monocypher signs as Forge does: the same key from the seed, the same signature on the image
static void monocypher_signs_as_forge(void)
{
  uint8_t seed[32], secret[64], key[BLD_KEY_SIZE], signature[BLD_SIGNATURE_SIZE];
  memcpy(seed, FORGE_KEY_SEEDS[0], sizeof(seed));
  crypto_ed25519_key_pair(secret, key, seed);
  TEST_Check(!memcmp(key, FORGE_KEY, sizeof(key)), "key");
  image_make(new_image, FORGE_SIZE, FORGE_IMAGE_SEED);
  crypto_ed25519_sign(signature, secret, new_image, FORGE_SIZE);
  TEST_Check(!memcmp(signature, FORGE_SIGNATURE, sizeof(signature)), "signature");
}

// What Forge signs, the bootloader starts
static void forge_signature_starts(void)
{
  FLASH_HostBlank();
  uint32_t crc = image_make(new_image, FORGE_SIZE, FORGE_IMAGE_SEED);
  memcpy(new_image + FORGE_SIZE + BLD_SIGNATURE_OFFSET, FORGE_SIGNATURE, BLD_SIGNATURE_SIZE);
  slot_program(new_image, FORGE_SIZE, crc);
  uint32_t app;
  start(&app);
  TEST_Check(app == APP_ADDR, "started");
}

#endif
//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  #if defined(BOOT_KEYS)
  // Monocypher wipes the seed it takes, so each goes in as a copy
  for(uint32_t i = 0; i < BLD_KEY_COUNT; i++) {
    uint8_t seed[32], key[BLD_KEY_SIZE];
    memcpy(seed, FORGE_KEY_SEEDS[i], sizeof(seed));
    crypto_ed25519_key_pair(test_secret[i], key, seed);
  }
  #endif
  TEST(trailer_crosses_pages);
  TEST(install_survives_a_cut);
  TEST(held_image_is_not_copied_again);
  TEST(transfer_survives_a_cut);
  TEST(programmer_ones_do_not_block_the_mailbox);
  TEST(transfer_takes_bytes_in_order);
  TEST(largest_image_fills_the_slot);
  TEST(bootloader_refuses_a_foreign_image);
  TEST(broken_image_reads_broken);
  TEST(oversize_image_is_refused_as_size);
  TEST(malformed_header_reads_broken);
  TEST(request_names_no_slot);
  TEST(request_names_its_image);
  TEST(every_start_reports);
  TEST(erased_crc_never_starts);
  TEST(status_tells_the_last_result);
  #if !defined(BOOT_KEYS)
  TEST(signature_rides_in_the_trailer);
  #else
  TEST(foreign_signature_never_installs);
  TEST(changed_byte_never_starts);
  TEST(epoch_floor_holds);
  TEST(higher_key_revokes_lower);
  TEST(missing_key_signs_nothing);
  #if defined(BOOT_DEV_KEY)
  TEST(dev_key_stands_for_every_key);
  #endif
  #if(BOOT_OTP_WORDS)
  TEST(bad_otp_words_hold_no_floor);
  TEST(full_otp_keeps_the_floors);
  TEST(foreign_image_raises_no_floor);
  #endif
  TEST(flashed_image_raises_the_floors);
  TEST(monocypher_signs_as_forge);
  TEST(forge_signature_starts);
  #endif
}

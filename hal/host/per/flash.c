// hal/host/per/flash.c

#include "flash.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(HOST_TEST)
  #if defined(_WIN32) || defined(_WIN64)
    #include <direct.h>
    #define mkdir_p(path) _mkdir(path)
  #else
    #include <sys/stat.h>
    #define mkdir_p(path) mkdir(path, 0755)
  #endif
#endif

// Records run the code of the chip, over this model
#include "../../stm32/per/flash_record.c"

//---------------------------------------------------------------------------------------- Internal

#define FLASH_SIZE ((uint32_t)FLASH_PAGES * FLASH_PAGE_SIZE)
#define FLASH_CELLS ((FLASH_SIZE + FLASH_OTP_SIZE) / 8) // doublewords of flash and OTP
#define FLASH_ROW 256u // bytes of one `FLASH_WriteFast`
#define FLASH_NOWHERE UINT32_MAX // no byte of the model

// The chip, `FLASH_BASE` at index 0, OTP behind the last page
typedef struct {
  uint8_t memory[FLASH_SIZE + FLASH_OTP_SIZE];
  bool written[FLASH_CELLS];    // doubleword written since its erase, as its ECC tells the chip
  bool damaged[FLASH_CELLS];    // ECC cannot read it: a cycle cut halfway, two aged bits
  bool corrected[FLASH_CELLS];  // ECC corrects it: a cycle cut halfway, one aged bit
  bool torn[FLASH_CELLS];       // of a cycle cut halfway: a plain read can fail it, as on WB55
  bool unstable[FLASH_CELLS];   // of a cycle cut halfway: reads erased and fails ECC by turns
  bool flipped[FLASH_CELLS];    // unstable, read erased the last time
  bool weak[FLASH_PAGES];       // erase cut late: reads erased, its cells short of a full erase
  int32_t erased_last;          // page of the last erase cycle, the only one a row goes into
} chip_t;

static chip_t chip;
static bool mounted;
static uint32_t spent;       // erase and program cycles done
static int32_t budget = -1;  // cycles left before the power goes, `-1` for never
static FLASH_Cut_t cut_at;   // where in its cycle the power goes
static jmp_buf power_loss;

static uint8_t *page_at(uint16_t page) { return chip.memory + (uint32_t)page * FLASH_PAGE_SIZE; }

// Byte of the model at `addr`, the `size` bytes from it all in flash or all in OTP,
// `FLASH_NOWHERE` otherwise.
static uint32_t index_of(uint32_t addr, uint32_t size)
{
  uint32_t at = addr - FLASH_BASE;
  if(at < FLASH_SIZE && size <= FLASH_SIZE - at) return at;
  at = addr - FLASH_OTP_BASE;
  if(at < FLASH_OTP_SIZE && size <= FLASH_OTP_SIZE - at) return FLASH_SIZE + at;
  return FLASH_NOWHERE;
}

// Byte `at` of the model on a page an erase cut late left short of a full erase; OTP never is
static bool page_weak(uint32_t at) { return at < FLASH_SIZE && chip.weak[at / FLASH_PAGE_SIZE]; }

static bool blank(const uint8_t *cell, uint32_t size)
{
  for(uint32_t i = 0; i < size; i++) {
    if(cell[i] != 0xFF) return false;
  }
  return true;
}

static bool zeros(const uint8_t *cell, uint32_t size)
{
  for(uint32_t i = 0; i < size; i++) {
    if(cell[i]) return false;
  }
  return true;
}

// Every page erased and nothing written, as from the factory
static void chip_blank(void)
{
  memset(chip.memory, 0xFF, sizeof(chip.memory));
  memset(chip.written, false, sizeof(chip.written));
  memset(chip.damaged, false, sizeof(chip.damaged));
  memset(chip.corrected, false, sizeof(chip.corrected));
  memset(chip.torn, false, sizeof(chip.torn));
  memset(chip.unstable, false, sizeof(chip.unstable));
  memset(chip.flipped, false, sizeof(chip.flipped));
  memset(chip.weak, false, sizeof(chip.weak));
  chip.erased_last = -1;
}

// Page erased whole: `0xFF` in every byte, every doubleword fresh
static void page_clean(uint16_t page)
{
  uint32_t cell = (uint32_t)page * FLASH_PAGE_SIZE / 8;
  uint32_t cells = FLASH_PAGE_SIZE / 8;
  memset(page_at(page), 0xFF, FLASH_PAGE_SIZE);
  memset(chip.written + cell, false, cells);
  memset(chip.damaged + cell, false, cells);
  memset(chip.corrected + cell, false, cells);
  memset(chip.torn + cell, false, cells);
  memset(chip.unstable + cell, false, cells);
  memset(chip.flipped + cell, false, cells);
  chip.weak[page] = false;
}

// Zeros programmed over a doubleword, its ECC zeros too: it reads clean, whatever it held
static void cell_zeroed(uint32_t cell)
{
  chip.damaged[cell] = false;
  chip.corrected[cell] = false;
  chip.torn[cell] = false;
  chip.unstable[cell] = false;
  chip.flipped[cell] = false;
}

#if defined(HOST_TEST)

// A test owns the chip in RAM: every program starts from a blank one and keeps no file
static void load(void) {}
static void store(uint16_t page) { unused(page); }

#else

static char flash_dir[256] = FLASH_DIR;

static void page_file(uint16_t page, char *name, size_t size) {
  snprintf(name, size, "%s/flash_page_%03u.bin", flash_dir, page);
}

// No file is an erased page, a file of another size is no page and stays out.
// The mark of a doubleword written with ones stays out too: it comes back fresh.
static void load(void)
{
  for(uint16_t page = 0; page < FLASH_PAGES; page++) {
    char name[280];
    page_file(page, name, sizeof(name));
    FILE *file = fopen(name, "rb");
    if(!file) continue;
    uint8_t *mem = page_at(page);
    size_t count = fread(mem, 1, FLASH_PAGE_SIZE, file);
    if(count != FLASH_PAGE_SIZE || fgetc(file) != EOF) memset(mem, 0xFF, FLASH_PAGE_SIZE);
    fclose(file);
    for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
      chip.written[((uint32_t)page * FLASH_PAGE_SIZE + i) / 8] = !blank(mem + i, 8);
    }
  }
}

// A change reaches its file before the call returns, so a crash leaves what a power loss would
static void store(uint16_t page)
{
  mkdir_p(flash_dir); // there already, fine
  char name[280];
  page_file(page, name, sizeof(name));
  FILE *file = fopen(name, "wb");
  if(!file) return;
  fwrite(page_at(page), 1, FLASH_PAGE_SIZE, file);
  fclose(file);
}

#endif

static void mount(void)
{
  if(mounted) return;
  mounted = true;
  chip_blank();
  load();
}

// One erase or program cycle: the power goes before it, or halfway through it as `cut_at` says.
// `true` for a cut halfway, which the caller lands once its half is done.
static bool cycle(void)
{
  if(budget == 0) {
    if(cut_at == FLASH_Cut_Before) longjmp(power_loss, 1);
    return true;
  }
  if(budget > 0) budget--;
  spent++;
  return false;
}

// Flags a cycle cut halfway leaves on the doublewords it touched, by the kind of the cut
static bool *cut_flags(void)
{
  if(cut_at == FLASH_Cut_Corrected) return chip.corrected;
  if(cut_at == FLASH_Cut_Unstable) return chip.unstable;
  return chip.damaged;
}

// Erase cycle, blank or not.
// A cut halfway erases nothing whole: every doubleword the page holds written stays,
// unreadable to ECC, corrected by it to a wrong value, or read erased and unreadable by turns,
// as `cut_at` says.
// A cut late leaves the page reading erased, short of a full erase.
static void erase(uint16_t page)
{
  bool cut = cycle();
  if(cut && cut_at != FLASH_Cut_Late) {
    uint32_t cell = (uint32_t)page * FLASH_PAGE_SIZE / 8;
    for(uint32_t i = cell; i < cell + FLASH_PAGE_SIZE / 8; i++) {
      if(!chip.written[i]) continue;
      chip.torn[i] = true;
      cut_flags()[i] = true;
      // Corrected to what it reads: the upper word on its way to erased
      if(cut_at == FLASH_Cut_Corrected) memset(chip.memory + 8 * i + 4, 0xFF, 4);
    }
    store(page);
    longjmp(power_loss, 1);
  }
  page_clean(page);
  store(page);
  if(cut) {
    chip.weak[page] = true;
    longjmp(power_loss, 1);
  }
  chip.erased_last = page;
}

// Program cycle of `size` bytes at `at` from the base.
// A cut halfway leaves the upper half fresh, and every doubleword of the cycle unreadable to ECC,
// corrected by it to what the half reads, or read erased and unreadable by turns.
// A cut late lands the cycle whole. A page short of a full erase holds none of it.
static void program(uint32_t at, const void *data, uint32_t size)
{
  bool cut = cycle();
  bool torn = cut && cut_at != FLASH_Cut_Late;
  uint32_t first = at / 8, cells = (size + 7) / 8;
  if(torn) {
    memset(cut_flags() + first, true, cells);
    memset(chip.torn + first, true, cells);
    size /= 2;
  }
  memcpy(chip.memory + at, data, size);
  memset(chip.written + first, true, (size + 7) / 8);
  if(!torn) {
    bool weak = page_weak(at);
    for(uint32_t i = first; i < first + cells; i++) {
      if(weak) chip.damaged[i] = true;
      else if(zeros(chip.memory + 8 * i, 8)) cell_zeroed(i);
    }
  }
  if(at < FLASH_SIZE) store((uint16_t)(at / FLASH_PAGE_SIZE));
  if(cut) longjmp(power_loss, 1);
}

//------------------------------------------------------------------------------------------- Erase

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  mount();
  // A blank page is spared the cycle;
  // one with a doubleword ECC cannot read or corrects is not blank:
  // an erase cut halfway leaves ones ECC corrected over cells not erased whole.
  bool corrected;
  if(blank(page_at(page), FLASH_PAGE_SIZE) &&
    !FLASH_Check(FLASH_GetAddress(page, 0), FLASH_PAGE_SIZE, &corrected) && !corrected) return OK;
  erase(page);
  return OK;
}

status_t FLASH_EraseFull(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  mount();
  erase(page);
  return OK;
}

//-------------------------------------------------------------------------------------------- Read

uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
  return FLASH_BASE + (uint32_t)page * FLASH_PAGE_SIZE + offset;
}

uint32_t FLASH_Read(uint32_t addr)
{
  uint32_t value = 0xFFFFFFFFu;
  mount();
  uint32_t at = index_of(addr, sizeof(value));
  if(at == FLASH_NOWHERE) return value;
  uint32_t cell = at / 8;
  if(chip.damaged[cell] || chip.torn[cell]) {
    fprintf(stderr, "NMI: plain read of the doubleword at 0x%08X, ECC fails it\n", addr & ~7u);
    exit(EXIT_FAILURE);
  }
  memcpy(&value, chip.memory + at, sizeof(value));
  return value;
}

void *FLASH_Ref(uint32_t addr)
{
  mount();
  uint32_t at = index_of(addr, 1);
  return at == FLASH_NOWHERE ? NULL : chip.memory + at;
}

// Doubleword `cell` through ECC: `ERR` when ECC cannot read it.
// An unstable one reads erased and fails by turns, erased first.
static status_t cell_read(uint32_t cell, bool *corrected)
{
  if(chip.damaged[cell]) return ERR;
  if(chip.unstable[cell]) {
    chip.flipped[cell] = !chip.flipped[cell];
    return chip.flipped[cell] ? OK : ERR;
  }
  if(corrected && chip.corrected[cell]) *corrected = true;
  return OK;
}

status_t FLASH_Check(uint32_t addr, uint32_t size, bool *corrected)
{
  if(corrected) *corrected = false;
  for(uint32_t at = addr & ~7u; at < addr + size; at += 8) {
    uint32_t index = index_of(at, 8);
    if(index == FLASH_NOWHERE) continue;
    if(cell_read(index / 8, corrected)) return ERR;
  }
  return OK;
}

status_t FLASH_Copy(uint32_t addr, void *data, uint32_t size, bool *corrected)
{
  mount();
  uint32_t start = index_of(addr, size);
  if(start == FLASH_NOWHERE) return ERR;
  if(FLASH_Check(addr, size, corrected)) return ERR;
  memcpy(data, chip.memory + start, size);
  // An unstable doubleword the check read erased copies as ones
  for(uint32_t at = addr & ~7u; at < addr + size; at += 8) {
    uint32_t cell = (start - (addr - at)) / 8;
    if(!chip.unstable[cell] || !chip.flipped[cell]) continue;
    uint32_t from = maxv(at, addr), to = minv(at + 8, addr + size);
    memset((uint8_t *)data + (from - addr), 0xFF, to - from);
  }
  return OK;
}

//------------------------------------------------------------------------------------------- Write

status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  uint32_t at = index_of(addr, 8);
  if(addr & 7u || at == FLASH_NOWHERE) return ERR;
  mount();
  uint32_t cell = at / 8;
  // Ones program nothing, as in the driver: an erased doubleword reads them already
  if(data1 == 0xFFFFFFFFu && data2 == 0xFFFFFFFFu) {
    return chip.damaged[cell] || !blank(chip.memory + at, 8) ? ERR : OK;
  }
  // ECC takes one write per erase. Zeros over a written doubleword are the exception,
  // unless it was written with ones (errata).
  if(chip.written[cell] && (data1 || data2 || blank(chip.memory + at, 8))) return ERR;
  uint32_t data[2] = { data1, data2 };
  program(at, data, sizeof(data));
  return page_weak(at) ? ERR : OK;
}

#if !defined(STM32WB55xx)

status_t FLASH_WriteFast(uint32_t addr, const uint32_t *data)
{
  uint32_t at = addr - FLASH_BASE;
  if(addr & (FLASH_ROW - 1u) || at >= FLASH_SIZE) return ERR;
  mount();
  if((int32_t)(at / FLASH_PAGE_SIZE) != chip.erased_last) return ERR;
  for(uint32_t cell = at / 8; cell < (at + FLASH_ROW) / 8; cell++) {
    if(chip.written[cell]) return ERR;
  }
  program(at, data, FLASH_ROW);
  return chip.weak[at / FLASH_PAGE_SIZE] ? ERR : OK;
}

#endif

// As the driver: the page erased, blank or not, then programmed in rows,
// on WB in doublewords, since a CPU2 fetch breaks a row there.
status_t FLASH_WritePage(uint16_t page, const uint8_t *data)
{
  if(page >= FLASH_PAGES) return ERR;
  mount();
  erase(page);
  uint32_t addr = FLASH_GetAddress(page, 0);
  #if defined(STM32WB55xx)
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
    const uint32_t *d = (const uint32_t *)(data + i);
    if(FLASH_Write(addr + i, d[0], d[1])) return ERR;
  }
  #else
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += FLASH_ROW) {
    if(FLASH_WriteFast(addr + i, (const uint32_t *)(data + i))) return ERR;
  }
  #endif
  return OK;
}

//-------------------------------------------------------------------------------------------- Host
#if defined(HOST_TEST)

static chip_t saved;

void FLASH_HostBlank(void)
{
  mounted = true;
  chip_blank();
}

// A programmer leaves the chip reset, so no page counts as erased last
void FLASH_HostProgram(uint32_t addr, const uint8_t *data, uint32_t len)
{
  mount();
  uint32_t at = index_of(addr, len);
  if(at == FLASH_NOWHERE) {
    fprintf(stderr, "Programmer: %u bytes at 0x%08X leave the chip\n", len, addr);
    exit(EXIT_FAILURE);
  }
  if(at < FLASH_SIZE) {
    uint16_t first = (uint16_t)(at / FLASH_PAGE_SIZE);
    uint16_t last = (uint16_t)((at + len - 1) / FLASH_PAGE_SIZE);
    for(uint16_t page = first; page <= last; page++) page_clean(page);
  }
  memcpy(chip.memory + at, data, len);
  memset(chip.written + at / 8, true, (at + len - 1) / 8 - at / 8 + 1);
  chip.erased_last = -1;
}

void FLASH_HostAge(uint32_t addr, uint8_t bits)
{
  mount();
  uint32_t at = index_of(addr, 1);
  if(at == FLASH_NOWHERE || !bits) return;
  uint32_t cell = at / 8;
  if(bits == 1) chip.corrected[cell] = true;
  else chip.damaged[cell] = true;
}

uint32_t FLASH_HostCycles(void)
{
  return spent;
}

void FLASH_HostCut(int32_t cycles, FLASH_Cut_t cut, void (*Run)(void))
{
  budget = cycles;
  cut_at = cut;
  if(!setjmp(power_loss)) Run();
  budget = -1;
  cut_at = FLASH_Cut_Before;
}

void FLASH_HostSave(void)
{
  mount();
  saved = chip;
}

void FLASH_HostRestore(void)
{
  mounted = true;
  chip = saved;
}

void FLASH_SetDirectory(const char *path)
{
  unused(path);
}

#else

void FLASH_SetDirectory(const char *path)
{
  if(!path) return;
  strncpy(flash_dir, path, sizeof(flash_dir) - 1);
  mkdir_p(flash_dir); // there already, fine
  mounted = false; // the next access loads the pages from there
}

#endif
//-------------------------------------------------------------------------------------------------

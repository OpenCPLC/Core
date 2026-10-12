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
#define FLASH_ROW 256u // bytes of one `FLASH_WriteFast`

// The chip, `FLASH_BASE` at index 0
typedef struct {
  uint8_t memory[FLASH_SIZE];
  bool written[FLASH_SIZE / 8];    // doubleword written since its erase, as its ECC tells the chip
  bool damaged[FLASH_SIZE / 8];    // ECC cannot read it: a cycle cut halfway, two aged bits
  bool corrected[FLASH_SIZE / 8];  // ECC corrects it: a cycle cut halfway, one aged bit
  bool torn[FLASH_SIZE / 8];       // of a cycle cut halfway: a plain read can fail it, as on WB55
  int32_t erased_last;             // page of the last erase cycle, the only one a row goes into
} chip_t;

static chip_t chip;
static bool mounted;
static uint32_t spent;      // erase and program cycles done
static int32_t budget = -1; // cycles left before the power goes, `-1` for never
static FLASH_Cut_t cut_at;  // where in its cycle the power goes
static jmp_buf power_loss;

static uint8_t *page_at(uint16_t page) { return chip.memory + (uint32_t)page * FLASH_PAGE_SIZE; }

static bool blank(const uint8_t *cell, uint32_t size)
{
  for(uint32_t i = 0; i < size; i++) {
    if(cell[i] != 0xFF) return false;
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
  chip.erased_last = -1;
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

// Erase cycle, blank or not; a cut halfway leaves the upper half of the page as it was
static void erase(uint16_t page)
{
  bool torn = cycle();
  uint32_t size = torn ? FLASH_PAGE_SIZE / 2 : FLASH_PAGE_SIZE;
  uint32_t cell = (uint32_t)page * FLASH_PAGE_SIZE / 8;
  memset(page_at(page), 0xFF, size);
  memset(chip.written + cell, false, size / 8);
  memset(chip.damaged + cell, false, size / 8);
  memset(chip.corrected + cell, false, size / 8);
  memset(chip.torn + cell, false, size / 8);
  chip.erased_last = page;
  store(page);
  if(torn) longjmp(power_loss, 1);
}

// Program cycle of `size` bytes at `at` from the base.
// A cut halfway leaves the upper half fresh, and every doubleword of the cycle unreadable to ECC
// or corrected by it to what the half reads.
static void program(uint32_t at, const void *data, uint32_t size)
{
  bool torn = cycle();
  if(torn) {
    bool *ecc = cut_at == FLASH_Cut_Corrected ? chip.corrected : chip.damaged;
    memset(ecc + at / 8, true, (size + 7) / 8);
    memset(chip.torn + at / 8, true, (size + 7) / 8);
    size /= 2;
  }
  memcpy(chip.memory + at, data, size);
  memset(chip.written + at / 8, true, (size + 7) / 8);
  store((uint16_t)(at / FLASH_PAGE_SIZE));
  if(torn) longjmp(power_loss, 1);
}

//------------------------------------------------------------------------------------------- Erase

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  mount();
  // A blank page is spared the cycle; one with a doubleword ECC cannot read is not blank
  if(blank(page_at(page), FLASH_PAGE_SIZE) &&
    !FLASH_Check(FLASH_GetAddress(page, 0), FLASH_PAGE_SIZE, NULL)) return OK;
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
  if(addr - FLASH_BASE > FLASH_SIZE - sizeof(value)) return value;
  uint32_t cell = (addr - FLASH_BASE) / 8;
  if(chip.damaged[cell] || chip.torn[cell]) {
    fprintf(stderr, "NMI: plain read of the doubleword at 0x%08X, ECC fails it\n", addr & ~7u);
    exit(EXIT_FAILURE);
  }
  memcpy(&value, FLASH_Ref(addr), sizeof(value));
  return value;
}

void *FLASH_Ref(uint32_t addr)
{
  mount();
  return addr - FLASH_BASE < FLASH_SIZE ? chip.memory + (addr - FLASH_BASE) : NULL;
}

status_t FLASH_Check(uint32_t addr, uint32_t size, bool *corrected)
{
  if(corrected) *corrected = false;
  for(uint32_t at = addr & ~7u; at < addr + size; at += 8) {
    uint32_t cell = (at - FLASH_BASE) / 8;
    if(cell >= FLASH_SIZE / 8) continue;
    if(chip.damaged[cell]) return ERR;
    if(corrected && chip.corrected[cell]) *corrected = true;
  }
  return OK;
}

status_t FLASH_Copy(uint32_t addr, void *data, uint32_t size, bool *corrected)
{
  if(addr - FLASH_BASE > FLASH_SIZE || size > FLASH_SIZE - (addr - FLASH_BASE)) return ERR;
  if(FLASH_Check(addr, size, corrected)) return ERR;
  memcpy(data, FLASH_Ref(addr), size);
  return OK;
}

//------------------------------------------------------------------------------------------- Write

status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  uint32_t at = addr - FLASH_BASE;
  if(addr & 7u || at >= FLASH_SIZE) return ERR;
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
  return OK;
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
  return OK;
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
  uint32_t at = addr - FLASH_BASE;
  uint16_t first = (uint16_t)(at / FLASH_PAGE_SIZE);
  uint16_t last = (uint16_t)((at + len - 1) / FLASH_PAGE_SIZE);
  for(uint16_t page = first; page <= last; page++) {
    uint32_t cell = (uint32_t)page * FLASH_PAGE_SIZE / 8;
    memset(page_at(page), 0xFF, FLASH_PAGE_SIZE);
    memset(chip.written + cell, false, FLASH_PAGE_SIZE / 8);
    memset(chip.damaged + cell, false, FLASH_PAGE_SIZE / 8);
    memset(chip.corrected + cell, false, FLASH_PAGE_SIZE / 8);
    memset(chip.torn + cell, false, FLASH_PAGE_SIZE / 8);
  }
  memcpy(chip.memory + at, data, len);
  memset(chip.written + at / 8, true, (at + len - 1) / 8 - at / 8 + 1);
  chip.erased_last = -1;
}

void FLASH_HostAge(uint32_t addr, uint8_t bits)
{
  mount();
  uint32_t cell = (addr - FLASH_BASE) / 8;
  if(cell >= FLASH_SIZE / 8 || !bits) return;
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

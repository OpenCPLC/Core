// hal/host/flash.c

#include "flash.h"

#include <errno.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <direct.h>
  #define mkdir_p(path) _mkdir(path)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #define mkdir_p(path) mkdir(path, 0755)
#endif

//---------------------------------------------------------------------------------------- Internal

#define FLASH_SIZE ((uint32_t)FLASH_PAGES * FLASH_PAGE_SIZE)

static uint8_t flash_image[FLASH_SIZE];
static char flash_dir[256] = FLASH_DIR;
static bool flash_mounted = false;

static void _filename(uint16_t page, char *buf, size_t size)
{
  snprintf(buf, size, "%s/" FLASH_PREFIX "%03u.bin", flash_dir, page);
}

static uint8_t *_page(uint16_t page)
{
  return flash_image + (uint32_t)page * FLASH_PAGE_SIZE;
}

// Restore image from disk. No file means an erased page,
// so a fresh directory is blank Flash.
// A file of any other size is not a page image:
// merging it would commit a `FLASH_Save` header over a body that was never there
static void _mount(void)
{
  if(flash_mounted) return;
  flash_mounted = true;
  memset(flash_image, 0xFF, FLASH_SIZE);
  for(uint16_t page = 0; page < FLASH_PAGES; page++) {
    char filename[280];
    _filename(page, filename, sizeof(filename));
    FILE *file = fopen(filename, "rb");
    if(!file) continue;
    uint8_t *mem = _page(page);
    size_t count = fread(mem, 1, FLASH_PAGE_SIZE, file);
    if(count != FLASH_PAGE_SIZE || fgetc(file) != EOF) memset(mem, 0xFF, FLASH_PAGE_SIZE);
    fclose(file);
  }
}

static status_t _flush(uint16_t page)
{
  char filename[280];
  _filename(page, filename, sizeof(filename));
  FILE *file = fopen(filename, "wb");
  if(!file) return ERR;
  size_t count = fwrite(_page(page), 1, FLASH_PAGE_SIZE, file);
  if(fclose(file)) return ERR; // buffered tail may still fail on close
  return count == FLASH_PAGE_SIZE ? OK : ERR;
}

// Bounds-checked cell reference. `size` is the span caller is about to touch.
static uint8_t *_cell(uint32_t addr, uint32_t size)
{
  _mount();
  if(addr >= FLASH_SIZE || FLASH_SIZE - addr < size) return NULL;
  return flash_image + addr;
}

static bool _erased(const uint8_t *cell, uint32_t size)
{
  for(uint32_t i = 0; i < size; i++) {
    if(cell[i] != 0xFF) return false;
  }
  return true;
}

// Bits only ever go `1` -> `0`, and target must be erased (`PROGERR` on target).
// Span never crosses a page, so one flush covers it. Failed flush rolls the image
// back, so `ERR` always means the write did not happen
static status_t _program(uint32_t addr, const uint8_t *data, uint32_t size)
{
  uint8_t *cell = _cell(addr, size);
  if(!cell) return ERR;
  if(!_erased(cell, size)) return ERR;
  for(uint32_t i = 0; i < size; i++) cell[i] &= data[i];
  if(_flush((uint16_t)(addr / FLASH_PAGE_SIZE))) {
    memset(cell, 0xFF, size);
    return ERR;
  }
  return OK;
}

//------------------------------------------------------------------------------------------- Setup

void FLASH_Init(void)
{
  mkdir_p(flash_dir); // ignore error if exists
  _mount();
}

void FLASH_SetDirectory(const char *path)
{
  if(!path) return;
  strncpy(flash_dir, path, sizeof(flash_dir) - 1);
  flash_dir[sizeof(flash_dir) - 1] = '\0';
  mkdir_p(flash_dir); // ignore error if exists
  flash_mounted = false; // reload from new location on next access
}

//-------------------------------------------------------------------------------- Erase/Read/Write

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  _mount();
  char filename[280];
  _filename(page, filename, sizeof(filename));
  // No file means erased page. A file that cannot be dropped would resurrect
  // the page on next mount, so report it the way the target reports `WRPERR`.
  if(remove(filename) && errno != ENOENT) return ERR;
  memset(_page(page), 0xFF, FLASH_PAGE_SIZE);
  return OK;
}

uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
  return (uint32_t)page * FLASH_PAGE_SIZE + offset;
}

uint32_t FLASH_Read(uint32_t addr)
{
  uint8_t *cell = _cell(addr, sizeof(uint32_t));
  if(!cell) return 0xFFFFFFFF;
  uint32_t value;
  memcpy(&value, cell, sizeof(value));
  return value;
}

void *FLASH_Ref(uint32_t addr)
{
  return _cell(addr, 1);
}

status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  if(addr % 8) return ERR;
  uint32_t data[2] = { data1, data2 };
  return _program(addr, (const uint8_t *)data, sizeof(data));
}

status_t FLASH_WriteFast(uint32_t addr, uint32_t *data)
{
  if(addr % 256) return ERR;
  return _program(addr, (const uint8_t *)data, 256);
}

status_t FLASH_WritePage(uint16_t page, uint8_t *data)
{
  if(FLASH_Erase(page)) return ERR;
  uint32_t addr = FLASH_GetAddress(page, 0);
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 256) {
    if(FLASH_WriteFast(addr, (uint32_t *)data)) return ERR;
    addr += 256;
    data += 256;
  }
  return OK;
}

//------------------------------------------------------------------------------- Compare/Save/Load

bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return false;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  if(flash_end - addr < 4u + size) return false;
  uint32_t raw = FLASH_Read(addr);
  if(raw == 0xFFFFFFFFu) return false;
  if((uint16_t)raw != size) return false;
  return memcmp(data, FLASH_Ref(addr + 4u), size) == 0;
}

// Layout: [size:4B][data:size B] padded to 8B boundary per DW write.
// Real footprint = `align_up(size + 4, 8)`. Header DW = `(size, data[0..3])`.
// Header is written LAST as a commit marker: a torn save leaves it erased,
// so `FLASH_Load`/`FLASH_Compare` reject the record instead of a half-written body.
// Single-slot only; for CRC-grade torn-write safety use PDB/EEPROM.
status_t FLASH_Save(uint16_t page, uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return ERR;
  if(size == 0) return ERR;
  uint32_t total = ((uint32_t)size + 4u + 7u) & ~7u;
  uint32_t head = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  uint32_t end_page = FLASH_GetAddress(page + 1, 0);
  if(flash_end - head < total) return ERR;
  if(FLASH_Erase(page)) return ERR;
  uint16_t first = size > 4 ? 4 : size;
  uint32_t w1 = 0xFFFFFFFFu;
  memcpy(&w1, data, first);
  uint8_t *body = data + first;
  uint16_t left = size - first;
  uint32_t addr = head + 8u; // body begins after reserved header doubleword
  uint32_t d[2];
  while(left) {
    if(addr >= end_page) {
      page++;
      if(page >= FLASH_PAGES) return ERR;
      if(FLASH_Erase(page)) return ERR;
      end_page = FLASH_GetAddress(page + 1, 0);
    }
    d[0] = 0xFFFFFFFFu;
    d[1] = 0xFFFFFFFFu;
    uint16_t chunk = left > 8 ? 8 : left;
    memcpy(d, body, chunk);
    if(FLASH_Write(addr, d[0], d[1])) return ERR;
    addr += 8u;
    body += chunk;
    left -= chunk;
  }
  if(FLASH_Write(head, (uint32_t)size, w1)) return ERR; // commit marker LAST
  return OK;
}

uint16_t FLASH_Load(uint16_t page, uint8_t *data)
{
  if(page >= FLASH_PAGES) return 0;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  uint32_t raw = FLASH_Read(addr);
  if(raw == 0xFFFFFFFFu) return 0;
  uint16_t size = (uint16_t)raw;
  if(size == 0) return 0;
  if(flash_end - addr < 4u + size) return 0;
  memcpy(data, FLASH_Ref(addr + 4u), size);
  return size;
}

//-------------------------------------------------------------------------------------------------

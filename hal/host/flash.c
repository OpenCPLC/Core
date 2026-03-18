// hal/host/flash.c

#include "flash.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <direct.h>
  #define mkdir_p(path) _mkdir(path)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #define mkdir_p(path) mkdir(path, 0755)
#endif

//------------------------------------------------------------------------------------------------- Internal

static char flash_dir[256] = ".";

static void flash_get_filename(uint16_t page, char *buf, size_t bufsize)
{
  snprintf(buf, bufsize, "%s/" FLASH_PREFIX "%03u.bin", flash_dir, page);
}

//-------------------------------------------------------------------------------------------------

void FLASH_Init(void)
{
  mkdir_p(flash_dir); // ignore error if exists
}

void FLASH_SetDirectory(const char *path)
{
  if(path) {
    strncpy(flash_dir, path, sizeof(flash_dir) - 1);
    flash_dir[sizeof(flash_dir) - 1] = '\0';
    mkdir_p(flash_dir);
  }
}

//------------------------------------------------------------------------------------------------- Erase

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  char filename[280];
  flash_get_filename(page, filename, sizeof(filename));
  remove(filename); // ignore error if doesn't exist
  return OK;
}

//------------------------------------------------------------------------------------------------- Address (compatibility)

uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
  return (uint32_t)(FLASH_PAGE_SIZE * page) + offset;
}

uint32_t FLASH_Read(uint32_t addr)
{
  (void)addr;
  return 0xFFFFFFFF; // erased state
}

//------------------------------------------------------------------------------------------------- Write (compatibility stubs)

status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  (void)addr; (void)data1; (void)data2;
  return ERR; // use FLASH_Save
}

status_t FLASH_WriteFast(uint32_t addr, uint32_t *data)
{
  (void)addr; (void)data;
  return ERR; // use FLASH_Save
}

status_t FLASH_WritePage(uint16_t page, uint8_t *data)
{
  (void)page; (void)data;
  return ERR; // use FLASH_Save
}

//------------------------------------------------------------------------------------------------- Compare/Save/Load

bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size)
{
  char filename[280];
  flash_get_filename(page, filename, sizeof(filename));
  FILE *f = fopen(filename, "rb");
  if(!f) return false;
  // read stored size (first 2 bytes)
  uint16_t stored_size = 0;
  if(fread(&stored_size, sizeof(stored_size), 1, f) != 1) {
    fclose(f);
    return false;
  }
  if(stored_size != size) {
    fclose(f);
    return false;
  }
  // compare data
  uint8_t *buf = (uint8_t *)malloc(size);
  if(!buf) {
    fclose(f);
    return false;
  }
  bool equal = false;
  if(fread(buf, 1, size, f) == size) {
    equal = (memcmp(buf, data, size) == 0);
  }
  free(buf);
  fclose(f);
  return equal;
}

status_t FLASH_Save(uint16_t page, uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return ERR;
  if(size > FLASH_PAGE_SIZE - 2) return ERR; // need 2 bytes for size header
  char filename[280];
  flash_get_filename(page, filename, sizeof(filename));
  FILE *f = fopen(filename, "wb");
  if(!f) return ERR;
  // write size header (2 bytes, little-endian like STM32)
  if(fwrite(&size, sizeof(size), 1, f) != 1) {
    fclose(f);
    return ERR;
  }
  // write data
  if(fwrite(data, 1, size, f) != size) {
    fclose(f);
    return ERR;
  }
  fclose(f);
  return OK;
}

uint16_t FLASH_Load(uint16_t page, uint8_t *data)
{
  if(page >= FLASH_PAGES) return 0;
  char filename[280];
  flash_get_filename(page, filename, sizeof(filename));
  FILE *f = fopen(filename, "rb");
  if(!f) return 0;
  // read size header
  uint16_t size = 0;
  if(fread(&size, sizeof(size), 1, f) != 1) {
    fclose(f);
    return 0;
  }
  if(size == 0 || size == 0xFFFF || size > FLASH_PAGE_SIZE) {
    fclose(f);
    return 0;
  }
  // read data
  if(fread(data, 1, size, f) != size) {
    fclose(f);
    return 0;
  }
  fclose(f);
  return size;
}

//-------------------------------------------------------------------------------------------------
// hal/stm32/per/flash_record.c
// Records over the flash API, one code for the chip and its host model

#include "flash.h"

#include <string.h>

bool FLASH_Compare(uint16_t page, const uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return false;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  if(flash_end - addr < 8u + size) return false;
  uint32_t head[2];
  if(FLASH_Copy(addr, head, 8u, NULL)) return false;
  if(head[0] != size || head[1] != ~(uint32_t)size) return false;
  // The body through ECC, piece by piece
  uint8_t piece[64];
  for(uint32_t done = 0; done < size; done += sizeof(piece)) {
    uint32_t take = minv(size - done, sizeof(piece));
    if(FLASH_Copy(addr + 8u + done, piece, take, NULL)) return false;
    if(memcmp(piece, data + done, take)) return false;
  }
  return true;
}

// Layout: header doubleword `[size:4B][~size:4B]`, then the data in doublewords.
// The header is written last as the commit marker: a save cut before it leaves it erased,
// and one cut inside it leaves a size its complement does not match,
// since programming only clears bits. Either way `FLASH_Load` sees no record.
// A doubleword ECC cannot read, header or body, reads as no record too:
// both come through ECC, `FLASH_Copy`.
// Single slot, no checksum; torn-write safety of the body is what PDB and EEPROM add.
status_t FLASH_Save(uint16_t page, const uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return ERR;
  if(size == 0) return ERR;
  uint32_t total = ((uint32_t)size + 8u + 7u) & ~7u;
  uint32_t head = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  uint32_t end_page = FLASH_GetAddress(page + 1, 0);
  if(flash_end - head < total) return ERR;
  if(FLASH_Erase(page)) return ERR;
  const uint8_t *body = data;
  uint16_t left = size;
  uint32_t addr = head + 8u; // the body follows the header doubleword
  while(left) {
    if(addr >= end_page) {
      page++;
      if(page >= FLASH_PAGES) return ERR;
      if(FLASH_Erase(page)) return ERR;
      end_page = FLASH_GetAddress(page + 1, 0);
    }
    uint32_t d[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
    uint16_t chunk = left > 8 ? 8 : left;
    memcpy(d, body, chunk);
    if(FLASH_Write(addr, d[0], d[1])) return ERR;
    addr += 8u;
    body += chunk;
    left -= chunk;
  }
  return FLASH_Write(head, size, ~(uint32_t)size);
}

uint16_t FLASH_Load(uint16_t page, uint8_t *data)
{
  if(page >= FLASH_PAGES) return 0;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  uint32_t head[2];
  if(FLASH_Copy(addr, head, 8u, NULL)) return 0;
  uint32_t size = head[0];
  if(!size || size > 0xFFFFu || head[1] != ~size) return 0;
  if(flash_end - addr < 8u + size) return 0;
  if(FLASH_Copy(addr + 8u, data, size, NULL)) return 0;
  return (uint16_t)size;
}

//-------------------------------------------------------------------------------------------------

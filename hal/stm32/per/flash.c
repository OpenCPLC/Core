// hal/stm32/per/flash.c

#include "flash.h"

//------------------------------------------------------------------------- Compatibility Layer

#define FLASH_START_ADDR ((uint32_t)0x08000000)
#define FLASH_KEY1 ((uint32_t)0x45670123)
#define FLASH_KEY2 ((uint32_t)0xCDEF89AB)

#if defined(STM32G0)
  #define FLASH_BSY FLASH_SR_BSY1
  #define FLASH_PNB_POS 3
  #if defined(STM32G0C1xx)
    #define FLASH_PNB_MASK (FLASH_CR_PNB | FLASH_CR_BKER)
  #else
    #define FLASH_PNB_MASK FLASH_CR_PNB
  #endif
  // G0 error flags
  #define FLASH_ERR_FLAGS (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
    FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
  #define FLASH_CLR_FLAGS (FLASH_SR_EOP | FLASH_ERR_FLAGS)
#elif defined(STM32WB)
  #define FLASH_BSY FLASH_SR_BSY
  #define FLASH_PNB_POS FLASH_CR_PNB_Pos
  #define FLASH_PNB_MASK FLASH_CR_PNB
  // WB error flags (includes OPERR, PROGERR)
  #define FLASH_ERR_FLAGS (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
    FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
    FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
  #define FLASH_CLR_FLAGS (FLASH_SR_EOP | FLASH_ERR_FLAGS)
#elif defined(STM32G4)
  #define FLASH_BSY FLASH_SR_BSY
  #define FLASH_PNB_POS FLASH_CR_PNB_Pos
  #define FLASH_PNB_MASK FLASH_CR_PNB
  #define FLASH_ERR_FLAGS (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
    FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
    FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
  #define FLASH_CLR_FLAGS (FLASH_SR_EOP | FLASH_ERR_FLAGS)
#endif

//------------------------------------------------------------------------------------ Internal

static inline void flash_wait(void)
{
  while(FLASH->SR & FLASH_BSY) __DSB();
}

#if defined(STM32G0) && defined(STM32G0C1xx)
/**
 * @brief Get bank bit for dual-bank G0C1.
 * @param[in] page Page index (0-255)
 * @return Bank selection bit (0 or 1)
 */
static inline uint32_t flash_bank_bit(uint16_t page)
{
  return (page > 127) ? (1u << 13) : 0;
}
#endif

//---------------------------------------------------------------------------------------- Init

static inline status_t flash_unlock(void)
{
  flash_wait();
  if(FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    if(FLASH->CR & FLASH_CR_LOCK) return ERR;
  }
  return OK;
}

static inline void flash_lock(void)
{
  flash_wait();
  FLASH->CR |= FLASH_CR_LOCK;
}

//--------------------------------------------------------------------------------------- Erase

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  if(flash_unlock()) return ERR;
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR &= ~FLASH_PNB_MASK;
  #if defined(STM32G0) && defined(STM32G0C1xx)
    FLASH->CR |= flash_bank_bit(page) | ((uint32_t)page << FLASH_PNB_POS) | FLASH_CR_PER;
  #else
    FLASH->CR |= ((uint32_t)page << FLASH_PNB_POS) | FLASH_CR_PER;
  #endif
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;
  __DSB();
  uint32_t sr = FLASH->SR;
  FLASH->SR = FLASH_CLR_FLAGS;
  flash_lock();
  if(sr & FLASH_ERR_FLAGS) return ERR;
  if(sr & FLASH_SR_EOP) return OK;
  return ERR;
}

//-------------------------------------------------------------------------------- Address/Read

uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
  return FLASH_START_ADDR + (FLASH_PAGE_SIZE * page) + offset;
}

uint32_t FLASH_Read(uint32_t addr)
{
  return *(uint32_t *)addr;
}

//--------------------------------------------------------------------------------------- Write

// Doubleword program: requires 8B-aligned address and uninterrupted store pair.
// IRQ between `data1` and `data2` triggers `PROGERR`/`SIZERR` on G0/WB/G4.
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  if(addr & 7u) return ERR;
  if(flash_unlock()) return ERR;
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR |= FLASH_CR_PG;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  *(volatile uint32_t *)addr = data1;
  *(volatile uint32_t *)(addr + 4u) = data2;
  __DSB();
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PG;
  __set_PRIMASK(primask);
  uint32_t sr = FLASH->SR;
  FLASH->SR = FLASH_CLR_FLAGS;
  flash_lock();
  if(sr & FLASH_ERR_FLAGS) return ERR;
  if(*(volatile uint32_t *)addr != data1) return ERR;
  if(*(volatile uint32_t *)(addr + 4u) != data2) return ERR;
  return OK;
}

// Fast (row) program: 256B aligned, MUST execute from RAM (RWW conflict otherwise).
// Linker must place this in `.RamFunc` and copy at startup.
status_t FLASH_WriteFast(uint32_t addr, uint32_t *data)
{
  if(addr & 0xFFu) return ERR;
  uint32_t self = (uint32_t)&FLASH_WriteFast & ~1u; // mask Thumb bit
  if(self >= FLASH_START_ADDR && self < FLASH_START_ADDR + (FLASH_PAGES * FLASH_PAGE_SIZE)) {
    return ERR; // not in RAM: linker misconfig
  }
  if(flash_unlock()) return ERR;
  flash_wait();
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR |= FLASH_CR_FSTPG;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  for(int i = 0; i < 64; i++) {
    *(uint32_t *)addr = data[i];
    addr += 4u;
  }
  flash_wait();
  FLASH->CR &= ~FLASH_CR_FSTPG;
  __set_PRIMASK(primask);
  uint32_t sr = FLASH->SR;
  FLASH->SR = FLASH_CLR_FLAGS;
  flash_lock();
  if(sr & FLASH_ERR_FLAGS) return ERR;
  if(sr & FLASH_SR_EOP) return OK;
  return ERR;
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

//--------------------------------------------------------------------------- Compare/Save/Load

bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return false;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  if(flash_end - addr < 4u + size) return false;
  uint32_t raw = FLASH_Read(addr);
  if(raw == 0xFFFFFFFFu) return false;
  if((uint16_t)raw != size) return false;
  return memcmp(data, (uint8_t *)(addr + 4u), size) == 0;
}

// Layout: [size:4B][data:size B] padded to 8B boundary per DW write.
// Real footprint = `align_up(size + 4, 8)`. First DW = `(size, data[0..3])`.
status_t FLASH_Save(uint16_t page, uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return ERR;
  if(size == 0) return ERR;
  uint32_t total = ((uint32_t)size + 4u + 7u) & ~7u;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  uint32_t end_page = FLASH_GetAddress(page + 1, 0);
  if(flash_end - addr < total) return ERR;
  if(FLASH_Erase(page)) return ERR;
  uint32_t w1 = 0xFFFFFFFFu;
  uint16_t chunk = size > 4 ? 4 : size;
  memcpy(&w1, data, chunk);
  if(FLASH_Write(addr, (uint32_t)size, w1)) return ERR;
  addr += 8u;
  data += chunk;
  size -= chunk;
  uint32_t d[2];
  while(size) {
    if(addr >= end_page) {
      page++;
      if(page >= FLASH_PAGES) return ERR;
      if(FLASH_Erase(page)) return ERR;
      end_page = FLASH_GetAddress(page + 1, 0);
    }
    d[0] = 0xFFFFFFFFu;
    d[1] = 0xFFFFFFFFu;
    chunk = size > 8 ? 8 : size;
    memcpy(d, data, chunk);
    if(FLASH_Write(addr, d[0], d[1])) return ERR;
    addr += 8u;
    data += chunk;
    size -= chunk;
  }
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
  memcpy(data, (uint8_t *)(addr + 4u), size);
  return size;
}

//---------------------------------------------------------------------------------------------
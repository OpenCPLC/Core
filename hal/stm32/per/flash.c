// hal/stm32/per/flash.c

#include "flash.h"

#include <string.h>

#if defined(STM32WB)
  #include "hsem_wb.h"
  #define flash_take() HSEM_Wait(HSEM_FLASH)
  #define flash_give() HSEM_Give(HSEM_FLASH)
#else
  #define flash_take()
  #define flash_give()
  #define WPAN_FlashEraseActivity(active)
#endif

//--------------------------------------------------------------------------------------- Constants

#define FLASH_START_ADDR 0x08000000u
#define FLASH_KEY1 0x45670123u
#define FLASH_KEY2 0xCDEF89ABu

// `CFGBSY` holds while the control register is being taken over, a write during it is lost.
// The second bank of the dual-bank parts reports on `BSY2`, its pages start at 128.
#if defined(STM32G0)
  #define FLASH_PNB_POS 3
  #if defined(STM32G0C1xx)
  #define FLASH_BSY (FLASH_SR_BSY1 | FLASH_SR_BSY2 | FLASH_SR_CFGBSY)
  #define FLASH_PNB_MASK (FLASH_CR_PNB | FLASH_CR_BKER)
  #else
  #define FLASH_BSY (FLASH_SR_BSY1 | FLASH_SR_CFGBSY)
  #define FLASH_PNB_MASK FLASH_CR_PNB
  #endif
#elif defined(STM32WB)
  #define FLASH_BSY (FLASH_SR_BSY | FLASH_SR_CFGBSY)
  #define FLASH_PNB_POS FLASH_CR_PNB_Pos
  #define FLASH_PNB_MASK FLASH_CR_PNB
#endif

// Busy bit and page-number field move between families, the error set does not.
// `PROGERR` reports a write into a location that was not erased
#define FLASH_ERR_FLAGS (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
  FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
  FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
#define FLASH_CLR_FLAGS (FLASH_SR_EOP | FLASH_ERR_FLAGS)

//---------------------------------------------------------------------------------------- Internal

#if defined(STM32WB)
// The CPU2 stack stretches its radio timing around erases when told ahead; the real
// send lives in `wpan_wb.c`, a build without the radio keeps this stub
__attribute__((weak)) void WPAN_FlashEraseActivity(bool active)
{
  unused(active);
}
#endif

// Always inline: `FLASH_WriteFast` runs from RAM and a call into flash mid-row faults
static inline __attribute__((always_inline)) void flash_wait(void)
{
  while(FLASH->SR & FLASH_BSY) __DSB();
}

// Page reads as erased: `0xFFFFFFFF` in every word
static bool page_is_erased(uint16_t page)
{
  const volatile uint32_t *word = (const volatile uint32_t *)FLASH_GetAddress(page, 0);
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE / sizeof(uint32_t); i++) {
    if(word[i] != 0xFFFFFFFFu) return false;
  }
  return true;
}

// The semaphore spans unlock to finish, so every operation holds it exactly once
static status_t flash_unlock(void)
{
  flash_take();
  flash_wait();
  if(FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    if(FLASH->CR & FLASH_CR_LOCK) {
      flash_give();
      return ERR;
    }
  }
  return OK;
}

// Latch the error flags, clear the status, lock and release
static status_t flash_finish(void)
{
  uint32_t sr = FLASH->SR;
  FLASH->SR = FLASH_CLR_FLAGS;
  flash_wait();
  FLASH->CR |= FLASH_CR_LOCK;
  flash_give();
  return (sr & FLASH_ERR_FLAGS) ? ERR : OK;
}

//------------------------------------------------------------------------------------------- Erase

// Erase, blank or not
static status_t flash_erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  WPAN_FlashEraseActivity(true);
  if(flash_unlock()) {
    WPAN_FlashEraseActivity(false);
    return ERR;
  }
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR &= ~FLASH_PNB_MASK;
  #if defined(STM32G0) && defined(STM32G0C1xx)
  // Dual bank: `BKER` picks the bank, `PNB` is the page within it, not the absolute one
  uint32_t bank = page > 127 ? FLASH_CR_BKER : 0;
  FLASH->CR |= bank | ((uint32_t)(page & 0x7Fu) << FLASH_PNB_POS) | FLASH_CR_PER;
  #else
  FLASH->CR |= ((uint32_t)page << FLASH_PNB_POS) | FLASH_CR_PER;
  #endif
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;
  __DSB();
  status_t ret = flash_finish();
  WPAN_FlashEraseActivity(false);
  if(ret) return ERR;
  // `EOP` is gated by `EOPIE`, so success is read from the flash itself
  return page_is_erased(page) ? OK : ERR;
}

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  if(page_is_erased(page)) return OK; // a blank page is spared the cycle
  return flash_erase(page);
}

//-------------------------------------------------------------------------------------------- Read

uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
  return FLASH_START_ADDR + (FLASH_PAGE_SIZE * page) + offset;
}

uint32_t FLASH_Read(uint32_t addr)
{
  return *(const uint32_t *)addr;
}

//------------------------------------------------------------------------------------------- Write

// The two stores must not be split by an interrupt: a gap raises `PROGERR` or `SIZERR`
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
  if(flash_finish()) return ERR;
  if(*(volatile uint32_t *)addr != data1) return ERR;
  if(*(volatile uint32_t *)(addr + 4u) != data2) return ERR;
  return OK;
}

status_t FLASH_WriteFast(uint32_t addr, const uint32_t *data)
{
  if(addr & 0xFFu) return ERR;
  // Linker placed this function in flash: a row program from there would stall the core
  uint32_t self = (uint32_t)&FLASH_WriteFast & ~1u; // without the Thumb bit
  if(self >= FLASH_START_ADDR && self < FLASH_START_ADDR + (FLASH_PAGES * FLASH_PAGE_SIZE)) {
    return ERR;
  }
  if(flash_unlock()) return ERR;
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR |= FLASH_CR_FSTPG;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  for(int i = 0; i < 64; i++) *(volatile uint32_t *)(addr + 4u * i) = data[i];
  flash_wait();
  FLASH->CR &= ~FLASH_CR_FSTPG;
  __set_PRIMASK(primask);
  if(flash_finish()) return ERR;
  // Same `EOPIE` gate as the erase: the row is verified against the source
  for(int i = 0; i < 64; i++) {
    if(*(volatile uint32_t *)(addr + 4u * i) != data[i]) return ERR;
  }
  return OK;
}

status_t FLASH_WritePage(uint16_t page, const uint8_t *data)
{
  // Fast programming takes only the page erased last, so a blank page is erased again:
  // skipping it ends the row in `PGSERR` and `PGAERR`
  if(flash_erase(page)) return ERR;
  uint32_t addr = FLASH_GetAddress(page, 0);
  #if defined(STM32WB)
  // A row program breaks on every CPU2 fetch and ends in `MISERR`: beside a running
  // radio core the page goes in double words, the way the EEPROM writes
  if(PWR->CR4 & PWR_CR4_C2BOOT) {
    for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
      const uint32_t *d = (const uint32_t *)(data + i);
      if(FLASH_Write(addr + i, d[0], d[1])) return ERR;
    }
    return OK;
  }
  #endif
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 256) {
    if(FLASH_WriteFast(addr + i, (const uint32_t *)(data + i))) return ERR;
  }
  return OK;
}

//------------------------------------------------------------------------------------------ Record

bool FLASH_Compare(uint16_t page, const uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return false;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t flash_end = FLASH_GetAddress(FLASH_PAGES, 0);
  if(flash_end - addr < 4u + size) return false;
  uint32_t raw = FLASH_Read(addr);
  if(raw == 0xFFFFFFFFu) return false;
  if((uint16_t)raw != size) return false;
  return memcmp(data, (const uint8_t *)(addr + 4u), size) == 0;
}

// Layout: `[size:4B][data:size B]` in doublewords, so the header doubleword carries
// the size and the first four data bytes. The header is written last as the commit
// marker: a torn save leaves it erased and `FLASH_Load` sees no record.
// Single slot, no checksum; torn-write safety of the body is what PDB and EEPROM add
status_t FLASH_Save(uint16_t page, const uint8_t *data, uint16_t size)
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
  const uint8_t *body = data + first;
  uint16_t left = size - first;
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
  return FLASH_Write(head, size, w1);
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
  memcpy(data, (const uint8_t *)(addr + 4u), size);
  return size;
}

//-------------------------------------------------------------------------------------------------

// hal/stm32/per/flash.c

#include "flash.h"

//------------------------------------------------------------------------------------------------- Compatibility Layer

#define FLASH_START_ADDR  ((uint32_t)0x08000000)
#define FLASH_KEY1        ((uint32_t)0x45670123)
#define FLASH_KEY2        ((uint32_t)0xCDEF89AB)

#if defined(STM32G0)
  #define FLASH_BSY         FLASH_SR_BSY1
  #define FLASH_PNB_POS     3
  // G0 error flags
  #define FLASH_ERR_FLAGS   (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
                             FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
  #define FLASH_CLR_FLAGS   (FLASH_SR_EOP | FLASH_ERR_FLAGS)
#elif defined(STM32WB)
  #define FLASH_BSY         FLASH_SR_BSY
  #define FLASH_PNB_POS     FLASH_CR_PNB_Pos
  // WB error flags (includes OPERR, PROGERR)
  #define FLASH_ERR_FLAGS   (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                             FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
                             FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
  #define FLASH_CLR_FLAGS   (FLASH_SR_EOP | FLASH_ERR_FLAGS)
#elif defined(STM32G4)
  #define FLASH_BSY         FLASH_SR_BSY
  #define FLASH_PNB_POS     FLASH_CR_PNB_Pos
  #define FLASH_ERR_FLAGS   (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                             FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
                             FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)
  #define FLASH_CLR_FLAGS   (FLASH_SR_EOP | FLASH_ERR_FLAGS)
#endif

//------------------------------------------------------------------------------------------------- Internal

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

//------------------------------------------------------------------------------------------------- Init

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

//------------------------------------------------------------------------------------------------- Erase

status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  if(flash_unlock()) return ERR;
  FLASH->CR &= ~FLASH_CR_PNB;
  #if defined(STM32G0) && defined(STM32G0C1xx)
    FLASH->CR |= flash_bank_bit(page) | ((uint32_t)page << FLASH_PNB_POS) | FLASH_CR_PER;
  #else
    FLASH->CR |= ((uint32_t)page << FLASH_PNB_POS) | FLASH_CR_PER;
  #endif
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;
  __DSB();
  if(FLASH->SR & FLASH_SR_EOP) {
    FLASH->SR = FLASH_SR_EOP;
    flash_lock();
    return OK;
  }
  flash_lock();
  return ERR;
}

//------------------------------------------------------------------------------------------------- Address/Read

uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
  return FLASH_START_ADDR + (FLASH_PAGE_SIZE * page) + offset;
}

uint32_t FLASH_Read(uint32_t addr)
{
  return *(uint32_t *)addr;
}

//------------------------------------------------------------------------------------------------- Write

status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  if(flash_unlock()) return ERR;
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR |= FLASH_CR_PG;
  *(uint32_t *)addr = data1;
  __ISB();
  *(uint32_t *)(addr + 4u) = data2;
  __DSB();
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PG;
  if(FLASH->SR & FLASH_ERR_FLAGS) {
    flash_lock();
    return ERR;
  }
  FLASH->SR = FLASH_SR_EOP;
  if(*(volatile uint32_t *)addr != data1 || *(volatile uint32_t *)(addr + 4u) != data2) {
    flash_lock();
    return ERR;
  }
  flash_lock();
  return OK;
}

status_t FLASH_WriteFast(uint32_t addr, uint32_t *data)
{
  if(flash_unlock()) return ERR;
  while(FLASH->SR & FLASH_BSY);
  FLASH->CR |= FLASH_CR_FSTPG;
  __disable_irq();
  for(int i = 0; i < 64; i++) {
    *(uint32_t *)addr = data[i];
    addr += 4u;
  }
  while(FLASH->SR & FLASH_BSY);
  FLASH->CR &= ~FLASH_CR_FSTPG;
  __enable_irq();
  if(FLASH->SR & FLASH_SR_EOP) {
    FLASH->SR = FLASH_SR_EOP;
    flash_lock();
    return OK;
  }
  flash_lock();
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

//------------------------------------------------------------------------------------------------- Compare/Save/Load

bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size)
{
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint16_t size_flash = FLASH_Read(addr);
  if(size_flash != size) return false;
  addr += 4;
  return (memcmp(data, (uint8_t *)addr, size) == 0);
}

status_t FLASH_Save(uint16_t page, uint8_t *data, uint16_t size)
{
  if(page >= FLASH_PAGES) return ERR;
  if(FLASH_Erase(page)) return ERR;
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t end = FLASH_GetAddress(page + 1, 0);
  int32_t length = (int32_t)size;
  if(addr + size >= FLASH_GetAddress(FLASH_PAGES, 0)) return ERR;
  if(FLASH_Write(addr, (uint32_t)size, *(uint32_t *)data)) return ERR;
  length -= 4;
  data += 4;
  if(length <= 0) return OK;
  addr += 8;
  while(length > 0) {
    if(addr >= end) {
      page++;
      if(FLASH_Erase(page)) return ERR;
      end = FLASH_GetAddress(page + 1, 0);
    }
    if(FLASH_Write(addr, *(uint32_t *)data, *((uint32_t *)data + 1))) return ERR;
    length -= 8;
    addr += 8;
    data += 8;
  }
  return OK;
}

uint16_t FLASH_Load(uint16_t page, uint8_t *data)
{
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint16_t size = FLASH_Read(addr);
  if(addr + size >= FLASH_GetAddress(FLASH_PAGES, 0)) return 0;
  if(size == 0xFFFF) return 0;
  addr += 4;
  memcpy(data, (uint8_t *)addr, size);
  return size;
}

//-------------------------------------------------------------------------------------------------
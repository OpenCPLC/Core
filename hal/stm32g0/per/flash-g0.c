#include "flash.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Enable Flash write access.
 * Wait until not busy and unlock control register if locked.
 */
void FLASH_Init(void)
{
  while(FLASH->SR & FLASH_SR_BSY1) __DSB();
  if((FLASH->CR & FLASH_CR_LOCK) != 0) {
    FLASH->KEYR = ((uint32_t)0x45670123);
    FLASH->KEYR = ((uint32_t)0xCDEF89AB);
  }
}

/**
 * @brief Erase Flash page.
 * @param page Page index
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Erase(uint16_t page)
{
  if(page >= FLASH_PAGES) return ERR;
  while(FLASH->SR & FLASH_SR_BSY1) __DSB();
  FLASH->CR &= ~FLASH_CR_PNB;
  #ifdef STM32G081xx
    FLASH->CR |= (page << 3) | FLASH_CR_PER;
  #endif
  #ifdef STM32G0C1xx
    FLASH->CR |= ((page > 127 ? 1 : 0) << 13) | (page << 3) | FLASH_CR_PER;
  #endif
  FLASH->CR |= FLASH_CR_STRT;
  while(FLASH->SR & FLASH_SR_BSY1) __DSB();
  FLASH->CR &= ~FLASH_CR_PER;
  __DSB();
  if(FLASH->SR & FLASH_SR_EOP) FLASH->SR |= FLASH_SR_EOP;
  else return ERR;
  return OK;
}

/**
 * @brief Get Flash address for page with offset.
 * @param page Page index
 * @param offset Byte offset inside page
 * @return Address in Flash
 */
uint32_t FLASH_GetAddress(uint16_t page, int16_t offset)
{
	return FLASH_START_ADDR + (FLASH_PAGE_SIZE * page) + offset;
}

/**
 * @brief Read 32-bit word from Flash.
 * @param addr Flash address
 * @return Value at address
 */
uint32_t FLASH_Read(uint32_t addr)
{
  return *(uint32_t *)addr;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Write double word (64-bit) to Flash.
 * @param addr Flash address (aligned to 64-bit)
 * @param data1 Lower 32 bits
 * @param data2 Upper 32 bits
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  // Wait for any ongoing operation
  while(FLASH->SR & FLASH_SR_BSY1) __DSB();
  // Clear all status flags
  FLASH->SR |= FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR |
               FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_MISERR |
               FLASH_SR_FASTERR | FLASH_SR_RDERR;
  // Enable programming
  FLASH->CR |= FLASH_CR_PG;
  // Write double word (64 bits)
  *(uint32_t *)addr = data1;
  __ISB();
  *(uint32_t *)(addr + 4U) = data2;
  __DSB();
  // Wait for completion
  while(FLASH->SR & FLASH_SR_BSY1) __DSB();
  // Disable programming
  FLASH->CR &= ~FLASH_CR_PG;
  // Check for errors
  if(FLASH->SR & (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR |
    FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)) {
    return ERR;
  }
  // Clear EOP flag if set
  FLASH->SR |= FLASH_SR_EOP;
  return OK;
}


/**
 * @brief Fast write 256 bytes to Flash.
 * @param addr Block address (aligned to 256 bytes)
 * @param ary Pointer to 256 bytes buffer (aligned to 32-bit words)
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_WriteFast(uint32_t addr, uint32_t *ary)
{
  while(FLASH->SR & FLASH_SR_BSY1);
  FLASH->CR |= FLASH_CR_FSTPG;
  __disable_irq();
  for(int i=0; i<64; i++) {
	  *(uint32_t *)addr = ary[i];
	  addr += 4U;
  }
  while(FLASH->SR & FLASH_SR_BSY1);
  FLASH->CR &= ~FLASH_CR_FSTPG;
  __enable_irq();
  if(FLASH->SR & FLASH_SR_EOP) FLASH->SR |= FLASH_SR_EOP;
  else return ERR;
  return OK;
}

/**
 * @brief Write full Flash page.
 * Erases target page and writes it using fast 256-byte blocks.
 * @param page Page index
 * @param ary Pointer to buffer (`FLASH_PAGE_SIZE` bytes)
 * @return `OK` on success, `ERR` on error
 */
status_t FLASH_WritePage(uint16_t page, uint8_t *ary)
{
	if(FLASH_Erase(page)) return ERR;
	uint32_t addr = FLASH_GetAddress(page, 0);
	for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 256){
		if(FLASH_WriteFast(addr, (uint32_t *)ary)) return ERR;
		addr += 256;
		ary += 256;
	}
	return OK;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Compare Flash page with buffer.
 * @param page Page index
 * @param data Pointer to data buffer
 * @param size Buffer size in bytes
 * @return `true` if equal, `false` if different
 */
bool FLASH_Compare(uint16_t page, uint8_t *data, uint16_t size)
{
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint16_t size_flash = FLASH_Read(addr);
  if(size_flash != size) return false;
  addr += 4;
  if(memcmp(data, (uint8_t *)addr, size)) return false;
  return true;
}

/**
 * @brief Save data to Flash.
 * Stores size at first word, then writes buffer sequentially.  
 * Erases pages as needed when crossing boundaries.  
 * @param page Page index
 * @param data Pointer to buffer
 * @param size Buffer size in bytes
 * @return `OK` on success, `ERR` on error
 */
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
  while(length > 0){
    if(addr >= end) {
      addr = end;
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

/**
 * @brief Load data from Flash.
 * Reads size header and copies buffer into `data`.  
 * @param page Page index
 * @param data Pointer to buffer
 * @return Size in bytes, `0` if empty or out of range
 */
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

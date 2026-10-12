// hal/stm32/per/flash.c

#include "flash.h"

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
  // `PESD` holds while CPU2 suspends flash operations, a control write then ends in `PGSERR`
  #define FLASH_BSY (FLASH_SR_BSY | FLASH_SR_CFGBSY | FLASH_SR_PESD)
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

// `FLASH_WriteFast` runs from RAM, a call into flash mid-row faults
static inline __attribute__((always_inline)) void flash_wait(void)
{
  while(FLASH->SR & FLASH_BSY) __DSB();
}

// Page reads as erased: `0xFFFFFFFF` in every word, read through ECC piece by piece
static bool page_is_erased(uint16_t page)
{
  uint32_t addr = FLASH_GetAddress(page, 0);
  uint32_t word[16];
  for(uint32_t at = addr; at < addr + FLASH_PAGE_SIZE; at += sizeof(word)) {
    if(FLASH_Copy(at, word, sizeof(word), NULL)) return false;
    for(uint32_t i = 0; i < array_len(word); i++) {
      if(word[i] != 0xFFFFFFFFu) return false;
    }
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

//--------------------------------------------------------------------------------------------- ECC

// A double ECC error raises NMI a few instructions after the load,
// which completes with a wrong value.
// `ecc_window` names the doubleword a check reads,
// so the NMI tells that read from a fault anywhere else.
static volatile uint32_t ecc_window;
static volatile bool ecc_failed;

// A read the current buffer or the WB data cache serves checks nothing: RM0444 and RM0434,
// "a new read at the failing address may not generate an ECC error". So a check reads with
// the data cache off, after this doubleword moved the current buffer off the caller's last one
static const volatile uint64_t ecc_spare = 0x5A5A5A5A5A5A5A5Aull;

// `flag` set in either ECC register, `ECC2R` has the flags at the bits of `ECCR`.
// Inside the window it is the read's, whatever address the register holds: cleared just before
// the load, and after a reset that cut a program or an erase the address can be wrong (errata)
static bool ecc_flagged(uint32_t flag)
{
  if(FLASH->ECCR & flag) return true;
  #if defined(STM32G0C1xx)
  if(FLASH->ECC2R & flag) return true;
  #endif
  return false;
}

// A set flag freezes the register, so every read starts from clear ones
static void ecc_clear(void)
{
  FLASH->ECCR = FLASH_ECCR_ECCC | FLASH_ECCR_ECCD;
  #if defined(STM32G0C1xx)
  FLASH->ECC2R = FLASH_ECC2R_ECCC | FLASH_ECC2R_ECCD;
  #endif
}

// Doublewords of the range read once each in the window, `data` (`NULL` = skip) taking
// the bytes of the range from those very reads
static status_t ecc_copy(uint32_t addr, uint8_t *data, uint32_t size, bool *corrected)
{
  if(corrected) *corrected = false;
  #if defined(STM32WB)
  uint32_t dcen = FLASH->ACR & FLASH_ACR_DCEN;
  FLASH->ACR &= ~FLASH_ACR_DCEN;
  #endif
  (void)ecc_spare;
  status_t status = OK;
  for(uint32_t at = addr & ~7u; at < addr + size; at += 8) {
    uint32_t word[2];
    ecc_clear();
    ecc_failed = false;
    ecc_window = at;
    __DSB(); // flags cleared before the load
    word[0] = *(volatile uint32_t *)at; // reads, and ECC checks, the whole doubleword
    word[1] = *(volatile uint32_t *)(at + 4u); // from the same read
    __DSB(); // a pending NMI taken before the window closes
    __ISB();
    ecc_window = 0;
    if(ecc_failed) {
      status = ERR;
      break;
    }
    if(corrected && ecc_flagged(FLASH_ECCR_ECCC)) *corrected = true;
    if(!data) continue;
    const uint8_t *bytes = (const uint8_t *)word;
    for(uint32_t i = 0; i < 8; i++) {
      if(at + i >= addr && at + i < addr + size) data[at + i - addr] = bytes[i];
    }
  }
  ecc_clear();
  #if defined(STM32WB)
  FLASH->ACR |= dcen;
  #endif
  return status;
}

status_t FLASH_Check(uint32_t addr, uint32_t size, bool *corrected)
{
  return ecc_copy(addr, NULL, size, corrected);
}

status_t FLASH_Copy(uint32_t addr, void *data, uint32_t size, bool *corrected)
{
  return ecc_copy(addr, data, size, corrected);
}

// A double ECC error inside the window is the read of the check, which it marks and returns.
// Any other NMI stops the chip, as the default handler.
void NMI_Handler(void)
{
  if(ecc_window && ecc_flagged(FLASH_ECCR_ECCD)) {
    ecc_clear();
    ecc_failed = true;
    return;
  }
  while(1);
}

//------------------------------------------------------------------------------------------- Write

// The two stores must not be split by an interrupt: a gap raises `PROGERR` or `SIZERR`.
// The cycle they start waits with interrupts on, CPU2 can hold it suspended for long.
// Ones program nothing, an erased doubleword reads them already: a cut never leaves a page
// that reads blank, which `FLASH_Erase` spares, with a doubleword that takes no more writes.
status_t FLASH_Write(uint32_t addr, uint32_t data1, uint32_t data2)
{
  if(addr & 7u) return ERR;
  uint32_t word[2];
  if(data1 == 0xFFFFFFFFu && data2 == 0xFFFFFFFFu) {
    if(FLASH_Copy(addr, word, 8, NULL)) return ERR;
    return word[0] == data1 && word[1] == data2 ? OK : ERR;
  }
  if(flash_unlock()) return ERR;
  FLASH->SR = FLASH_CLR_FLAGS;
  FLASH->CR |= FLASH_CR_PG;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  *(volatile uint32_t *)addr = data1;
  *(volatile uint32_t *)(addr + 4u) = data2;
  __DSB();
  __set_PRIMASK(primask);
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PG;
  if(flash_finish()) return ERR;
  // Read back through ECC: a plain read takes the line of the data cache along, and with it
  // a neighbour a cut tore
  if(FLASH_Copy(addr, word, 8, NULL)) return ERR;
  return word[0] == data1 && word[1] == data2 ? OK : ERR;
}

#if defined(STM32G0)

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

#endif

status_t FLASH_WritePage(uint16_t page, const uint8_t *data)
{
  // Fast programming takes only the page erased last, so a blank page is erased again
  if(flash_erase(page)) return ERR;
  uint32_t addr = FLASH_GetAddress(page, 0);
  #if defined(STM32WB)
  // A row program breaks on every CPU2 fetch and ends in `MISERR`, and the radio core
  // outlives a CPU1 reset, so the page goes in double words, the way the EEPROM writes
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
    const uint32_t *d = (const uint32_t *)(data + i);
    if(FLASH_Write(addr + i, d[0], d[1])) return ERR;
  }
  #else
  for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 256) {
    if(FLASH_WriteFast(addr + i, (const uint32_t *)(data + i))) return ERR;
  }
  #endif
  return OK;
}

//-------------------------------------------------------------------------------------------------

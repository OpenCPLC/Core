#ifndef FLASH_G0_H_
#define FLASH_G0_H_

#define FLASH_PAGE_SIZE ((uint32_t)0x00000800)

#ifdef STM32G081xx
  #define FLASH_PAGES 64
#endif
#ifdef STM32G0C1xx
  #define FLASH_PAGES 256
#endif

#endif
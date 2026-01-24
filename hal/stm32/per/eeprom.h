#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <stdarg.h>
#include "flash.h"
#include "xdef.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

typedef enum {
  EEPROM_State_NONE,
  EEPROM_State_EMPTY,
  EEPROM_State_FILLED,
  EEPROM_State_FULL,
} EEPROM_State_t;

typedef enum {
  EEPROM_Storage_A = 0,
  EEPROM_Storage_B = 1
} EEPROM_Storage_t;

/**
 * @brief EEPROM emulation descriptor
 * @param[in] page_start First flash page reserved for EEPROM 
 * @param[in] page_count Number of flash pages reserved for EEPROM (must be even)
 * @param storage_pages Number of pages per storage block (`page_count` / `2`)
 * @param adrr_start Start addresses for each storage block [A,B]
 * @param adrr_end  End addresses for each storage block [A,B]
 * @param active_storage Currently active storage block
 * @param cursor Current write cursor inside active block
 */
typedef struct {
  uint8_t page_start;
  uint8_t page_count;
  uint8_t storage_pages;
  uint32_t adrr_start[2];
  uint32_t adrr_end[2];
  EEPROM_Storage_t active_storage;
  uint32_t cursor;
} EEPROM_t;

status_t EEPROM_Init(EEPROM_t *eeprom);
status_t EEPROM_Clear(EEPROM_t *eeprom);
status_t EEPROM_Write(EEPROM_t *eeprom, uint32_t key, uint32_t value);
uint32_t EEPROM_Read(EEPROM_t *eeprom, uint32_t key, uint32_t default_value);
status_t EEPROM_Save(EEPROM_t *eeprom, uint32_t *var);
status_t EEPROM_Load(EEPROM_t *eeprom, uint32_t *var);
status_t EEPROM_SaveList(EEPROM_t *eeprom, uint32_t *var, ...);
status_t EEPROM_LoadList(EEPROM_t *eeprom, uint32_t *var, ...);
status_t EEPROM_Save64(EEPROM_t *eeprom, uint64_t *var);
status_t EEPROM_Load64(EEPROM_t *eeprom, uint64_t *var);
status_t EEPROM_WriteF32(EEPROM_t *eeprom, uint32_t key, float value);
float EEPROM_ReadF32(EEPROM_t *eeprom, uint32_t key, float value);

status_t CACHE_Init(EEPROM_t *eeprom);
status_t CACHE_Clear(void);
status_t CACHE_Write(uint32_t key, uint32_t value);
uint32_t CACHE_Read(uint32_t key, uint32_t default_value);
status_t CACHE_Save(uint32_t *var);
status_t CACHE_Load(uint32_t *var);
status_t CACHE_SaveList(uint32_t *var, ...);
status_t CACHE_LoadList(uint32_t *var, ...);
status_t CACHE_Save64(uint64_t *var);
status_t CACHE_Load64(uint64_t *var);
status_t CACHE_WriteF32(uint32_t key, float value);
float CACHE_ReadF32(uint32_t key, float value);


//-------------------------------------------------------------------------------------------------
#endif

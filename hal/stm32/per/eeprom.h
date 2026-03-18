// hal/stm32/per/eeprom.h

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <stdarg.h>
#include "flash.h"
#include "xdef.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

typedef enum {
  EEPROM_State_None,
  EEPROM_State_Empty,
  EEPROM_State_Filled,
  EEPROM_State_Full,
} EEPROM_State_t;

typedef enum {
  EEPROM_Storage_A = 0,
  EEPROM_Storage_B = 1
} EEPROM_Storage_t;

/**
 * @brief EEPROM emulation descriptor.
 * @param[in] page_start First flash page reserved for EEPROM
 * @param[in] page_count Number of flash pages (must be even, split A/B)
 */
typedef struct {
  uint8_t page_start;
  uint8_t page_count;
  // internal
  uint8_t _storage_pages;
  uint32_t _addr_start[2];
  uint32_t _addr_end[2];
  EEPROM_Storage_t _active;
  uint32_t _cursor;
} EEPROM_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize EEPROM emulation.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @return `OK` on success, `ERR` on invalid config
 */
status_t EEPROM_Init(EEPROM_t *eeprom);

/**
 * @brief Erase all EEPROM pages (both A and B).
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Clear(EEPROM_t *eeprom);

/**
 * @brief Write key/value pair to EEPROM.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] key Entry key
 * @param[in] value Entry value
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Write(EEPROM_t *eeprom, uint32_t key, uint32_t value);

/**
 * @brief Read value by key from EEPROM.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[in] key Entry key
 * @param[in] default_value Returned if key not found
 * @return Stored value or `default_value`
 */
uint32_t EEPROM_Read(EEPROM_t *eeprom, uint32_t key, uint32_t default_value);

/**
 * @brief Save variable (uses address as key).
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] var Pointer to variable
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Save(EEPROM_t *eeprom, uint32_t *var);

/**
 * @brief Load variable (uses address as key).
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[out] var Pointer to variable (updated if found)
 * @return `OK` if found, `ERR` if not found
 */
status_t EEPROM_Load(EEPROM_t *eeprom, uint32_t *var);

/**
 * @brief Save multiple variables (NULL-terminated list).
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] var First variable, then more via `...`, end with `NULL`
 * @return `OK` if all saved, `ERR` if any failed
 */
status_t EEPROM_SaveList(EEPROM_t *eeprom, uint32_t *var, ...);

/**
 * @brief Load multiple variables (NULL-terminated list).
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[out] var First variable, then more via `...`, end with `NULL`
 * @return `OK` if all found, `ERR` if any missing
 */
status_t EEPROM_LoadList(EEPROM_t *eeprom, uint32_t *var, ...);

/**
 * @brief Save 64-bit variable.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] var Pointer to 64-bit variable
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Save64(EEPROM_t *eeprom, uint64_t *var);

/**
 * @brief Load 64-bit variable.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[out] var Pointer to 64-bit variable
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_Load64(EEPROM_t *eeprom, uint64_t *var);

/**
 * @brief Write float value to EEPROM.
 * @param[in,out] eeprom Pointer to `EEPROM_t` instance
 * @param[in] key Entry key
 * @param[in] value Float value
 * @return `OK` on success, `ERR` on error
 */
status_t EEPROM_WriteF32(EEPROM_t *eeprom, uint32_t key, float value);

/**
 * @brief Read float value from EEPROM.
 * @param[in] eeprom Pointer to `EEPROM_t` instance
 * @param[in] key Entry key
 * @param[in] default_value Returned if key not found
 * @return Stored float or `default_value`
 */
float EEPROM_ReadF32(EEPROM_t *eeprom, uint32_t key, float default_value);

//-------------------------------------------------------------------------------------------------

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
float CACHE_ReadF32(uint32_t key, float default_value);

//-------------------------------------------------------------------------------------------------

#endif
// hal/stm32/per/crc.h

#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>
#include <stdbool.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#elif defined(STM32G4)
  #include "stm32g4xx.h"
#endif
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------- Compatibility

#if defined(STM32G0)
  #define RCC_CRC_EN()  (RCC->AHBENR |= RCC_AHBENR_CRCEN)
#elif defined(STM32WB) || defined(STM32G4)
  #define RCC_CRC_EN()  (RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN)
#endif

//-------------------------------------------------------------------------------------- Config

#ifndef CRC_PRESETS
  #define CRC_PRESETS 1
#endif

//--------------------------------------------------------------------------------------- Types

/**
 * @brief CRC algorithm configuration.
 * @param[in] width CRC width in bits (8, 16, or 32)
 * @param[in] polynomial Generator polynomial
 * @param[in] initial Initial CRC register value
 * @param[in] reflect_data_in Bit-reflect input bytes (0 = none, 8/16/32 = width)
 * @param[in] reflect_data_out Bit-reflect final CRC
 * @param[in] final_xor XOR mask applied to final CRC
 * @param[in] invert_out Invert final CRC byte order
 */
typedef struct {
  uint8_t width;
  uint32_t polynomial;
  uint32_t initial;
  uint8_t reflect_data_in;
  bool reflect_data_out;
  uint32_t final_xor;
  bool invert_out;
} CRC_t;

//---------------------------------------------------------------------------------------- API

/**
 * @brief Calculate CRC checksum.
 * @param[in] crc CRC algorithm configuration
 * @param[in] data Pointer to input data
 * @param[in] count Data length in bytes
 * @return CRC checksum
 */
uint32_t CRC_Run(const CRC_t *crc, void *data, uint16_t count);

/**
 * @brief Calculate CRC and append to data (big-endian).
 * @param[in] crc CRC algorithm configuration
 * @param[in,out] data Data buffer (must have space for CRC)
 * @param[in] count Data length in bytes (without CRC)
 * @return New length including CRC bytes
 */
uint16_t CRC_Append(const CRC_t *crc, uint8_t *data, uint16_t count);

/**
 * @brief Verify CRC at end of data.
 * @param[in] crc CRC algorithm configuration
 * @param[in] data Data buffer with CRC appended
 * @param[in] count Total length including CRC bytes
 * @return `ERR` if mismatch, `OK` if valid
 */
status_t CRC_Error(const CRC_t *crc, uint8_t *data, uint16_t count);

/**
 * @brief Verify CRC at end of data.
 * @param[in] crc CRC algorithm configuration
 * @param[in] data Data buffer with CRC appended
 * @param[in] count Total length including CRC bytes
 * @return `OK` if valid, `ERR` if mismatch
 */
status_t CRC_Ok(const CRC_t *crc, uint8_t *data, uint16_t count);

//------------------------------------------------------------------------------------- Presets

#if(CRC_PRESETS)
  extern const CRC_t crc32_iso;
  extern const CRC_t crc32_aixm;
  extern const CRC_t crc32_autosar;
  extern const CRC_t crc32_cksum;
  extern const CRC_t crc16_kermit;
  extern const CRC_t crc16_modbus;
  extern const CRC_t crc16_buypass;
  extern const CRC_t crc8_maxim;
  extern const CRC_t crc8_smbus;
#endif

//---------------------------------------------------------------------------------------------
#endif
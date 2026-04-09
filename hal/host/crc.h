// hal/host/crc.h

#ifndef CRC_H_
#define CRC_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "xdef.h"

//------------------------------------------------------------------------------------------------- Config

#ifndef CRC_PRESETS
  #define CRC_PRESETS ON
#endif

//------------------------------------------------------------------------------------------------- Types

/**
 * @brief CRC algorithm configuration.
 * @param[in] width CRC width in bits (8, 16, or 32)
 * @param[in] polynomial Generator polynomial
 * @param[in] initial Initial CRC register value
 * @param[in] reflect_data_in Bit-reflect input bytes (0 = none, 8/16/32 = width)
 * @param[in] reflect_data_out Bit-reflect final CRC
 * @param[in] final_xor XOR mask applied to final CRC
 * @param[in] invert_out Invert final CRC byte order
 * @param[in,out] table Pointer to 256-entry lookup table buffer
 */
typedef struct {
  uint8_t width;
  uint32_t polynomial;
  uint32_t initial;
  uint8_t reflect_data_in;
  bool reflect_data_out;
  uint32_t final_xor;
  bool invert_out;
  uint32_t *table;
} CRC_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Build lookup table for CRC instance. Must be called before `CRC_Run`.
 * @param[in,out] crc CRC instance with `table` pointing to 256-entry buffer
 */
void CRC_Init(CRC_t *crc);

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

//------------------------------------------------------------------------------------------------- Presets
#if(CRC_PRESETS)

  extern CRC_t crc32_iso;
  extern CRC_t crc32_aixm;
  extern CRC_t crc32_autosar;
  extern CRC_t crc32_cksum;
  extern CRC_t crc16_kermit;
  extern CRC_t crc16_modbus;
  extern CRC_t crc16_buypass;
  extern CRC_t crc8_maxim;
  extern CRC_t crc8_smbus;

#endif
//-------------------------------------------------------------------------------------------------
#endif
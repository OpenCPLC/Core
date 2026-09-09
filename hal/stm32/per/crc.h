// hal/stm32/per/crc.h

#ifndef CRC_H_
#define CRC_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef CRC_PRESETS
  // Preset tables below, off saves their flash
  #define CRC_PRESETS 1
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief CRC algorithm, run on the hardware unit.
 * @param[in] width CRC width [bits], `8`, `16` or `32`
 * @param[in] polynomial Generator polynomial
 * @param[in] initial Initial register value
 * @param[in] reflect_data_in Bit-reflect input words, `0` = none, `8`, `16` or `32` bits
 * @param[in] reflect_data_out Bit-reflect the final value
 * @param[in] final_xor Mask applied to the final value
 * @param[in] invert_out Swap the byte order of the final value
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

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Checksum of a byte range.
 * @param[in] crc Algorithm
 * @param[in] data Bytes
 * @param[in] count Number of bytes, a whole flash image fits
 * @return Checksum
 */
uint32_t CRC_Run(const CRC_t *crc, const void *data, uint32_t count);

/**
 * @brief Append the checksum big-endian after the data.
 * @param[in] crc Algorithm
 * @param[in,out] data Bytes with room for the checksum behind them
 * @param[in] count Number of bytes without the checksum
 * @return Length with the checksum
 */
uint16_t CRC_Append(const CRC_t *crc, uint8_t *data, uint16_t count);

/**
 * @brief Verify the checksum at the end of the data.
 * @param[in] crc Algorithm
 * @param[in] data Bytes with the checksum appended
 * @param[in] count Length with the checksum
 * @return `OK` when valid, `ERR` on a mismatch or a range shorter than the checksum
 */
status_t CRC_Error(const CRC_t *crc, const uint8_t *data, uint16_t count);

// `true` when the checksum at the end of the data is valid
bool CRC_Ok(const CRC_t *crc, const uint8_t *data, uint16_t count);

//----------------------------------------------------------------------------------------- Presets

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

//-------------------------------------------------------------------------------------------------
#endif

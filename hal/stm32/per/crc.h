#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>
#include <stdbool.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "xdef.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef CRC_PRESETS
  #define CRC_PRESETS 1
#endif

/**
 * @brief CRC algorithm configuration.
 * @param width CRC width in bits (8, 16, or 32).
 * @param polynomial Generator polynomial.
 * @param initial Initial CRC register value.
 * @param reflect_data_in Bit-reflect input bytes (0 = none, 8/16/32 = width).
 * @param reflect_data_out Bit-reflect final CRC.
 * @param final_xor XOR mask applied to final CRC.
 * @param invert_out Invert final CRC bits.
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

uint32_t CRC_Run(const CRC_t *crc, void *data, uint16_t count);
uint16_t CRC_Append(const CRC_t *crc, uint8_t *data, uint16_t count);
status_t CRC_Error(const CRC_t *crc, uint8_t *data, uint16_t count);
status_t CRC_Ok(const CRC_t *crc, uint8_t *data, uint16_t count);

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

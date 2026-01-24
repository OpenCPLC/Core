#include "crc.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Calculates CRC checksum.
 * @param[in] crc CRC module configuration.
 * @param[in] data Pointer to input data.
 * @param[in] count Data length in bytes.
 * @return CRC checksum.
 */
uint32_t CRC_Run(const CRC_t *crc, void *data, uint16_t count)
{
  uint32_t out;
  uint8_t *pointner = (uint8_t *)&out;
  CRC->CR &= 0xFFFFFF07;
  RCC->AHBENR |= RCC_AHBENR_CRCEN;
  CRC->POL = crc->polynomial;
  CRC->INIT = crc->initial;
  switch(crc->width) {
    case 8:  CRC->CR = (2 << CRC_CR_POLYSIZE_Pos); break;
    case 16: CRC->CR = (1 << CRC_CR_POLYSIZE_Pos); break;
    case 32: CRC->CR = 0; break;
  }
  switch(crc->reflect_data_in) {
    case 8:  CRC->CR |= (1 << CRC_CR_REV_IN_Pos); break;
    case 16: CRC->CR |= (2 << CRC_CR_REV_IN_Pos); break;
    case 32: CRC->CR |= (3 << CRC_CR_REV_IN_Pos); break;
  }
  CRC->CR |= (crc->reflect_data_out << CRC_CR_REV_OUT_Pos) | CRC_CR_RESET;
  __DSB();
  while(count--) {
    *(volatile uint8_t *) &CRC->DR = *(const uint8_t *)(data);
    data = (void *)((const uint8_t *)data + 1);
  }
  out = (CRC->DR ^ crc->final_xor);
  if(crc->invert_out) {
    switch(crc->width) {
      case 32: return (pointner[0] << 24) | (pointner[1] << 16) | (pointner[2] << 8) | pointner[3];
      case 16: return (pointner[0] << 8) | pointner[1];
    }
  }
  return out;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Calculates CRC and appends it to data (big-endian).
 * @param[in] crc CRC module configuration.
 * @param[in,out] data Data buffer (must have space for CRC).
 * @param[in] count Data length in bytes (without CRC).
 * @return New length including CRC bytes.
 */
uint16_t CRC_Append(const CRC_t *crc, uint8_t *data, uint16_t count)
{
  uint32_t code = CRC_Run(crc, (void *)data, count);
  switch(crc->width) {
    case 32:
      data[count++] = (uint8_t)(code >> 24);
      data[count++] = (uint8_t)(code >> 16);
    case 16:
      data[count++] = (uint8_t)(code >> 8);
    case 8:
      data[count++] = (uint8_t)code;
  }
  return count;
}

/**
 * @brief Verifies CRC at end of data (big-endian).
 * @param[in] crc CRC module configuration.
 * @param[in] data Data buffer with CRC appended.
 * @param[in] count Total length including CRC bytes.
 * @return `OK` if valid, `ERR` if mismatch.
 */
status_t CRC_Error(const CRC_t *crc, uint8_t *data, uint16_t count)
{
  count -= crc->width / 8;
  uint32_t code = CRC_Run(crc, (void *)data, count);

  switch(crc->width) {
    case 32:
      if(data[count++] != (uint8_t)(code >> 24)) return ERR;
      if(data[count++] != (uint8_t)(code >> 16)) return ERR;
    case 16:
      if(data[count++] != (uint8_t)(code >> 8)) return ERR;
    case 8:
      if(data[count++] != (uint8_t)code) return ERR;
  }
  return OK;
}

/**
 * @brief Verifies CRC at end of data (big-endian).
 * @param[in] crc CRC module configuration.
 * @param[in] data Data buffer with CRC appended.
 * @param[in] count Total length including CRC bytes.
 * @return `OK` if valid, `ERR` if mismatch.
 */
status_t CRC_Ok(const CRC_t *crc, uint8_t *data, uint16_t count)
{
  return !CRC_Error(crc, data, count);
}

//-------------------------------------------------------------------------------------------------
#if(CRC_PRESETS)

/** @brief CRC-32 ISO 3309, used in Ethernet, ZIP, PNG */
const CRC_t crc32_iso = {
  .width = 32,
  .polynomial = 0x04C11DB7,
  .initial = 0xFFFFFFFF,
  .reflect_data_in = 32,
  .reflect_data_out = true,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

/** @brief CRC-32 AIXM, aviation data exchange */
const CRC_t crc32_aixm = {
  .width = 32,
  .polynomial = 0x814141AB,
  .initial = 0x00000000,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00000000,
  .invert_out = false
};

/** @brief CRC-32 AUTOSAR, automotive E2E protection */
const CRC_t crc32_autosar = {
  .width = 32,
  .polynomial = 0xF4ACFB13,
  .initial = 0xFFFFFFFF,
  .reflect_data_in = 32,
  .reflect_data_out = true,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

/** @brief CRC-32 POSIX cksum command */
const CRC_t crc32_cksum = {
  .width = 32,
  .polynomial = 0x04C11DB7,
  .initial = 0x00000000,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

/** @brief CRC-16 Kermit, file transfer protocol */
const CRC_t crc16_kermit = {
  .width = 16,
  .polynomial = 0x1021,
  .initial = 0x0000,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0x0000,
  .invert_out = false
};

/** @brief CRC-16 Modbus RTU, industrial automation */
const CRC_t crc16_modbus = {
  .width = 16,
  .polynomial = 0x8005,
  .initial = 0xFFFF,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0x0000,
  .invert_out = true
};

/** @brief CRC-16 Buypass, payment systems */
const CRC_t crc16_buypass = {
  .width = 16,
  .polynomial = 0x8005,
  .initial = 0x0000,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x0000,
  .invert_out = false
};

/** @brief CRC-8 Maxim/Dallas, 1-Wire devices */
const CRC_t crc8_maxim = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0x00,
  .reflect_data_in = 8,
  .reflect_data_out = true,
  .final_xor = 0x00,
  .invert_out = false
};

/** @brief CRC-8 SMBus, system management bus */
const CRC_t crc8_smbus = {
  .width = 8,
  .polynomial = 0x07,
  .initial = 0x00,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

#endif
//-------------------------------------------------------------------------------------------------

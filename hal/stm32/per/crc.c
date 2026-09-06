// hal/stm32/per/crc.c

#include "crc.h"

#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "pwr.h"

//--------------------------------------------------------------------------------------------- API

uint32_t CRC_Run(const CRC_t *crc, const void *data, uint32_t count)
{
  RCC_EnableCRC();
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
  const uint8_t *bytes = data;
  while(count--) *(volatile uint8_t *)&CRC->DR = *bytes++;
  uint32_t out = CRC->DR ^ crc->final_xor;
  if(!crc->invert_out) return out;
  switch(crc->width) {
    case 32: return __REV(out);
    case 16: return __REV16(out) & 0xFFFFu;
    default: return out;
  }
}

uint16_t CRC_Append(const CRC_t *crc, uint8_t *data, uint16_t count)
{
  uint32_t code = CRC_Run(crc, data, count);
  switch(crc->width) {
    case 32:
      data[count++] = (uint8_t)(code >> 24);
      data[count++] = (uint8_t)(code >> 16);
      fallthrough;
    case 16:
      data[count++] = (uint8_t)(code >> 8);
      fallthrough;
    case 8:
      data[count++] = (uint8_t)code;
  }
  return count;
}

status_t CRC_Error(const CRC_t *crc, const uint8_t *data, uint16_t count)
{
  // `count` spans the frame including its checksum, shorter would wrap the subtraction
  if(count < crc->width / 8) return ERR;
  count -= crc->width / 8;
  uint32_t code = CRC_Run(crc, data, count);
  switch(crc->width) {
    case 32:
      if(data[count++] != (uint8_t)(code >> 24)) return ERR;
      if(data[count++] != (uint8_t)(code >> 16)) return ERR;
      fallthrough;
    case 16:
      if(data[count++] != (uint8_t)(code >> 8)) return ERR;
      fallthrough;
    case 8:
      if(data[count++] != (uint8_t)code) return ERR;
  }
  return OK;
}

bool CRC_Ok(const CRC_t *crc, const uint8_t *data, uint16_t count)
{
  return CRC_Error(crc, data, count) == OK;
}

//----------------------------------------------------------------------------------------- Presets
#if(CRC_PRESETS)

const CRC_t crc32_iso = {
  .width = 32,
  .polynomial = 0x04C11DB7,
  .initial = 0xFFFFFFFF,
  .reflect_data_in = 32,
  .reflect_data_out = true,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

const CRC_t crc32_aixm = {
  .width = 32,
  .polynomial = 0x814141AB,
  .initial = 0x00000000,
  .reflect_data_in = 0,
  .reflect_data_out = false,
  .final_xor = 0x00000000,
  .invert_out = false
};

const CRC_t crc32_autosar = {
  .width = 32,
  .polynomial = 0xF4ACFB13,
  .initial = 0xFFFFFFFF,
  .reflect_data_in = 32,
  .reflect_data_out = true,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

const CRC_t crc32_cksum = {
  .width = 32,
  .polynomial = 0x04C11DB7,
  .initial = 0x00000000,
  .reflect_data_in = 0,
  .reflect_data_out = false,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

const CRC_t crc16_kermit = {
  .width = 16,
  .polynomial = 0x1021,
  .initial = 0x0000,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0x0000,
  .invert_out = false
};

const CRC_t crc16_modbus = {
  .width = 16,
  .polynomial = 0x8005,
  .initial = 0xFFFF,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0x0000,
  .invert_out = true
};

const CRC_t crc16_buypass = {
  .width = 16,
  .polynomial = 0x8005,
  .initial = 0x0000,
  .reflect_data_in = 0,
  .reflect_data_out = false,
  .final_xor = 0x0000,
  .invert_out = false
};

const CRC_t crc8_maxim = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0x00,
  .reflect_data_in = 8,
  .reflect_data_out = true,
  .final_xor = 0x00,
  .invert_out = false
};

const CRC_t crc8_smbus = {
  .width = 8,
  .polynomial = 0x07,
  .initial = 0x00,
  .reflect_data_in = 0,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

#endif
//-------------------------------------------------------------------------------------------------

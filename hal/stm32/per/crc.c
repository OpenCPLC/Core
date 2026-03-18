// hal/stm32/per/crc.c

#include "crc.h"

//-------------------------------------------------------------------------------------------------

uint32_t CRC_Run(const CRC_t *crc, void *data, uint16_t count)
{
  uint32_t out;
  uint8_t *pointner = (uint8_t *)&out;
  CRC->CR &= 0xFFFFFF07;
  RCC_CRC_EN();
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

uint16_t CRC_Append(const CRC_t *crc, uint8_t *data, uint16_t count)
{
  uint32_t code = CRC_Run(crc, (void *)data, count);
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

status_t CRC_Ok(const CRC_t *crc, uint8_t *data, uint16_t count)
{
  return !CRC_Error(crc, data, count);
}

//-------------------------------------------------------------------------------------------------
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
  .reflect_data_in = false,
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
  .reflect_data_in = false,
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
  .reflect_data_in = false,
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
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

#endif
//-------------------------------------------------------------------------------------------------
// hal/host/crc.c

#include "crc.h"

//------------------------------------------------------------------------------------------------- Internal

static uint32_t get_crc_mask(uint8_t width)
{
  switch(width) {
    case 8:  return 0xFF;
    case 16: return 0xFFFF;
    case 32: return 0xFFFFFFFF;
    default: return 0;
  }
}

static uint32_t reflect_bits(uint32_t data, uint8_t width)
{
  uint32_t reflection = 0;
  for(uint8_t bit = 0; bit < width; bit++) {
    if(data & 1) reflection |= (1u << ((width - 1) - bit));
    data >>= 1;
  }
  return reflection;
}

//-------------------------------------------------------------------------------------------------

void CRC_Init(CRC_t *crc)
{
  uint32_t topbit = 1u << (crc->width - 1);
  uint32_t mask = get_crc_mask(crc->width);
  for(uint16_t i = 0; i < 256; i++) {
    uint32_t remainder = (uint32_t)i << (crc->width - 8);
    for(uint8_t bit = 0; bit < 8; bit++) {
      if(remainder & topbit) remainder = (remainder << 1) ^ crc->polynomial;
      else remainder <<= 1;
    }
    crc->table[i] = remainder & mask;
  }
}

uint32_t CRC_Run(const CRC_t *crc, void *data, uint16_t count)
{
  uint32_t mask = get_crc_mask(crc->width);
  uint32_t remainder = crc->initial;
  uint8_t *bytes = (uint8_t *)data;
  for(uint16_t i = 0; i < count; i++) {
    uint8_t byte = bytes[i];
    if(crc->reflect_data_in) byte = (uint8_t)reflect_bits(byte, 8);
    uint8_t idx = (uint8_t)((byte ^ (remainder >> (crc->width - 8))) & 0xFF);
    remainder = crc->table[idx] ^ (remainder << 8);
    remainder &= mask;
  }
  if(crc->reflect_data_out) remainder = reflect_bits(remainder, crc->width);
  remainder ^= crc->final_xor;
  if(crc->invert_out) {
    uint8_t *p = (uint8_t *)&remainder;
    switch(crc->width) {
      case 32: return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
      case 16: return ((uint32_t)p[0] << 8) | p[1];
    }
  }
  return remainder;
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
      fallthrough;
    case 16:
      if(data[count++] != (uint8_t)(code >> 8)) return ERR;
      fallthrough;
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

CRC_t crc32_iso = {
  .width = 32,
  .polynomial = 0x04C11DB7,
  .initial = 0xFFFFFFFF,
  .reflect_data_in = 32,
  .reflect_data_out = true,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

CRC_t crc32_aixm = {
  .width = 32,
  .polynomial = 0x814141AB,
  .initial = 0x00000000,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00000000,
  .invert_out = false
};

CRC_t crc32_autosar = {
  .width = 32,
  .polynomial = 0xF4ACFB13,
  .initial = 0xFFFFFFFF,
  .reflect_data_in = 32,
  .reflect_data_out = true,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

CRC_t crc32_cksum = {
  .width = 32,
  .polynomial = 0x04C11DB7,
  .initial = 0x00000000,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0xFFFFFFFF,
  .invert_out = false
};

CRC_t crc16_kermit = {
  .width = 16,
  .polynomial = 0x1021,
  .initial = 0x0000,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0x0000,
  .invert_out = false
};

CRC_t crc16_modbus = {
  .width = 16,
  .polynomial = 0x8005,
  .initial = 0xFFFF,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0x0000,
  .invert_out = true
};

CRC_t crc16_buypass = {
  .width = 16,
  .polynomial = 0x8005,
  .initial = 0x0000,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x0000,
  .invert_out = false
};

CRC_t crc8_maxim = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0x00,
  .reflect_data_in = 8,
  .reflect_data_out = true,
  .final_xor = 0x00,
  .invert_out = false
};

CRC_t crc8_smbus = {
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
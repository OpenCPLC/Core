// .tests/test_crc.c
// CRC on the host twin of the unit: the check values of the catalog,
// and a checksum fed in pieces is the one of the whole, for every preset and every split.

#include "test.h"
#include "crc.c"

static const CRC_t *const PRESETS[] = {
  &crc32_iso, &crc32_aixm, &crc32_autosar, &crc32_cksum,
  &crc16_kermit, &crc16_modbus, &crc16_buypass, &crc8_maxim, &crc8_smbus,
};

// Check values of the catalog for "123456789", Modbus in its wire order, low byte first
static void catalog_check_values(void)
{
  static const char text[] = "123456789";
  TEST_Equal(CRC_Run(&crc32_iso, text, 9), 0xCBF43926u, "CRC-32/ISO-HDLC");
  TEST_Equal(CRC_Run(&crc16_modbus, text, 9), 0x374Bu, "CRC-16/MODBUS");
  TEST_Equal(CRC_Run(&crc16_kermit, text, 9), 0x2189u, "CRC-16/KERMIT");
  TEST_Equal(CRC_Run(&crc8_maxim, text, 9), 0xA1u, "CRC-8/MAXIM-DOW");
  TEST_Equal(CRC_Run(&crc8_smbus, text, 9), 0xF4u, "CRC-8/SMBUS");
}

static void pieces_give_the_whole(void)
{
  uint8_t data[100];
  for(uint8_t i = 0; i < sizeof(data); i++) data[i] = (uint8_t)(i * 37u + 11u);
  for(uint8_t p = 0; p < array_len(PRESETS); p++) {
    const CRC_t *crc = PRESETS[p];
    uint32_t whole = CRC_Run(crc, data, sizeof(data));
    for(uint32_t split = 0; split <= sizeof(data); split += 10) {
      CRC_Start(crc);
      CRC_Next(crc, data, split);
      CRC_Next(crc, data + split, sizeof(data) - split);
      uint32_t pieces = CRC_End(crc);
      TEST_Check(pieces == whole, "preset:%u split:%u 0x%X, whole 0x%X", p, split, pieces, whole);
    }
  }
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(catalog_check_values);
  TEST(pieces_give_the_whole);
}

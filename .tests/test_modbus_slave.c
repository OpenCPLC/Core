// .tests/test_modbus_slave.c
// Modbus RTU slave against the frames a master sends, answered as the protocol specifies:
// registers and coils read and written, coils across register boundaries,
// the last coil byte padded with zeros, the write mask honoured,
// a frame with a bad CRC or for another address left unanswered,
// and an exception for a function the slave lacks.

#include <string.h>

#include "test.h"
#include "plc/com/modbus_slave.c"

#define SLAVE 0x11

static uint16_t regs[8];
static bool updated[8];
static const bool writable[8] = { true, true, true, true, true, true, false, true };

static uint8_t rx_memory[300];
static BUFF_t rx = { .memory = rx_memory, .size = sizeof(rx_memory) };
static UART_t port = { .UART_19200, .buff = &rx };

static MODBUS_Slave_t slave = {
  .uart = &port,
  .address = SLAVE,
  .reg_read = regs,
  .reg_write = regs,
  .reg_count = array_len(regs),
  .write_mask = writable,
  .update_flag = updated
};

// Registers set as listed, the rest zero, no update pending, nothing on the line
static void registers(uint16_t r0, uint16_t r1)
{
  UART_HostSent(&port, NULL);
  memset(regs, 0, sizeof(regs));
  memset(updated, 0, sizeof(updated));
  regs[0] = r0;
  regs[1] = r1;
  slave.update_any = false;
}

// One frame the way a master puts it on the line: the bytes, then their CRC
static MODBUS_Status_t request(const uint8_t *frame, uint8_t len)
{
  uint8_t bytes[260];
  memcpy(bytes, frame, len);
  len = (uint8_t)CRC_Append(&crc16_modbus, bytes, len);
  for(uint8_t i = 0; i < len; i++) BUFF_Push(&rx, bytes[i]);
  BUFF_Break(&rx);
  return MODBUS_Loop(&slave);
}

// What the slave put on the line since the last frame is `frame` with its CRC
static void answered(const uint8_t *frame, uint8_t len, const char *msg)
{
  uint8_t expected[260], sent[256];
  memcpy(expected, frame, len);
  len = (uint8_t)CRC_Append(&crc16_modbus, expected, len);
  TEST_Check(UART_HostSent(&port, sent) == len, "%s, length", msg);
  TEST_Bytes(sent, expected, len, msg);
}

//--------------------------------------------------------------------------------------- Registers

static void reads_registers(void)
{
  registers(0x000A, 0x0014);
  regs[2] = 0xBEEF;
  static const uint8_t holding[] = { SLAVE, 0x03, 0x00, 0x01, 0x00, 0x02 };
  TEST_Check(request(holding, sizeof(holding)) == MODBUS_Status_Handled);
  static const uint8_t holding_reply[] = { SLAVE, 0x03, 0x04, 0x00, 0x14, 0xBE, 0xEF };
  answered(holding_reply, sizeof(holding_reply), "two holding registers from 1");
  static const uint8_t input[] = { SLAVE, 0x04, 0x00, 0x00, 0x00, 0x01 };
  TEST_Check(request(input, sizeof(input)) == MODBUS_Status_Handled);
  static const uint8_t input_reply[] = { SLAVE, 0x04, 0x02, 0x00, 0x0A };
  answered(input_reply, sizeof(input_reply), "one input register");
}

// A write lands, raises the flag of its register and the one the application polls
static void writes_registers(void)
{
  registers(0, 0);
  static const uint8_t single[] = { SLAVE, 0x06, 0x00, 0x02, 0x12, 0x34 };
  TEST_Check(request(single, sizeof(single)) == MODBUS_Status_Handled);
  answered(single, sizeof(single), "single register echoed");
  TEST_Check(regs[2] == 0x1234 && updated[2], "register 2 written");
  TEST_Check(MODBUS_HasUpdate(&slave) && !MODBUS_HasUpdate(&slave), "one update reported");
  static const uint8_t multiple[] = {
    SLAVE, 0x10, 0x00, 0x03, 0x00, 0x02, 0x04, 0xAB, 0xCD, 0x00, 0x05
  };
  TEST_Check(request(multiple, sizeof(multiple)) == MODBUS_Status_Handled);
  static const uint8_t multiple_reply[] = { SLAVE, 0x10, 0x00, 0x03, 0x00, 0x02 };
  answered(multiple_reply, sizeof(multiple_reply), "start and count of the write");
  TEST_Check(regs[3] == 0xABCD && regs[4] == 0x0005 && updated[3] && updated[4], "written");
}

// A register the mask protects keeps its value, the master's write raises no update
static void write_mask_protects_a_register(void)
{
  registers(0, 0);
  regs[6] = 0x0042;
  static const uint8_t single[] = { SLAVE, 0x06, 0x00, 0x06, 0x00, 0x99 };
  request(single, sizeof(single));
  static const uint8_t multiple[] = {
    SLAVE, 0x10, 0x00, 0x05, 0x00, 0x02, 0x04, 0x00, 0x01, 0x00, 0x02
  };
  request(multiple, sizeof(multiple));
  TEST_Check(regs[6] == 0x0042 && !updated[6], "protected register");
  TEST_Check(regs[5] == 0x0001 && updated[5], "writable register beside it");
}

//------------------------------------------------------------------------------------------- Coils

// Coil `n` is bit `n % 16` of register `n / 16`, the first coil the lowest bit of a byte
static void reads_coils_across_registers(void)
{
  registers(0xA000, 0x0005);
  static const uint8_t coils[] = { SLAVE, 0x01, 0x00, 12, 0x00, 8 };
  TEST_Check(request(coils, sizeof(coils)) == MODBUS_Status_Handled);
  static const uint8_t reply[] = { SLAVE, 0x01, 0x01, 0x5A };
  answered(reply, sizeof(reply), "coils 12 to 19");
}

// Bits of the last byte past the coils asked for are zero
static void coil_read_pads_with_zeros(void)
{
  registers(0xFFFF, 0xFFFF);
  static const uint8_t coils[] = { SLAVE, 0x01, 0x00, 0x00, 0x00, 3 };
  TEST_Check(request(coils, sizeof(coils)) == MODBUS_Status_Handled);
  static const uint8_t reply[] = { SLAVE, 0x01, 0x01, 0x07 };
  answered(reply, sizeof(reply), "three coils, five bits of padding");
}

static void writes_coils_across_registers(void)
{
  registers(0x0000, 0xFFFF);
  static const uint8_t single[] = { SLAVE, 0x05, 0x00, 3, 0xFF, 0x00 };
  TEST_Check(request(single, sizeof(single)) == MODBUS_Status_Handled);
  answered(single, sizeof(single), "single coil echoed");
  TEST_Check(regs[0] == 0x0008, "coil 3 on");
  static const uint8_t multiple[] = { SLAVE, 0x0F, 0x00, 14, 0x00, 4, 0x01, 0x05 };
  TEST_Check(request(multiple, sizeof(multiple)) == MODBUS_Status_Handled);
  static const uint8_t reply[] = { SLAVE, 0x0F, 0x00, 14, 0x00, 4 };
  answered(reply, sizeof(reply), "start and count of the write");
  TEST_Check(regs[0] == 0x4008 && regs[1] == 0xFFFD, "coils 14 to 17 set to 1, 0, 1, 0");
}

//------------------------------------------------------------------------------------------ Errors

// A frame with a broken CRC, for another address, or too short for any function
static void frames_left_unanswered(void)
{
  registers(0, 0);
  static const uint8_t write[] = { SLAVE, 0x06, 0x00, 0x00, 0x00, 0x07 };
  uint8_t broken[sizeof(write) + 2];
  memcpy(broken, write, sizeof(write));
  CRC_Append(&crc16_modbus, broken, sizeof(write));
  broken[sizeof(broken) - 1] ^= 0x01;
  for(uint8_t i = 0; i < sizeof(broken); i++) BUFF_Push(&rx, broken[i]);
  BUFF_Break(&rx);
  TEST_Equal(MODBUS_Loop(&slave), MODBUS_Status_InvalidCRC, "broken CRC");
  static const uint8_t other[] = { SLAVE + 1, 0x06, 0x00, 0x00, 0x00, 0x07 };
  TEST_Equal(request(other, sizeof(other)), MODBUS_Status_Ignored, "another address");
  static const uint8_t cut[] = { SLAVE, 0x06, 0x00 };
  TEST_Equal(request(cut, sizeof(cut)), MODBUS_Status_TooShort, "three bytes and a CRC");
  TEST_Check(!regs[0] && !updated[0], "nothing written");
  TEST_Check(!UART_HostSent(&port, NULL), "nothing on the line");
}

// A function the slave lacks is answered with exception `01`, illegal function
static void unknown_function_gets_an_exception(void)
{
  static const uint8_t identification[] = { SLAVE, 0x2B, 0x0E, 0x01, 0x00 };
  TEST_Check(request(identification, sizeof(identification)) == MODBUS_Status_Handled);
  static const uint8_t exception[] = { SLAVE, 0x2B | 0x80, 0x01 };
  answered(exception, sizeof(exception), "illegal function");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  UART_Init(&port);
  TEST(reads_registers);
  TEST(writes_registers);
  TEST(write_mask_protects_a_register);
  TEST(reads_coils_across_registers);
  TEST(coil_read_pads_with_zeros);
  TEST(writes_coils_across_registers);
  TEST(frames_left_unanswered);
  TEST(unknown_function_gets_an_exception);
}

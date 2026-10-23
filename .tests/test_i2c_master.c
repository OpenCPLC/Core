// .tests/test_i2c_master.c

// I2C master over a modelled bus: a transfer ends inside its call, answered by the device on it,
// the bus busy meanwhile and its bytes as the chip puts them on the wire:
// the register address ahead of the data of a register write,
// and before the read of a register, under a repeated start.

#include <string.h>

#include "test.h"
#include "i2c_master.h"

// What the device saw of the last transfer
static struct {
  uint8_t addr;     // address the master called
  uint8_t tx[8];    // bytes written, the first eight
  uint16_t tx_len;  // number of bytes written
  uint16_t rx_len;  // number of bytes asked for
  bool busy;        // the bus busy while the device answered
  uint32_t calls;   // transfers that reached the device
  bool refuse;      // a NACK for every transfer
} seen;

static I2C_Master_t bus;

// Device answering every address, the bytes it gives counting up from `0xA0`
static bool device(void *ctx, uint8_t addr, const uint8_t *tx_data, uint16_t tx_len,
  uint8_t *rx_data, uint16_t rx_len)
{
  unused(ctx);
  seen.addr = addr;
  seen.tx_len = tx_len;
  seen.rx_len = rx_len;
  seen.busy = I2C_Master_IsBusy(&bus);
  seen.calls++;
  for(uint16_t i = 0; i < tx_len && i < sizeof(seen.tx); i++) seen.tx[i] = tx_data[i];
  for(uint16_t i = 0; i < rx_len; i++) rx_data[i] = (uint8_t)(0xA0 + i);
  return !seen.refuse;
}

static void power_up(void)
{
  memset(&seen, 0, sizeof(seen));
  I2C_Master_Init(&bus);
  I2C_HostAttach(&bus, device, NULL);
}

//------------------------------------------------------------------------------------------- Cases

// A write brings its bytes to the address, a read takes the bytes of the device
static void write_and_read_frames(void)
{
  power_up();
  uint8_t data[3] = { 1, 2, 3 }, read[4];
  TEST_Check(I2C_Master_Write(&bus, 0x44, data, 3) == FREE, "write");
  TEST_Check(seen.addr == 0x44 && seen.tx_len == 3 && !seen.rx_len, "write frame");
  TEST_Bytes(seen.tx, data, 3, "bytes written");
  TEST_Check(I2C_Master_Read(&bus, 0x45, read, 4) == FREE, "read");
  TEST_Check(seen.addr == 0x45 && !seen.tx_len && seen.rx_len == 4, "read frame");
  uint8_t back[4] = { 0xA0, 0xA1, 0xA2, 0xA3 };
  TEST_Bytes(read, back, 4, "bytes read");
}

// A register write sends the register ahead of the data, a register read sends it before the read
static void register_frames_carry_the_register(void)
{
  power_up();
  uint8_t data[2] = { 7, 8 }, read[2];
  TEST_Check(I2C_Master_WriteReg(&bus, 0x50, 0x1F, data, 2) == FREE, "register write");
  uint8_t frame[3] = { 0x1F, 7, 8 };
  TEST_Check(seen.tx_len == 3 && !seen.rx_len, "register, then the data");
  TEST_Bytes(seen.tx, frame, 3, "register write frame");
  TEST_Check(I2C_Master_ReadReg(&bus, 0x50, 0x2E, read, 2) == FREE, "register read");
  TEST_Check(seen.tx_len == 1 && seen.tx[0] == 0x2E && seen.rx_len == 2,
    "register, then the read");
  uint8_t cmd[2] = { 0x10, 0x20 };
  TEST_Check(I2C_Master_WriteRead(&bus, 0x51, cmd, 2, read, 2) == FREE, "write-read");
  TEST_Check(seen.tx_len == 2 && seen.tx[1] == 0x20 && seen.rx_len == 2, "both phases");
}

// Nothing to send, nowhere to read into or past the limit of one transfer: refused before the bus
static void transfers_out_of_bounds_refused(void)
{
  power_up();
  static uint8_t data[I2C_TRANSFER_MAX + 1];
  TEST_Check(I2C_Master_Write(&bus, 0x44, data, 0) == ERR, "write of nothing");
  TEST_Check(I2C_Master_Write(&bus, 0x44, data, I2C_TRANSFER_MAX + 1) == ERR,
    "write past the limit");
  TEST_Check(I2C_Master_Read(&bus, 0x44, NULL, 1) == ERR, "read into nowhere");
  TEST_Check(I2C_Master_Read(&bus, 0x44, data, 0) == ERR, "read of nothing");
  TEST_Check(I2C_Master_WriteReg(&bus, 0x44, 0, data, I2C_TRANSFER_MAX) == ERR,
    "register write past the limit");
  TEST_Check(I2C_Master_ReadReg(&bus, 0x44, 0, data, I2C_TRANSFER_MAX + 1) == ERR,
    "register read past the limit");
  TEST_Check(I2C_Master_WriteRead(&bus, 0x44, data, 1, data, 0) == ERR, "write-read of no read");
  TEST_Equal(seen.calls, 0, "nothing on the bus");
  TEST_Check(I2C_Master_Write(&bus, 0x44, data, I2C_TRANSFER_MAX) == FREE, "write at the limit");
  TEST_Check(I2C_Master_WriteReg(&bus, 0x44, 0, data, I2C_TRANSFER_MAX - 1) == FREE,
    "register write at the limit");
}

// A NACK from the device or from an empty bus stays until the next transfer
static void nack_until_the_next_transfer(void)
{
  power_up();
  uint8_t data = 1;
  seen.refuse = true;
  TEST_Check(I2C_Master_Write(&bus, 0x44, &data, 1) == FREE && I2C_Master_Nack(&bus), "refused");
  seen.refuse = false;
  TEST_Check(I2C_Master_Write(&bus, 0x44, &data, 1) == FREE && !I2C_Master_Nack(&bus), "taken");
  I2C_HostAttach(&bus, NULL, NULL);
  TEST_Check(I2C_Master_Read(&bus, 0x44, &data, 1) == FREE && I2C_Master_Nack(&bus), "empty bus");
}

// The bus busy while the device answers and free on return; a disabled one refuses until init
static void busy_during_a_transfer_refused_when_disabled(void)
{
  power_up();
  uint8_t data = 1;
  TEST_Check(I2C_Master_Write(&bus, 0x44, &data, 1) == FREE, "write");
  TEST_Check(seen.busy, "busy while the device answers");
  TEST_Check(I2C_Master_IsFree(&bus) && !I2C_Master_IsBusy(&bus), "free on return");
  I2C_Master_Disable(&bus);
  TEST_Check(I2C_Master_Write(&bus, 0x44, &data, 1) == BUSY, "disabled");
  TEST_Equal(seen.calls, 1, "nothing on the bus");
  I2C_Master_Init(&bus);
  TEST_Check(I2C_Master_Write(&bus, 0x44, &data, 1) == FREE, "after init");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(write_and_read_frames);
  TEST(register_frames_carry_the_register);
  TEST(transfers_out_of_bounds_refused);
  TEST(nack_until_the_next_transfer);
  TEST(busy_during_a_transfer_refused_when_disabled);
}

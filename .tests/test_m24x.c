// .tests/test_m24x.c

// Serial EEPROM of the M24 family on a modelled bus, as its datasheets give it:
// the address in one byte up to 2kB and in two past it,
// the bits above them in the device address, a block each,
// data taken only with `~WC` low, a run wrapped around inside its page,
// and the address refused while a page is programmed, `cycle_ms` after its stop.
// M24C02, M24C16, M24C64 and M24M02 stand for the family:
// one block, eight blocks, two address bytes, and two address bytes with blocks and long pages.

#include <string.h>

#include "test.h"
#include "gpio.h"
#include "i2c_master.h"
#include "task.h"
#include "dvr/mem/m24x.c"

// Part of the family: its descriptor by the preset, its datasheet values for the model
typedef struct {
  const char *name;
  M24X_t preset;          // as a board gives it
  uint32_t size;          // [B]
  uint16_t page_size;     // [B]
  uint8_t address_bytes;  // address bytes after the device address
  uint32_t run_from;      // first byte of a run over page ends, and blocks where there are any
  uint16_t run_len;       // bytes of the run
  uint32_t run_parts;     // parts it goes in, pages and bursts of 64B, worked out apart
} part_t;

static const part_t PARTS[] = {
  { "M24C02", { .M24X_C02 }, 256,    16,  1, 0x002B, 60,  5  },
  { "M24C16", { .M24X_C16 }, 2048,   16,  1, 0x00F5, 300, 20 },
  { "M24C64", { .M24X_C64 }, 8192,   32,  2, 0x0FF0, 100, 4  },
  { "M24M02", { .M24X_M02 }, 262144, 256, 2, 0xFFC0, 300, 5  },
};

#define C16 (&PARTS[1]) // part of the cases run on one part only

// Memory on the bus
static struct {
  uint8_t cell[262144];   // room for the largest part
  uint32_t size;          // [B]
  uint16_t page_size;     // [B]
  uint8_t address_bytes;  // address bytes after the device address
  uint8_t base;           // device address of block 0
  uint32_t counter;       // address counter over the whole memory
  uint64_t busy_tick;     // end of the write cycle under way
  uint32_t cycle_ms;      // write cycle of a page
  uint32_t parts;         // parts of pages programmed
  GPIO_t *wc;             // `~WC` pin, `NULL` when tied low
} chip;

static I2C_Master_t bus;
static GPIO_t wc = { .port = GPIOB, .pin = 5, .reverse = true };
static M24X_t memory;

// `~WC` high at its pin
static bool wc_high(void) { return chip.wc && ((chip.wc->port->ODR >> chip.wc->pin) & 1u); }

// Device addresses of the part: a block each past what the address bytes reach
static uint8_t blocks(void) { return (uint8_t)maxv(chip.size >> (8 * chip.address_bytes), 1u); }

// Page write: the data wraps around inside the page of the counter, then the write cycle runs
static void program(const uint8_t *data, uint16_t len)
{
  uint32_t page = chip.counter - chip.counter % chip.page_size;
  for(uint16_t i = 0; i < len; i++) {
    chip.cell[page + (chip.counter + i) % chip.page_size] = data[i];
  }
  chip.busy_tick = tick_now() + chip.cycle_ms;
  chip.parts++;
}

// The device address picks the block, the address bytes after it the cell in the block.
// Then data goes in from that cell, or reads come out from it on.
static bool m24x_answer(void *ctx, uint8_t addr, const uint8_t *tx_data, uint16_t tx_len,
  uint8_t *rx_data, uint16_t rx_len)
{
  unused(ctx);
  if(addr < chip.base || addr >= chip.base + blocks()) return false;
  if(tick_now() < chip.busy_tick) return false; // programming a page
  uint8_t bytes = chip.address_bytes;
  if(tx_len && tx_len < bytes) return false; // address cut short
  if(tx_len) {
    uint32_t cell = bytes == 2 ? (uint32_t)tx_data[0] << 8 | tx_data[1] : tx_data[0];
    chip.counter = (uint32_t)(addr - chip.base) << (8 * bytes) | cell;
  }
  if(tx_len > bytes) {
    if(wc_high()) return false; // data refused
    program(&tx_data[bytes], tx_len - bytes);
  }
  for(uint16_t i = 0; i < rx_len; i++) {
    rx_data[i] = chip.cell[chip.counter];
    chip.counter = (chip.counter + 1) % chip.size;
  }
  return true;
}

// Memory as from the factory, every cell erased, a page programmed in `cycle_ms`.
// The pin of `~WC` starts low, which is how the memory reads a floating input.
static void power_up(const part_t *part, uint32_t cycle_ms)
{
  memset(&chip, 0, sizeof(chip));
  memset(chip.cell, 0xFF, sizeof(chip.cell));
  chip.size = part->size;
  chip.page_size = part->page_size;
  chip.address_bytes = part->address_bytes;
  chip.base = M24X_ADDRESS;
  chip.cycle_ms = cycle_ms;
  chip.wc = &wc;
  wc.mode = GPIO_Mode_Input;
  wc.port->ODR &= ~(1u << wc.pin);
  I2C_Master_Init(&bus);
  I2C_HostAttach(&bus, m24x_answer, NULL);
  memory = part->preset;
  memory.i2c = &bus;
  memory.wc = &wc;
  M24X_Init(&memory);
}

//--------------------------------------------------------------------------------------- Each part

// A run over page ends and blocks lands in the cells of its addresses, part by part,
// and comes back whole.
static void run_lands_where_its_addresses_say(void)
{
  static uint8_t data[300], read[300];
  for(uint8_t p = 0; p < array_len(PARTS); p++) {
    const part_t *part = &PARTS[p];
    power_up(part, M24X_WRITE_ms);
    for(uint16_t i = 0; i < part->run_len; i++) data[i] = (uint8_t)(i * 7 + p + 1);
    TEST_Check(M24X_Write(&memory, part->run_from, data, part->run_len) == OK, "%s write",
      part->name);
    TEST_Check(!memcmp(&chip.cell[part->run_from], data, part->run_len), "%s cells", part->name);
    TEST_Check(chip.parts == part->run_parts, "%s parts %u, %u expected",
      part->name, chip.parts, part->run_parts);
    TEST_Check(M24X_Read(&memory, part->run_from, read, part->run_len) == OK, "%s read",
      part->name);
    TEST_Check(!memcmp(read, data, part->run_len), "%s bytes back", part->name);
  }
}

// A read past the limit of one transfer, the end of the memory, and its last byte written
static void end_of_the_memory(void)
{
  static uint8_t read[600];
  for(uint8_t p = 0; p < array_len(PARTS); p++) {
    const part_t *part = &PARTS[p];
    power_up(part, M24X_WRITE_ms);
    // Cells at one offset differ from block to block
    for(uint32_t i = 0; i < part->size; i++) chip.cell[i] = (uint8_t)(i ^ (i >> 8) ^ (i >> 16));
    uint16_t len = (uint16_t)minv(part->size, sizeof(read));
    uint32_t from = part->size - len;
    TEST_Check(M24X_Read(&memory, from, read, len) == OK, "%s read", part->name);
    TEST_Check(!memcmp(read, &chip.cell[from], len), "%s cells to the end", part->name);
    uint8_t last = 0xA5;
    TEST_Check(M24X_Write(&memory, part->size - 1, &last, 1) == OK, "%s last byte", part->name);
    TEST_Check(chip.cell[part->size - 1] == 0xA5, "%s last cell", part->name);
  }
}

// A run past the end of the memory is refused before anything goes out
static void run_past_the_end_refused(void)
{
  for(uint8_t p = 0; p < array_len(PARTS); p++) {
    const part_t *part = &PARTS[p];
    power_up(part, M24X_WRITE_ms);
    uint8_t data[2] = { 1, 2 };
    TEST_Check(M24X_Write(&memory, part->size - 1, data, 2) == ERR, "%s write past the end",
      part->name);
    TEST_Check(M24X_Read(&memory, part->size - 1, data, 2) == ERR, "%s read past the end",
      part->name);
    TEST_Check(M24X_Read(&memory, part->size, data, 1) == ERR, "%s read from the end",
      part->name);
    TEST_Check(chip.parts == 0, "%s nothing written", part->name);
    TEST_Check(M24X_Write(&memory, part->size - 2, data, 2) == OK, "%s last two bytes",
      part->name);
    TEST_Check(!memcmp(&chip.cell[part->size - 2], data, 2), "%s last two cells", part->name);
  }
}

//------------------------------------------------------------------------------------------- Cases

// `~WC` low through a write and high after it; a memory with the line tied low writes too
static void write_only_with_wc_low(void)
{
  power_up(C16, M24X_WRITE_ms);
  TEST_Check(wc.mode == GPIO_Mode_Output && wc_high(), "output, high after init");
  uint8_t data[40];
  memset(data, 0x5A, sizeof(data));
  TEST_Check(M24X_Write(&memory, 100, data, sizeof(data)) == OK, "write");
  TEST_Check(wc_high(), "high after the write");
  TEST_Bytes(&chip.cell[100], data, sizeof(data), "cells written");
  M24X_t tied = { .i2c = &bus, .M24X_C16 };
  chip.wc = NULL;
  TEST_Check(M24X_Write(&tied, 200, data, sizeof(data)) == OK, "line tied low");
  TEST_Bytes(&chip.cell[200], data, sizeof(data), "cells written, line tied low");
}

// `~WC` high at the pin of a memory the driver takes for tied low: data refused, write failed
static void refused_data_fails_the_write(void)
{
  power_up(C16, M24X_WRITE_ms);
  M24X_t unwired = { .i2c = &bus, .M24X_C16 };
  uint8_t byte = 0x11;
  TEST_Check(M24X_Write(&unwired, 30, &byte, 1) == ERR, "write");
  TEST_Equal(chip.cell[30], 0xFF, "cell still erased");
}

// Each page waits for the cycle of the one before:
// a cycle within twice the longest of the family goes, one past it fails.
static void page_waits_for_its_write_cycle(void)
{
  power_up(C16, 5);
  uint8_t data[64];
  memset(data, 0x3C, sizeof(data));
  uint64_t start = tick_now();
  TEST_Check(M24X_Write(&memory, 0, data, sizeof(data)) == OK, "four pages");
  TEST_Check(tick_now() - start >= 4 * 5, "four cycles");
  power_up(C16, 15);
  TEST_Check(M24X_Write(&memory, 0, data, sizeof(data)) == OK, "cycles of 15ms");
  power_up(C16, 2 * M24X_WRITE_ms + 5);
  TEST_Check(M24X_Write(&memory, 0, data, sizeof(data)) == ERR, "cycle that does not end");
  TEST_Equal(chip.parts, 1, "no page after it");
  TEST_Check(wc_high(), "high after the failed write");
}

// No memory on the bus, or a bus that stays taken: reads and writes fail, `~WC` high after
static void silent_memory_fails(void)
{
  power_up(C16, M24X_WRITE_ms);
  I2C_HostAttach(&bus, NULL, NULL);
  uint8_t byte = 1;
  TEST_Check(M24X_Read(&memory, 0, &byte, 1) == ERR, "read, empty bus");
  TEST_Check(M24X_Write(&memory, 0, &byte, 1) == ERR, "write, empty bus");
  TEST_Check(wc_high(), "high after the failed write");
  power_up(C16, M24X_WRITE_ms);
  I2C_Master_Disable(&bus);
  TEST_Check(M24X_Read(&memory, 0, &byte, 1) == ERR, "read, bus taken");
  TEST_Check(M24X_Write(&memory, 0, &byte, 1) == ERR, "write, bus taken");
  TEST_Equal(chip.parts, 0, "nothing written");
}

// A bus another device holds for 10ms: a write and a read wait their turn and go through
static void transfer_waits_for_a_held_bus(void)
{
  power_up(C16, M24X_WRITE_ms);
  uint8_t byte = 0x42;
  I2C_Master_Disable(&bus);
  TASK_Add(TASK_&I2C_Master_Init, &bus, 10);
  TEST_Check(M24X_Write(&memory, 7, &byte, 1) == OK, "write");
  TEST_Equal(chip.cell[7], 0x42, "cell written");
  byte = 0;
  I2C_Master_Disable(&bus);
  TASK_Add(TASK_&I2C_Master_Init, &bus, 10);
  TEST_Check(M24X_Read(&memory, 7, &byte, 1) == OK, "read");
  TEST_Equal(byte, 0x42, "byte back");
}

// Another device takes the bus while a page programs: the poll waits its turn, the write goes
static void poll_waits_for_a_bus_taken_meanwhile(void)
{
  power_up(C16, M24X_WRITE_ms);
  uint8_t byte = 0x24;
  TASK_Add(TASK_&I2C_Master_Disable, &bus, 2);
  TASK_Add(TASK_&I2C_Master_Init, &bus, 9);
  TEST_Check(M24X_Write(&memory, 9, &byte, 1) == OK, "write");
  TEST_Equal(chip.cell[9], 0x24, "cell written");
}

// A memory with chip enable pins high answers under their address, and not under the default
static void memory_under_its_chip_enable_pins(void)
{
  power_up(&PARTS[0], M24X_WRITE_ms);
  chip.base = 0x53;
  uint8_t data[20], read[20];
  memset(data, 0x6B, sizeof(data));
  TEST_Check(M24X_Write(&memory, 0x40, data, sizeof(data)) == ERR, "default address");
  memory.address = 0x53;
  TEST_Check(M24X_Write(&memory, 0x40, data, sizeof(data)) == OK, "write");
  TEST_Bytes(&chip.cell[0x40], data, sizeof(data), "cells from 0x40 on");
  TEST_Check(M24X_Read(&memory, 0x40, read, sizeof(read)) == OK, "read");
  TEST_Bytes(read, data, sizeof(data), "bytes back");
}

// A memory without its size or its page refuses every transfer
static void part_without_size_refused(void)
{
  power_up(C16, M24X_WRITE_ms);
  uint8_t byte = 1;
  M24X_t no_size = { .i2c = &bus, .page_size = 16 };
  M24X_t no_page = { .i2c = &bus, .size = 2048 };
  TEST_Check(M24X_Read(&no_size, 0, &byte, 1) == ERR, "read without size");
  TEST_Check(M24X_Write(&no_page, 0, &byte, 1) == ERR, "write without page");
  TEST_Equal(chip.parts, 0, "nothing written");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(run_lands_where_its_addresses_say);
  TEST(end_of_the_memory);
  TEST(run_past_the_end_refused);
  TEST(write_only_with_wc_low);
  TEST(refused_data_fails_the_write);
  TEST(page_waits_for_its_write_cycle);
  TEST(silent_memory_fails);
  TEST(transfer_waits_for_a_held_bus);
  TEST(poll_waits_for_a_bus_taken_meanwhile);
  TEST(memory_under_its_chip_enable_pins);
  TEST(part_without_size_refused);
}

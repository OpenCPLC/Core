// .tests/test_retain.c

// Retained state of `DIN_t` and `DOUT_t` in EEPROM, under keys from the channel name:
// djb2 of the name and one round more for the field, the same in every build.
// The keys of every store of the Uno board stay apart and clear of the reserved ones,
// a retained value comes back after a power-up, and a channel without a name keeps nothing.

// A G081: 128kB of flash in pages of 2kB
#define FLASH_PAGE_SIZE 2048
#define FLASH_PAGES 64

#include "test.h"
#include "flash.c"
#include "eeprom.c"
#include "plc/io/din.c"
#include "plc/io/dout.c"

#define PAGE 56 // first of the four pages of the store

// Channels of each store of the Uno board, as `brd/uno` names them
static const char *const RELAY_DOUT[] = { "RO1", "RO2", "RO3", "RO4" };
static const char *const IO_DOUT[] = { "TO1", "TO2", "TO3", "TO4", "XO1", "XO2" };
static const char *const IO_DIN[] = { "DI1", "DI2", "DI3", "DI4" };

static EEPROM_t eeprom;
static uint32_t keys[64];
static uint8_t keys_count;

// Power-up over whatever the flash holds: a new descriptor, as after a reset
static void power_up(void)
{
  eeprom = (EEPROM_t){ .page_start = PAGE, .page_count = 4 };
  TEST_Check(EEPROM_Init(&eeprom) == OK, "init");
}

static void keys_of_douts(const char *const *names, uint8_t count)
{
  for(uint8_t i = 0; i < count; i++) {
    DOUT_t dout = { .name = names[i] };
    for(DOUT_Field_t field = DOUT_Field_Save; field <= DOUT_Field_Cycles; field++) {
      keys[keys_count++] = dout_key(&dout, field);
    }
  }
}

static void keys_of_dins(const char *const *names, uint8_t count)
{
  for(uint8_t i = 0; i < count; i++) {
    DIN_t din = { .name = names[i] };
    for(DIN_Time_t time = DIN_Time_Ton; time <= DIN_Time_Hold; time++) {
      keys[keys_count++] = din_key(&din, time);
    }
  }
}

// Relay `name` on pin `pin` of `GPIOA`, its state retained in the store
static DOUT_t relay(const char *name, uint8_t pin)
{
  return (DOUT_t){ .name = name, .relay = true, .gpio = { .port = GPIOA, .pin = pin },
    .eeprom = &eeprom };
}

// Input `name` on `PA6`, its times retained in the store
static DIN_t input(const char *name)
{
  return (DIN_t){ .name = name, .gpio = { .port = GPIOA, .pin = 6 }, .eeprom = &eeprom };
}

// Every key gathered for one store apart from the others and below the reserved ones
static void keys_apart(const char *what)
{
  for(uint8_t i = 0; i < keys_count; i++) {
    TEST_Check(keys[i] < EEPROM_DAMAGED_KEY, "%s key %u reserved", what, i);
    for(uint8_t j = i + 1; j < keys_count; j++) {
      TEST_Check(keys[i] != keys[j], "%s keys %u and %u", what, i, j);
    }
  }
}

//-------------------------------------------------------------------------------------------- Keys

// Keys worked out apart from this code: a stored value finds its key after every update
static void keys_stay_the_same(void)
{
  DOUT_t ro1 = { .name = "RO1" };
  DIN_t di1 = { .name = "DI1" };
  TEST_Equal(dout_key(&ro1, DOUT_Field_Cycles), 0x7C8B5CF9u, "cycles of RO1");
  TEST_Equal(din_key(&di1, DIN_Time_Ton), 0x7C839623u, "on time of DI1");
}

static void uno_keys_stay_apart(void)
{
  keys_count = 0;
  keys_of_douts(RELAY_DOUT, array_len(RELAY_DOUT));
  keys_apart("relay store");
  keys_count = 0;
  keys_of_douts(IO_DOUT, array_len(IO_DOUT));
  keys_of_dins(IO_DIN, array_len(IO_DIN));
  keys_apart("io store");
}

//------------------------------------------------------------------------------------------ Retain

// Value of a relay, the choice to retain it and its cycles come back after a power-up
static void relay_comes_back(void)
{
  FLASH_HostBlank();
  power_up();
  DOUT_t ro = relay("RO1", 1);
  DOUT_Init(&ro);
  DOUT_SaveValue(&ro, true);
  DOUT_Set(&ro);
  relay_cycles_inc(&ro);
  relay_cycles_inc(&ro);
  power_up();
  DOUT_t again = relay("RO1", 1);
  DOUT_Init(&again);
  TEST_Check(again.save, "retain choice");
  TEST_Check(again.value, "value");
  TEST_Equal(again.cycles, 2, "cycles");
  DOUT_t other = relay("RO2", 2);
  DOUT_Init(&other);
  TEST_Check(!other.save && !other.value && !other.cycles, "another channel, nothing");
}

// Retaining turned off: the value stored while it was on stays behind after a power-up
static void value_left_behind_once_retaining_is_off(void)
{
  FLASH_HostBlank();
  power_up();
  DOUT_t ro = relay("RO1", 1);
  DOUT_Init(&ro);
  DOUT_SaveValue(&ro, true);
  DOUT_Set(&ro);
  DOUT_SaveValue(&ro, false);
  power_up();
  DOUT_t again = relay("RO1", 1);
  DOUT_Init(&again);
  TEST_Check(!again.save, "retaining off");
  TEST_Check(!again.value, "value left behind");
}

// A time set through the API comes back after a power-up, one the board sets wins
static void input_time_comes_back(void)
{
  FLASH_HostBlank();
  power_up();
  DIN_t di = input("DI1");
  DIN_Init(&di);
  TEST_Equal(DIN_Time(&di, DIN_Time_Ton, 123), 123, "set");
  power_up();
  DIN_t again = input("DI1");
  DIN_Init(&again);
  TEST_Equal(again.gpif.ton_ms, 123, "stored time");
  DIN_t board = input("DI1");
  board.gpif.ton_ms = 50;
  DIN_Init(&board);
  TEST_Equal(board.gpif.ton_ms, 50, "time of the board");
}

// A channel without a name keeps nothing: the store takes no write, RAM holds the change
static void nameless_channel_keeps_nothing(void)
{
  FLASH_HostBlank();
  power_up();
  DOUT_t out = relay(NULL, 1);
  out.save = true;
  DOUT_Init(&out);
  DOUT_Set(&out);
  relay_cycles_inc(&out);
  DOUT_SaveValue(&out, false);
  DIN_t in = input(NULL);
  DIN_Init(&in);
  TEST_Equal(DIN_Time(&in, DIN_Time_Ton, 77), 77, "time in RAM");
  TEST_Check(eeprom._cursor == eeprom._addr_start[eeprom._active], "nothing written");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(keys_stay_the_same);
  TEST(uno_keys_stay_apart);
  TEST(relay_comes_back);
  TEST(value_left_behind_once_retaining_is_off);
  TEST(input_time_comes_back);
  TEST(nameless_channel_keeps_nothing);
}

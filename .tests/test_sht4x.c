// .tests/test_sht4x.c

// SHT40 on a modelled bus, as its datasheet gives it: a measurement command, its conversion,
// up to 8.3ms at the highest repeatability, then six bytes, the read refused until then.
// Temperature and humidity are words, each with its CRC-8, worked out apart from this code.
// A result stays readable until the next command.

#include <string.h>

#include "test.h"
#include "i2c_master.h"
#include "task.h"
#include "dvr/temp/sht4x.c"

// Two samples the sensor gives in turn: 25.0°C and 56.5%RH, then 16.25°C and 25.25%RH
static const uint8_t SAMPLES[2][6] = {
  { 0x66, 0x66, 0x93, 0x80, 0x00, 0xA2 },
  { 0x59, 0x99, 0x14, 0x40, 0x00, 0x08 },
};

// Sensor on the bus
static struct {
  uint8_t response[2][6];  // samples given in turn
  uint8_t sample;          // sample the next conversion gives
  uint8_t ready;           // sample a read gives
  uint32_t commands;       // measurement commands taken
  uint64_t ready_tick;     // end of the conversion, `0` before the first command
  bool refuse_command;     // a NACK for the command byte
  bool refuse_read;        // a NACK for every read
} chip;

static I2C_Master_t bus;
static SHT4X_t sensor;

static bool sht40_answer(void *ctx, uint8_t addr, const uint8_t *tx_data, uint16_t tx_len,
  uint8_t *rx_data, uint16_t rx_len)
{
  unused(ctx);
  if(addr != SHT4X_ADDRESS) return false;
  if(tx_len == 1 && tx_data[0] == SHT4X_Mode_High) {
    if(chip.refuse_command) return false;
    chip.commands++;
    chip.ready = chip.sample;
    chip.sample = (chip.sample + 1) % 2;
    chip.ready_tick = tick_now() + 9; // 8.3ms, whole ticks
    return true;
  }
  if(rx_len == sizeof(chip.response[0])) {
    if(chip.refuse_read || !chip.ready_tick || tick_now() < chip.ready_tick) return false;
    memcpy(rx_data, chip.response[chip.ready], rx_len);
    return true;
  }
  return false;
}

// Sensor answering its address with the two samples, a driver yet to measure
static void power_up(void)
{
  memset(&chip, 0, sizeof(chip));
  memcpy(chip.response, SAMPLES, sizeof(SAMPLES));
  I2C_Master_Init(&bus);
  I2C_HostAttach(&bus, sht40_answer, NULL);
  sensor = (SHT4X_t){ .i2c = &bus, .mode = SHT4X_Mode_High,
    .expiry_ms = 1000, .interval_ms = 500, .humidity = NaN, .temperature = NaN };
}

//------------------------------------------------------------------------------------------- Cases

// One measurement: the command, its conversion, the reading
static void reading_comes_through(void)
{
  power_up();
  TEST_Check(isNaN(SHT4X_Temperature_C(&sensor)), "no reading before the first");
  TEST_Check(SHT4X_Loop(&sensor) == OK, "measurement");
  TEST_Near(SHT4X_Temperature_C(&sensor), 25.0f, 0.01f, "temperature");
  TEST_Near(SHT4X_Humidity_RH(&sensor), 56.5f, 0.01f, "humidity");
}

// Within the interval a loop measures nothing, past it the next measurement goes
static void one_measurement_an_interval(void)
{
  power_up();
  TEST_Check(SHT4X_Loop(&sensor) == OK, "first");
  TEST_Check(SHT4X_Loop(&sensor) == OK, "within the interval");
  TEST_Equal(chip.commands, 1, "one command");
  TEST_Run(500);
  TEST_Check(SHT4X_Loop(&sensor) == OK, "past the interval");
  TEST_Equal(chip.commands, 2, "next command");
  TEST_Near(SHT4X_Temperature_C(&sensor), 16.25f, 0.01f, "the next sample");
}

// Oversampling averages the samples, temperature and humidity alike
static void samples_averaged(void)
{
  power_up();
  sensor.oversampling = 2;
  TEST_Check(SHT4X_Loop(&sensor) == OK, "measurement");
  TEST_Equal(chip.commands, 2, "a command a sample");
  TEST_Near(SHT4X_Temperature_C(&sensor), 20.625f, 0.01f, "mean temperature");
  TEST_Near(SHT4X_Humidity_RH(&sensor), 40.875f, 0.01f, "mean humidity");
}

// A word whose CRC-8 does not match fails the measurement, the reading stays as it was
static void wrong_crc_fails(void)
{
  power_up();
  chip.response[0][5] ^= 0x01;
  TEST_Check(SHT4X_Loop(&sensor) == ERR, "humidity CRC");
  TEST_Check(isNaN(SHT4X_Humidity_RH(&sensor)), "no reading");
  power_up();
  chip.response[0][2] ^= 0x01;
  TEST_Check(SHT4X_Loop(&sensor) == ERR, "temperature CRC");
  TEST_Check(isNaN(SHT4X_Temperature_C(&sensor)), "no reading");
}

// A NACK for the command or for the read fails the measurement, the last result readable or not
static void refused_transfer_fails(void)
{
  power_up();
  TEST_Check(SHT4X_Loop(&sensor) == OK, "measurement");
  TEST_Run(500);
  chip.refuse_command = true;
  TEST_Check(SHT4X_Loop(&sensor) == ERR, "command refused");
  TEST_Check(I2C_Master_Nack(&bus), "NACK seen");
  chip.refuse_command = false;
  chip.refuse_read = true;
  TEST_Check(SHT4X_Loop(&sensor) == ERR, "read refused");
  TEST_Near(SHT4X_Temperature_C(&sensor), 25.0f, 0.01f, "reading of the last measurement");
}

// No measurement for `expiry_ms` after the last one: the readings turn to `NaN`
static void readings_expire(void)
{
  power_up();
  TEST_Check(SHT4X_Loop(&sensor) == OK, "measurement");
  I2C_HostAttach(&bus, NULL, NULL);
  TEST_Run(600);
  TEST_Check(SHT4X_Loop(&sensor) == ERR, "sensor gone");
  TEST_Near(SHT4X_Temperature_C(&sensor), 25.0f, 0.01f, "before the expiry");
  TEST_Run(500);
  TEST_Check(SHT4X_Loop(&sensor) == ERR, "sensor still gone");
  TEST_Check(isNaN(SHT4X_Temperature_C(&sensor)) && isNaN(SHT4X_Humidity_RH(&sensor)),
    "expired");
}

// Another device takes the bus during the conversion and frees it 5ms after: the read waits
static void read_waits_for_a_bus_taken_meanwhile(void)
{
  power_up();
  TASK_Add(TASK_&I2C_Master_Disable, &bus, 3);
  TASK_Add(TASK_&I2C_Master_Init, &bus, 15);
  TEST_Check(SHT4X_Loop(&sensor) == OK, "measurement");
  TEST_Near(SHT4X_Temperature_C(&sensor), 25.0f, 0.01f, "temperature");
}

// A bus taken when the measurement is due: `BUSY`, nothing measured
static void bus_taken_at_start_is_busy(void)
{
  power_up();
  I2C_Master_Disable(&bus);
  TEST_Check(SHT4X_Loop(&sensor) == BUSY, "bus taken");
  TEST_Equal(chip.commands, 0, "no command");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(reading_comes_through);
  TEST(one_measurement_an_interval);
  TEST(samples_averaged);
  TEST(wrong_crc_fails);
  TEST(refused_transfer_fails);
  TEST(readings_expire);
  TEST(read_waits_for_a_bus_taken_meanwhile);
  TEST(bus_taken_at_start_is_busy);
}

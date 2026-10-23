// .tests/test_spi_master.c

// SPI master over a modelled bus: a transfer ends inside its call, answered by the device on it,
// with chip select driven as the chip drives it and `0xFF` from an empty bus.

#include <string.h>

#include "test.h"
#include "spi_master.h"
#include "vrts.h"

// What the device saw of the last transfer
static struct {
  uint8_t tx[8];    // bytes on MOSI, the first eight
  bool driven;      // MOSI carried bytes
  bool taken;       // the master kept the bytes on MISO
  bool selected;    // chip select set while the bytes went
  uint16_t len;     // number of bytes
  uint64_t tick;    // when the bytes went
  bool busy;        // the bus busy while the bytes went
  status_t nested;  // a transfer the device started while it answered
} seen;

static GPIO_t cs = { .port = GPIOA, .pin = 4, .reverse = true };
static SPI_Master_t spi = { .cs = &cs, .mosi = SPI1_MOSI_PA7 };

// Device answering every byte with its complement, byte by byte as the bus moves them;
// `ctx` is the bus it sits on.
static void complement(void *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len)
{
  seen.busy = SPI_Master_IsBusy(ctx);
  seen.driven = tx_data != NULL;
  seen.taken = rx_data != NULL;
  seen.selected = GPIO_In(&cs);
  seen.len = len;
  seen.tick = tick_now();
  for(uint16_t i = 0; i < len; i++) {
    uint8_t out = tx_data ? tx_data[i] : 0;
    if(i < sizeof(seen.tx)) seen.tx[i] = out;
    if(rx_data) rx_data[i] = (uint8_t)~out;
  }
}

// Device starting a transfer of its own while it answers one;
// a transfer let through answers without starting another.
static void starts_its_own(void *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len)
{
  unused(tx_data);
  unused(rx_data);
  unused(len);
  static bool inside;
  if(inside) return;
  inside = true;
  uint8_t byte = 0;
  seen.nested = SPI_Master_Write(ctx, &byte, 1);
  inside = false;
}

static void power_up(void)
{
  memset(&seen, 0, sizeof(seen));
  SPI_HostAttach(&spi, complement, &spi);
  SPI_Master_Init(&spi);
}

//------------------------------------------------------------------------------------------- Cases

// Bytes out under chip select, the answer back in the buffer, chip select released after
static void transfer_under_chip_select(void)
{
  power_up();
  TEST_Check(cs.mode == GPIO_Mode_Output && !GPIO_In(&cs), "output, released after init");
  uint8_t tx[3] = { 0x01, 0x80, 0xFF }, rx[3];
  TEST_Check(SPI_Master_Transfer(&spi, rx, tx, 3) == FREE, "transfer");
  TEST_Check(seen.selected && seen.busy, "selected and busy while the bytes go");
  TEST_Bytes(seen.tx, tx, 3, "bytes out");
  uint8_t back[3] = { 0xFE, 0x7F, 0x00 };
  TEST_Bytes(rx, back, 3, "bytes back");
  TEST_Check(!GPIO_In(&cs), "released after");
  TEST_Check(SPI_Master_IsFree(&spi) && !SPI_Master_IsBusy(&spi), "free on return");
}

// A read sends `cmd` with every byte, a write drops what comes back
static void read_repeats_its_byte_write_drops_the_answer(void)
{
  power_up();
  uint8_t rx[4];
  TEST_Check(SPI_Master_Read(&spi, 0x9A, rx, 4) == FREE, "read");
  uint8_t cmd[4] = { 0x9A, 0x9A, 0x9A, 0x9A }, back[4] = { 0x65, 0x65, 0x65, 0x65 };
  TEST_Bytes(seen.tx, cmd, 4, "`cmd` with every byte");
  TEST_Bytes(rx, back, 4, "bytes back");
  uint8_t tx[2] = { 1, 2 };
  TEST_Check(SPI_Master_Write(&spi, tx, 2) == FREE, "write");
  TEST_Check(seen.driven && !seen.taken && seen.len == 2, "answer dropped");
}

// Without MOSI a read drives no byte, chip select released once the bytes are in
static void read_without_mosi_drives_nothing(void)
{
  power_up();
  SPI_Master_t rx_only = { .cs = &cs };
  SPI_HostAttach(&rx_only, complement, &rx_only);
  SPI_Master_Init(&rx_only);
  uint8_t rx[2];
  GPIO_Set(&cs); // the caller selects: a receive-only read leaves chip select as it is
  TEST_Check(SPI_Master_Read(&rx_only, 0x9A, rx, 2) == FREE, "read");
  TEST_Check(seen.selected, "selected while the bytes go");
  TEST_Check(!seen.driven && seen.taken && seen.len == 2, "MOSI not driven");
  TEST_Check(!GPIO_In(&cs), "released after");
}

// `cs_delay` passes between chip select and the first byte
static void chip_select_waits_its_delay(void)
{
  power_up();
  spi.cs_delay = 3;
  uint64_t start = tick_now();
  uint8_t tx = 0x42;
  TEST_Check(SPI_Master_Write(&spi, &tx, 1) == FREE, "write");
  spi.cs_delay = 0;
  TEST_Check(seen.tick - start >= 3, "3ms before the byte");
}

// A transfer started while another is under way is refused
static void transfer_refused_while_busy(void)
{
  power_up();
  SPI_HostAttach(&spi, starts_its_own, &spi);
  uint8_t tx = 1;
  TEST_Check(SPI_Master_Write(&spi, &tx, 1) == FREE, "transfer");
  TEST_Check(seen.nested == BUSY, "transfer started meanwhile refused");
}

// An empty bus reads `0xFF`
static void empty_bus_reads_ones(void)
{
  power_up();
  SPI_HostAttach(&spi, NULL, NULL);
  uint8_t tx[2] = { 1, 2 }, rx[2] = { 0, 0 }, ones[2] = { 0xFF, 0xFF };
  TEST_Check(SPI_Master_Transfer(&spi, rx, tx, 2) == FREE, "transfer");
  TEST_Bytes(rx, ones, 2, "ones");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(transfer_under_chip_select);
  TEST(read_repeats_its_byte_write_drops_the_answer);
  TEST(read_without_mosi_drives_nothing);
  TEST(chip_select_waits_its_delay);
  TEST(transfer_refused_while_busy);
  TEST(empty_bus_reads_ones);
}

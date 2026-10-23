// .tests/test_uart.c

// UART of a test build: the bytes it sends go on the line as the chip puts them there,
// kept for `UART_HostSent` and handed to the device on the line,
// `prefix` ahead of every transfer, `dir` set while the bytes go and released after.

#include "test.h"
#include "uart.h"

static uint8_t rx_memory[64];
static BUFF_t rx = { .memory = rx_memory, .size = sizeof(rx_memory) };
static GPIO_t dir = { .port = GPIOB, .pin = 1 };

// What the device on the line saw
static struct {
  uint16_t len;  // bytes taken
  bool idle;     // a byte went with `dir` released
} line;

static void receiver(void *ctx, const uint8_t *data, uint16_t len)
{
  unused(ctx);
  unused(data);
  if(!GPIO_In(&dir)) line.idle = true;
  line.len += len;
}

//------------------------------------------------------------------------------------------- Cases

// Bytes on the line as sent, `prefix` ahead of every transfer
static void prefix_goes_ahead_of_every_transfer(void)
{
  UART_t port = { .UART_9600, .buff = &rx };
  TEST_Check(UART_Init(&port) == OK, "init");
  uint8_t sent[8];
  TEST_Check(UART_Send(&port, (const uint8_t *)"AB", 2) == OK, "send");
  TEST_Equal(UART_HostSent(&port, sent), 2, "no prefix");
  port.prefix = 0x55;
  TEST_Check(UART_Send(&port, (const uint8_t *)"AB", 2) == OK, "send");
  TEST_Check(UART_Send(&port, (const uint8_t *)"C", 1) == OK, "send");
  uint8_t bytes[] = { 0x55, 'A', 'B', 0x55, 'C' };
  TEST_Equal(UART_HostSent(&port, sent), sizeof(bytes), "bytes with their prefix");
  TEST_Bytes(sent, bytes, sizeof(bytes), "prefix ahead of every transfer");
}

// `dir` an output released from init on, set while the prefix and the bytes go, released after;
// nothing to send is refused.
static void dir_set_for_the_bytes(void)
{
  UART_t port = { .UART_9600, .buff = &rx, .dir = &dir, .prefix = 0x55 };
  dir.mode = GPIO_Mode_Input;
  GPIO_Set(&dir);
  TEST_Check(UART_Init(&port) == OK, "init");
  TEST_Check(dir.mode == GPIO_Mode_Output && !GPIO_In(&dir), "output, released after init");
  line.len = 0;
  line.idle = false;
  UART_HostAttach(&port, receiver, NULL);
  TEST_Check(UART_Send(&port, (const uint8_t *)"AB", 2) == OK, "send");
  TEST_Check(line.len == 3 && !line.idle, "set while the prefix and the bytes go");
  TEST_Check(!GPIO_In(&dir), "released after");
  TEST_Check(UART_Send(&port, (const uint8_t *)"AB", 0) == ERR, "nothing to send");
  UART_HostSent(&port, NULL);
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(prefix_goes_ahead_of_every_transfer);
  TEST(dir_set_for_the_bytes);
}

// hal/host/itf/uart.c

#include "uart.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <conio.h>
#else
  #include "posix.h"
  #include <termios.h>
  #include <fcntl.h>
  #include <sys/select.h>
#endif

#ifdef HOST_TEST
  #include "test.h"
#endif

USART_TypeDef HostUsart[8];

//----------------------------------------------------------------------------------- Console setup
#if defined(HOST_TEST)

// A test has no console: nothing comes in, and the harness takes what goes out
static void console_init(void) {}
static void console_deinit(void) {}
static int console_kbhit(void) { return 0; }
static int console_getch(void) { return -1; }

#elif defined(_WIN32) || defined(_WIN64)

static HANDLE stdin_handle;
static DWORD orig_mode;

static void console_init(void)
{
  stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(stdin_handle, &orig_mode);
  SetConsoleMode(stdin_handle, orig_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
}

static void console_deinit(void)
{
  SetConsoleMode(stdin_handle, orig_mode);
}

static int console_kbhit(void) { return _kbhit(); }
static int console_getch(void) { return _getch(); }

#else

static struct termios orig_termios;

static void console_init(void)
{
  struct termios raw;
  tcgetattr(STDIN_FILENO, &orig_termios);
  raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

static void console_deinit(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static int console_kbhit(void)
{
  struct timeval tv = { 0, 0 };
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int console_getch(void)
{
  unsigned char c;
  if(read(STDIN_FILENO, &c, 1) == 1) return c;
  return -1;
}

#endif
//------------------------------------------------------------------------------------ Console read

// Console input is drained on the reading side, not by a thread of its own.
// `BUFF_t` is written by one producer and read by one consumer; on the target those are
// the interrupt and the thread it feeds, both on one core. A second OS thread here would
// be neither, so the port keeps the single-producer contract by staying single-threaded.
static void console_drain(UART_t *uart)
{
  while(console_kbhit()) {
    int c = console_getch();
    if(c < 0) break;
    BUFF_Push(uart->buff, (uint8_t)c);
    if(c == '\r' || c == '\n') BUFF_Break(uart->buff);
  }
}

//--------------------------------------------------------------------------------------------- API

status_t UART_Init(UART_t *uart)
{
  if(!uart->buff) return ERR;
  if(uart->dir) {
    uart->dir->mode = GPIO_Mode_Output;
    GPIO_Init(uart->dir);
  }
  BUFF_Init(uart->buff);
  console_init();
  uart->_init = true;
  return OK;
}

void UART_DeInit(UART_t *uart)
{
  if(!uart->_init) return;
  console_deinit();
  uart->_init = false;
}

status_t UART_ReInit(UART_t *uart)
{
  UART_DeInit(uart);
  return UART_Init(uart);
}

void UART_SetTimeout(UART_t *uart, uint16_t timeout) { uart->timeout = timeout; }

//------------------------------------------------------------------------------------------ Status

bool UART_SendCompleted(UART_t *uart) { return !uart->_tx_busy; }
bool UART_SendActive(UART_t *uart) { return uart->_tx_busy; }
bool UART_IsBusy(UART_t *uart) { return uart->_tx_busy; }
bool UART_IsFree(UART_t *uart) { return !uart->_tx_busy; }

//-------------------------------------------------------------------------------------------- Send

// Bytes on the line: kept for the test and handed to its device, or out on the console
static void line_put(UART_t *uart, const uint8_t *data, uint16_t len)
{
  #ifdef HOST_TEST
  uint16_t room = sizeof(uart->_sent) - uart->_sent_len;
  uint16_t kept = len < room ? len : room;
  memcpy(uart->_sent + uart->_sent_len, data, kept);
  uart->_sent_len += kept;
  if(uart->_Line) uart->_Line(uart->_ctx, data, len);
  _TEST_Output(uart, data, len);
  #else
  fwrite(data, 1, len, stdout);
  fflush(stdout);
  #endif
}

// As on the chip: nothing to send is refused, `dir` set for the bytes, `prefix` ahead of them
status_t UART_Send(UART_t *uart, const uint8_t *data, uint16_t len)
{
  if(!uart->_init) return ERR;
  if(uart->_tx_busy) return BUSY;
  if(!len) return ERR;
  uart->_tx_busy = true;
  if(uart->dir) GPIO_Set(uart->dir);
  if(uart->prefix) line_put(uart, &uart->prefix, 1);
  line_put(uart, data, len);
  if(uart->dir) GPIO_Rst(uart->dir);
  uart->_tx_busy = false;
  return OK;
}

//----------------------------------------------------------------------------------------- Receive

uint16_t UART_Size(UART_t *uart)
{
  console_drain(uart);
  return BUFF_Size(uart->buff);
}

uint16_t UART_Read(UART_t *uart, uint8_t *data)
{
  console_drain(uart);
  return BUFF_Read(uart->buff, data);
}

char *UART_ReadString(UART_t *uart)
{
  console_drain(uart);
  return BUFF_ReadString(uart->buff);
}

bool UART_Skip(UART_t *uart)
{
  console_drain(uart);
  return BUFF_Skip(uart->buff);
}

void UART_Clear(UART_t *uart) { BUFF_Clear(uart->buff); }

// Frames waiting in the RX queue, the count `MODBUS_Loop` polls
uint16_t UART_MessageCount(UART_t *uart) { return BUFF_MessageCount(uart->buff); }

//------------------------------------------------------------------------------------------- Utils

uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len)
{
  if(!uart->baud) return 0;
  uint32_t bits = 10;
  if(uart->parity) bits++;
  if(uart->stop_bits >= UART_StopBits_1_5) bits++;
  return (uint32_t)(((uint64_t)bits * len * 1000) / uart->baud);
}

//-------------------------------------------------------------------------------------------- Host
#if defined(HOST_TEST)

uint16_t UART_HostSent(UART_t *uart, uint8_t *data)
{
  uint16_t len = uart->_sent_len;
  if(data) memcpy(data, uart->_sent, len);
  uart->_sent_len = 0;
  return len;
}

void UART_HostAttach(UART_t *uart, UART_HostLine_t Line, void *ctx)
{
  uart->_Line = Line;
  uart->_ctx = ctx;
}

#endif
//-------------------------------------------------------------------------------------------------

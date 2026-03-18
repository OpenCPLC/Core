// hal/host/uart.c

#include "uart.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <conio.h>
#else
  #include <unistd.h>
  #include <pthread.h>
  #include <termios.h>
  #include <fcntl.h>
  #include <sys/select.h>
#endif

//------------------------------------------------------------------------------------------------- Console setup
#if defined(_WIN32) || defined(_WIN64)

static HANDLE hStdin;
static DWORD orig_mode;

static void UART_InitConsole(void)
{
  hStdin = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hStdin, &orig_mode);
  SetConsoleMode(hStdin, orig_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
}

static void UART_DeinitConsole(void)
{
  SetConsoleMode(hStdin, orig_mode);
}

static int console_kbhit(void) { return _kbhit(); }
static int console_getch(void) { return _getch(); }

#else //------------------------------------------------------------------------------------------- Linux

static struct termios orig_termios;

static void UART_InitConsole(void)
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

static void UART_DeinitConsole(void)
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
//------------------------------------------------------------------------------------------------- RX Thread
#if defined(_WIN32) || defined(_WIN64)

static DWORD WINAPI uart_rx_thread(LPVOID param)
{
  UART_t *uart = (UART_t *)param;
  while(uart->_running) {
    if(console_kbhit()) {
      int c = console_getch();
      if(c >= 0) {
        BUFF_Push(uart->buff, (uint8_t)c);
        if(c == '\r' || c == '\n') BUFF_Break(uart->buff);
      }
    }
    Sleep(1);
  }
  return 0;
}

#else //------------------------------------------------------------------------------------------- Linux

static void *uart_rx_thread(void *param)
{
  UART_t *uart = (UART_t *)param;
  while(uart->_running) {
    if(console_kbhit()) {
      int c = console_getch();
      if(c >= 0) {
        BUFF_Push(uart->buff, (uint8_t)c);
        if(c == '\r' || c == '\n') BUFF_Break(uart->buff);
      }
    }
    usleep(1000);
  }
  return NULL;
}

#endif
//------------------------------------------------------------------------------------------------- Internal

static void UART_StartRxThread(UART_t *uart)
{
  uart->_running = true;
  #if defined(_WIN32) || defined(_WIN64)
    uart->_rx_thread = CreateThread(NULL, 0, uart_rx_thread, uart, 0, NULL);
  #else
    pthread_create((pthread_t *)&uart->_rx_thread, NULL, uart_rx_thread, uart);
  #endif
}

static void UART_StopRxThread(UART_t *uart)
{
  uart->_running = false;
  #if defined(_WIN32) || defined(_WIN64)
    if(uart->_rx_thread) {
      WaitForSingleObject(uart->_rx_thread, 1000);
      CloseHandle(uart->_rx_thread);
      uart->_rx_thread = NULL;
    }
  #else
    if(uart->_rx_thread) {
      pthread_join((pthread_t)uart->_rx_thread, NULL);
      uart->_rx_thread = 0;
    }
  #endif
}

//------------------------------------------------------------------------------------------------- API

status_t UART_Init(UART_t *uart)
{
  if(!uart->buff) return ERR;
  BUFF_Init(uart->buff);
  UART_InitConsole();
  UART_StartRxThread(uart);
  uart->_init = true;
  return OK;
}

void UART_DeInit(UART_t *uart)
{
  if(!uart->_init) return;
  UART_StopRxThread(uart);
  UART_DeinitConsole();
  uart->_init = false;
}

status_t UART_ReInit(UART_t *uart)
{
  UART_DeInit(uart);
  return UART_Init(uart);
}

void UART_SetTimeout(UART_t *uart, uint16_t timeout) { uart->timeout = timeout; }

//------------------------------------------------------------------------------------------------- Status

bool UART_SendCompleted(UART_t *uart) { return !uart->_tx_busy; }
bool UART_SendActive(UART_t *uart) { return uart->_tx_busy; }
bool UART_IsBusy(UART_t *uart) { return uart->_tx_busy; }
bool UART_IsFree(UART_t *uart) { return !uart->_tx_busy; }

//------------------------------------------------------------------------------------------------- Send

status_t UART_Send(UART_t *uart, uint8_t *data, uint16_t len)
{
  if(!uart->_init) return ERR;
  if(uart->_tx_busy) return BUSY;
  uart->_tx_busy = true;
  fwrite(data, 1, len, stdout);
  fflush(stdout);
  uart->_tx_busy = false;
  return OK;
}

//------------------------------------------------------------------------------------------------- Receive

uint16_t UART_Size(UART_t *uart) { return BUFF_Size(uart->buff); }
uint16_t UART_Read(UART_t *uart, uint8_t *data) { return BUFF_Read(uart->buff, data); }
char *UART_ReadString(UART_t *uart) { return BUFF_ReadString(uart->buff); }
bool UART_Skip(UART_t *uart) { return BUFF_Skip(uart->buff); }
void UART_Clear(UART_t *uart) { BUFF_Clear(uart->buff); }

//------------------------------------------------------------------------------------------------- Utils

uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len)
{
  if(!uart->baud) return 0;
  uint32_t bits = 10;
  if(uart->parity) bits++;
  if(uart->stop_bits >= UART_StopBits_1_5) bits++;
  return (uint32_t)(((uint64_t)bits * len * 1000) / uart->baud);
}

//-------------------------------------------------------------------------------------------------
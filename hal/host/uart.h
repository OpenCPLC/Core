// hal/host/uart.h

#ifndef UART_H_
#define UART_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "buff.h"

//------------------------------------------------------------------------------------------------- Config

#ifndef UART_PORT_NAME_MAX
  #define UART_PORT_NAME_MAX 32
#endif

//------------------------------------------------------------------------------------------------- Presets

#define UART_115200  baud = 115200, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_57600   baud = 57600,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_19200   baud = 19200,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_9600    baud = 9600,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1

//------------------------------------------------------------------------------------------------- Types

typedef enum {
  UART_Parity_None = 0,
  UART_Parity_Odd = 1,
  UART_Parity_Even = 2
} UART_Parity_t;

typedef enum {
  UART_StopBits_0_5 = 0,
  UART_StopBits_1 = 1,
  UART_StopBits_2 = 2,
  UART_StopBits_1_5 = 3
} UART_StopBits_t;

//------------------------------------------------------------------------------------------------- Structure

typedef struct {
  uint32_t baud;
  UART_Parity_t parity;
  UART_StopBits_t stop_bits;
  uint16_t timeout;
  BUFF_t *buff;
  // internal
  #if defined(_WIN32) || defined(_WIN64)
    void *_rx_thread;
  #else
    unsigned long _rx_thread;
  #endif
  volatile bool _tx_busy;
  volatile bool _running;
  bool _init;
} UART_t;

//------------------------------------------------------------------------------------------------- API

status_t UART_Init(UART_t *uart);
void UART_DeInit(UART_t *uart);
status_t UART_ReInit(UART_t *uart);
void UART_SetTimeout(UART_t *uart, uint16_t timeout);

bool UART_SendCompleted(UART_t *uart);
bool UART_SendActive(UART_t *uart);
bool UART_IsBusy(UART_t *uart);
bool UART_IsFree(UART_t *uart);

status_t UART_Send(UART_t *uart, uint8_t *data, uint16_t len);

uint16_t UART_Size(UART_t *uart);
uint16_t UART_Read(UART_t *uart, uint8_t *data);
char *UART_ReadString(UART_t *uart);
bool UART_Skip(UART_t *uart);
void UART_Clear(UART_t *uart);

uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len);

//-------------------------------------------------------------------------------------------------

static const uint8_t _host_uid[12] = {
  0x48, 0x4F, 0x53, 0x54, // "HOST"
  0x50, 0x4C, 0x41, 0x54, // "PLAT"
  0x46, 0x4F, 0x52, 0x4D  // "FORM"
};
#define UID_BASE ((uintptr_t)(const void *)_host_uid)

#define __NOP()         ((void)0)
#define __WFI()         ((void)0)
#define __WFE()         ((void)0)
#define __SEV()         ((void)0)
#define __ISB()         ((void)0)
#define __DSB()         ((void)0)
#define __DMB()         ((void)0)

//-------------------------------------------------------------------------------------------------
#endif
// hal/stm32wb/uart_wb.h

#ifndef UART_WB_H_
#define UART_WB_H_

#include "gpio.h"

//-------------------------------------------------------------------------------------------------

typedef enum {
  UART_TX_None = 0,
  UART1_TX_PA9,
  UART1_TX_PB6,
  LPUART1_TX_PA2,
  LPUART1_TX_PA9,
  LPUART1_TX_PB5,
  LPUART1_TX_PC1,
  LPUART1_TX_PC4,
  LPUART1_TX_PC10
} UART_TX_t;

typedef enum {
  UART_RX_None = 0,
  UART1_RX_PA10,
  UART1_RX_PB7,
  LPUART1_RX_PA3,
  LPUART1_RX_PA10,
  LPUART1_RX_PB10,
  LPUART1_RX_PC0,
  LPUART1_RX_PC5,
  LPUART1_RX_PC11
} UART_RX_t;

//-------------------------------------------------------------------------------------------------

#endif
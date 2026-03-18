// hal/stm32wb/uart_wb.c

#include "uart.h"

//-------------------------------------------------------------------------------------------------

const GPIO_Map_t UART_TX_MAP[] = {
  [UART1_TX_PA9]    = { .port = GPIOA, .pin = 9,  .alternate = 7 },
  [UART1_TX_PB6]    = { .port = GPIOB, .pin = 6,  .alternate = 7 },
  [LPUART1_TX_PA2]  = { .port = GPIOA, .pin = 2,  .alternate = 8 },
  [LPUART1_TX_PA9]  = { .port = GPIOA, .pin = 9,  .alternate = 8 },
  [LPUART1_TX_PB5]  = { .port = GPIOB, .pin = 5,  .alternate = 8 },
  [LPUART1_TX_PC1]  = { .port = GPIOC, .pin = 1,  .alternate = 8 },
  [LPUART1_TX_PC4]  = { .port = GPIOC, .pin = 4,  .alternate = 8 },
  [LPUART1_TX_PC10] = { .port = GPIOC, .pin = 10, .alternate = 8 }
};

const GPIO_Map_t UART_RX_MAP[] = {
  [UART1_RX_PA10]   = { .port = GPIOA, .pin = 10, .alternate = 7 },
  [UART1_RX_PB7]    = { .port = GPIOB, .pin = 7,  .alternate = 7 },
  [LPUART1_RX_PA3]  = { .port = GPIOA, .pin = 3,  .alternate = 8 },
  [LPUART1_RX_PA10] = { .port = GPIOA, .pin = 10, .alternate = 8 },
  [LPUART1_RX_PB10] = { .port = GPIOB, .pin = 10, .alternate = 8 },
  [LPUART1_RX_PC0]  = { .port = GPIOC, .pin = 0,  .alternate = 8 },
  [LPUART1_RX_PC5]  = { .port = GPIOC, .pin = 5,  .alternate = 8 },
  [LPUART1_RX_PC11] = { .port = GPIOC, .pin = 11, .alternate = 8 }
};

//-------------------------------------------------------------------------------------------------
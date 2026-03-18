// hal/stm32g0/uart_g0.c

#include "uart.h"

//-------------------------------------------------------------------------------------------------

const GPIO_Map_t UART_TX_MAP[] = {
  [UART1_TX_PA9]    = { .port = GPIOA, .pin = 9,  .alternate = 1 },
  [UART1_TX_PB6]    = { .port = GPIOB, .pin = 6,  .alternate = 0 },
  [UART1_TX_PC4]    = { .port = GPIOC, .pin = 4,  .alternate = 1 },
  [UART2_TX_PA2]    = { .port = GPIOA, .pin = 2,  .alternate = 1 },
  [UART2_TX_PA14]   = { .port = GPIOA, .pin = 14, .alternate = 1 },
  [UART2_TX_PD5]    = { .port = GPIOD, .pin = 5,  .alternate = 0 },
  [UART3_TX_PB2]    = { .port = GPIOB, .pin = 2,  .alternate = 4 },
  [UART3_TX_PB8]    = { .port = GPIOB, .pin = 8,  .alternate = 4 },
  [UART3_TX_PB10]   = { .port = GPIOB, .pin = 10, .alternate = 4 },
  [UART3_TX_PC4]    = { .port = GPIOC, .pin = 4,  .alternate = 0 },
  [UART3_TX_PC10]   = { .port = GPIOC, .pin = 10, .alternate = 0 },
  [UART3_TX_PD8]    = { .port = GPIOD, .pin = 8,  .alternate = 0 },
  [UART4_TX_PA0]    = { .port = GPIOA, .pin = 0,  .alternate = 4 },
  [UART4_TX_PC10]   = { .port = GPIOC, .pin = 10, .alternate = 1 },
  [LPUART1_TX_PA2]  = { .port = GPIOA, .pin = 2,  .alternate = 6 },
  [LPUART1_TX_PB11] = { .port = GPIOB, .pin = 11, .alternate = 1 },
  [LPUART1_TX_PC1]  = { .port = GPIOC, .pin = 1,  .alternate = 1 },
  #ifdef LPUART2
  [LPUART2_TX_PA2]  = { .port = GPIOA, .pin = 2,  .alternate = 8 },
  [LPUART2_TX_PC2]  = { .port = GPIOC, .pin = 2,  .alternate = 8 },
  [LPUART2_TX_PD5]  = { .port = GPIOD, .pin = 5,  .alternate = 7 }
  #endif
};

const GPIO_Map_t UART_RX_MAP[] = {
  [UART1_RX_PA10]   = { .port = GPIOA, .pin = 10, .alternate = 1 },
  [UART1_RX_PB7]    = { .port = GPIOB, .pin = 7,  .alternate = 0 },
  [UART1_RX_PC5]    = { .port = GPIOC, .pin = 5,  .alternate = 1 },
  [UART2_RX_PA3]    = { .port = GPIOA, .pin = 3,  .alternate = 1 },
  [UART2_RX_PA15]   = { .port = GPIOA, .pin = 15, .alternate = 1 },
  [UART2_RX_PD6]    = { .port = GPIOD, .pin = 6,  .alternate = 0 },
  [UART3_RX_PB0]    = { .port = GPIOB, .pin = 0,  .alternate = 4 },
  [UART3_RX_PB9]    = { .port = GPIOB, .pin = 9,  .alternate = 4 },
  [UART3_RX_PB11]   = { .port = GPIOB, .pin = 11, .alternate = 4 },
  [UART3_RX_PC5]    = { .port = GPIOC, .pin = 5,  .alternate = 0 },
  [UART3_RX_PC11]   = { .port = GPIOC, .pin = 11, .alternate = 0 },
  [UART3_RX_PD9]    = { .port = GPIOD, .pin = 9,  .alternate = 0 },
  [UART4_RX_PA1]    = { .port = GPIOA, .pin = 1,  .alternate = 4 },
  [UART4_RX_PC11]   = { .port = GPIOC, .pin = 11, .alternate = 1 },
  [LPUART1_RX_PA3]  = { .port = GPIOA, .pin = 3,  .alternate = 6 },
  [LPUART1_RX_PB10] = { .port = GPIOB, .pin = 10, .alternate = 1 },
  [LPUART1_RX_PC0]  = { .port = GPIOC, .pin = 0,  .alternate = 1 },
  #ifdef LPUART2
  [LPUART2_RX_PA3]  = { .port = GPIOA, .pin = 3,  .alternate = 8 },
  [LPUART2_RX_PC3]  = { .port = GPIOC, .pin = 3,  .alternate = 8 },
  [LPUART2_RX_PD6]  = { .port = GPIOD, .pin = 6,  .alternate = 7 }
  #endif
};

//-------------------------------------------------------------------------------------------------
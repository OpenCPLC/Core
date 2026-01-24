#ifndef UART_H_
#define UART_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "tim.h"
#include "buff.h"
#include "irq.h"
#if defined(STM32G0)
  #include "uart-g0.h"
#elif defined(STM32WB)
  #include "uart-wb.h"
#endif
#include "main.h"

//------------------------------------------------------------------------------------------------- Config

#define UART_CR1_RESET  0x00000000u
#define UART_CR2_RESET  0x00000000u
#define UART_CR3_RESET  0x00000000u
#define UART_ICR_CLEAR  0xFFFFFFFFu

#define UART_115200  baud = 115200, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_57600   baud = 57600,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_19200   baud = 19200,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_9600    baud = 9600,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1

//------------------------------------------------------------------------------------------------- Types

typedef enum {
  UART_Parity_None = 0,
  UART_Parity_Odd = 1,
  UART_Parity_Even = 2
} UART_Parity_e;

typedef enum {
  UART_StopBits_0_5 = 0,
  UART_StopBits_1 = 1,
  UART_StopBits_2 = 2,
  UART_StopBits_1_5 = 3
} UART_StopBits_e;

//------------------------------------------------------------------------------------------------- UART structure

/**
 * @brief UART control structure with DMA, buffer and optional timeout.
 * @param reg Pointer to UART register base
 * @param tx_pin TX pin mapping
 * @param rx_pin RX pin mapping
 * @param dma_nbr DMA channel for TX
 * @param irq_priority IRQ priority
 * @param baud Baudrate
 * @param parity Parity configuration
 * @param stop_bits Stop bits configuration
 * @param timeout Timeout in bit times (0 = disabled)
 * @param gpio_direction Optional GPIO for RS485 direction control
 * @param tim Optional timer for timeout (if no hardware RTO)
 * @param buff Pointer to receive buffer
 * @param prefix Optional address prefix byte
 */
typedef struct {
  // User config
  USART_TypeDef *reg;
  UART_TX_e tx_pin;
  UART_RX_e rx_pin;
  DMA_Nbr_t dma_nbr;
  IRQ_Priority_t irq_priority;
  uint32_t baud;
  UART_Parity_e parity;
  UART_StopBits_e stop_bits;
  uint16_t timeout;
  GPIO_t *gpio_direction;
  TIM_t *tim;
  BUFF_t *buff;
  uint8_t prefix;
  // Internal
  DMA_t dma;
  volatile bool tx_flag;
  volatile bool tc_flag;
  bool init_flag;
} UART_t;

//------------------------------------------------------------------------------------------------- API

void UART_Init(UART_t *uart);
void UART_ReInit(UART_t *uart);
void UART_SetTimeout(UART_t *uart, uint16_t timeout);
// Status flags
bool UART_SendCompleted(UART_t *uart);
bool UART_SendActive(UART_t *uart);
bool UART_IsBusy(UART_t *uart);
bool UART_IsFree(UART_t *uart);
// Send
status_t UART_Send(UART_t *uart, uint8_t *data, uint16_t len);
// Receive
uint16_t UART_Size(UART_t *uart);
uint16_t UART_Read(UART_t *uart, uint8_t *array);
char *UART_ReadString(UART_t *uart);
bool UART_Skip(UART_t *uart);
void UART_Clear(UART_t *uart);
// Utils
uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len);

//-------------------------------------------------------------------------------------------------
#endif
// hal/stm32/uart.h

#ifndef UART_H_
#define UART_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "tim.h"
#include "buff.h"
#include "irq.h"
#include "dma.h"
#include "main.h"

//------------------------------------------------------------------------------------------------- Presets

#define UART_CR1_RESET 0x00000000u
#define UART_CR2_RESET 0x00000000u
#define UART_CR3_RESET 0x00000000u
#define UART_ICR_CLEAR 0xFFFFFFFFu

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

//------------------------------------------------------------------------------------------------- Family Include

#if defined(STM32G0)
  #include "uart_g0.h"
#elif defined(STM32WB)
  #include "uart_wb.h"
#endif

//------------------------------------------------------------------------------------------------- Pin Maps

extern const GPIO_Map_t UART_TX_MAP[];
extern const GPIO_Map_t UART_RX_MAP[];

//------------------------------------------------------------------------------------------------- Structure

/**
 * @brief UART control structure with DMA TX and buffered RX.
 * @param[in] reg Pointer to USART peripheral registers
 * @param[in] tx TX pin mapping enum value
 * @param[in] rx RX pin mapping enum value
 * @param[in] dma DMA channel for TX
 * @param[in] irq_priority Interrupt priority for UART and DMA
 * @param[in] baud Baudrate
 * @param[in] parity Parity configuration
 * @param[in] stop_bits Stop bits configuration
 * @param[in] timeout RX timeout in bit times (0 = disabled)
 * @param[in] dir Optional GPIO for RS485 direction control (`NULL` = disabled)
 * @param[in] tim Optional timer for timeout (if no hardware RTO)
 * @param[in] buff Pointer to receive buffer
 * @param[in] prefix Optional address prefix byte (sent before DMA data)
 * Internal:
 * @param _dma DMA registers structure
 * @param _tx_busy TX DMA in progress flag
 * @param _tc_pending TX complete pending flag
 * @param _init Initialization completed flag
 */
typedef struct {
  USART_TypeDef *reg;
  UART_TX_t tx;
  UART_RX_t rx;
  DMA_CHx_t dma;
  IRQ_Priority_t irq_priority;
  uint32_t baud;
  UART_Parity_t parity;
  UART_StopBits_t stop_bits;
  uint16_t timeout;
  GPIO_t *dir;
  TIM_t *tim;
  BUFF_t *buff;
  uint8_t prefix;
  // internal
  DMA_t _dma;
  volatile bool _tx_busy;
  volatile bool _tc_pending;
  bool _init;
} UART_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize UART with DMA TX, buffered RX and optional timeout.
 * @param[in,out] uart Pointer to UART structure
 */
void UART_Init(UART_t *uart);

/**
 * @brief Reinitialize UART (waits for pending TX, resets peripheral).
 * @param[in,out] uart Pointer to UART structure
 */
void UART_ReInit(UART_t *uart);

/**
 * @brief Set RX timeout value.
 * @param[in,out] uart Pointer to UART structure
 * @param[in] timeout Timeout in bit times (0 = disable)
 */
void UART_SetTimeout(UART_t *uart, uint16_t timeout);

/**
 * @brief Check if TX is complete (DMA finished and shift register empty).
 * @param[in] uart Pointer to UART structure
 * @return `true` if TX complete
 */
bool UART_SendCompleted(UART_t *uart);

/**
 * @brief Check if TX is active (TC flag pending).
 * @param[in] uart Pointer to UART structure
 * @return `true` if TX active
 */
bool UART_SendActive(UART_t *uart);

/**
 * @brief Check if UART TX DMA is busy.
 * @param[in] uart Pointer to UART structure
 * @return `true` if busy
 */
bool UART_IsBusy(UART_t *uart);

/**
 * @brief Check if UART TX DMA is free.
 * @param[in] uart Pointer to UART structure
 * @return `true` if free
 */
bool UART_IsFree(UART_t *uart);

/**
 * @brief Start UART transmit via DMA.
 * @param[in,out] uart Pointer to UART structure
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to send
 * @return `OK` if started, `ERR` if not initialized, `BUSY` if transmitting
 */
status_t UART_Send(UART_t *uart, uint8_t *data, uint16_t len);

/**
 * @brief Get number of bytes in RX buffer.
 * @param[in] uart Pointer to UART structure
 * @return Number of bytes available
 */
uint16_t UART_Size(UART_t *uart);

/**
 * @brief Read data from RX buffer.
 * @param[in,out] uart Pointer to UART structure
 * @param[out] data Pointer to destination buffer
 * @return Number of bytes read
 */
uint16_t UART_Read(UART_t *uart, uint8_t *data);

/**
 * @brief Read RX buffer as null-terminated string.
 * @param[in,out] uart Pointer to UART structure
 * @return Pointer to string (valid until next read)
 */
char *UART_ReadString(UART_t *uart);

/**
 * @brief Skip current frame in RX buffer.
 * @param[in,out] uart Pointer to UART structure
 * @return `true` if frame was skipped
 */
bool UART_Skip(UART_t *uart);

/**
 * @brief Clear RX buffer.
 * @param[in,out] uart Pointer to UART structure
 */
void UART_Clear(UART_t *uart);

/**
 * @brief Calculate transmission time for frame.
 * @param[in] uart Pointer to UART structure
 * @param[in] len Frame length in bytes
 * @return Transmission time in milliseconds
 */
uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len);

//-------------------------------------------------------------------------------------------------
#endif
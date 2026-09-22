// hal/stm32/itf/uart.h

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

//----------------------------------------------------------------------------------------- Presets

#define UART_921600 baud = 921600, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_460800 baud = 460800, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_230400 baud = 230400, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_115200 baud = 115200, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_57600  baud = 57600,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_19200  baud = 19200,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_9600   baud = 9600,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1

// Preset by its number, so a project constant picks it: `.UART_Preset(MODULE_BAUD)`
#define UART_Preset(baud) _UART_Preset(baud)
#define _UART_Preset(baud) UART_##baud

//------------------------------------------------------------------------------------------- Types

// Data is always one byte. Parity does not take a data bit away,
// it adds a ninth bit to the word: a parity frame is 8 data bits plus 1 parity bit
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

//---------------------------------------------------------------------------------- Family include

#if defined(STM32G0)
  #include "uart_g0.h"
#elif defined(STM32WB)
  #include "uart_wb.h"
#endif

//---------------------------------------------------------------------------------------- Pin maps

extern const GPIO_Map_t UART_TX_MAP[];
extern const GPIO_Map_t UART_RX_MAP[];

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief UART with DMA transmit and a framed receive ring.
 *   A frame closes on the receiver timeout: the hardware one, or a timer when the port
 *   has none.
 * @param[in] reg USART peripheral registers
 * @param[in] tx TX pin mapping
 * @param[in] rx RX pin mapping
 * @param[in] dma TX DMA channel
 * @param[in] irq_priority Interrupt priority for UART, DMA and the timeout timer
 * @param[in] baud Baud rate [bit/s]
 * @param[in] parity Parity
 * @param[in] stop_bits Stop bits
 * @param[in] timeout Receiver timeout closing a frame [bit times], `0` = off
 * @param[in] dir RS485 direction GPIO, `NULL` = none
 * @param[in] tim Timeout timer for a port without the hardware receiver timeout
 * @param[in] buff Receive ring
 * @param[in] prefix Address byte sent ahead of every transfer, `0` = none
 * Internal:
 * @param _dma TX DMA register set
 * @param _tx_busy DMA transfer in progress
 * @param _tc_pending Last byte still in the shift register
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

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize the port, pins, DMA, timeout and interrupts.
 * @param[in,out] uart Pointer to UART structure
 */
void UART_Init(UART_t *uart);

/**
 * @brief Stop the port, let a transfer in flight finish, then initialize it again.
 * @param[in,out] uart Pointer to UART structure
 */
void UART_ReInit(UART_t *uart);

/**
 * @brief Change the receiver timeout.
 * @param[in,out] uart Pointer to UART structure
 * @param[in] timeout Timeout [bit times], `0` = off
 */
void UART_SetTimeout(UART_t *uart, uint16_t timeout);

// Transmit state: `SendCompleted` once the last bit left the pin,
// `IsFree` once the DMA transfer is done and another one may start
bool UART_SendCompleted(UART_t *uart);
bool UART_SendActive(UART_t *uart);
bool UART_IsBusy(UART_t *uart);
bool UART_IsFree(UART_t *uart);

/**
 * @brief Start a DMA transmit, `data` must stay valid until `UART_IsFree`.
 * @param[in,out] uart Pointer to UART structure
 * @param[in] data Bytes to send
 * @param[in] len Number of bytes, `0` is refused
 * @return `OK` if started, `ERR` if not initialized or empty, `BUSY` if transmitting
 */
status_t UART_Send(UART_t *uart, const uint8_t *data, uint16_t len);

// Received frames, one at a time: bytes of the current one, frames waiting,
// copy out, as a heap string, skip, or drop them all
uint16_t UART_Size(UART_t *uart);
uint16_t UART_MessageCount(UART_t *uart);
uint16_t UART_Read(UART_t *uart, uint8_t *data);
char *UART_ReadString(UART_t *uart);
bool UART_Skip(UART_t *uart);
void UART_Clear(UART_t *uart);

/**
 * @brief Time on the wire of a frame of `len` bytes plus the receiver timeout.
 * @param[in] uart Pointer to UART structure
 * @param[in] len Frame length [B]
 * @return Transmission time [ms]
 */
uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len);

//-------------------------------------------------------------------------------------------------
#endif

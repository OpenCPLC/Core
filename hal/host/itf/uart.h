// hal/host/itf/uart.h

// Contract of the UART a host build compiles against: the console of a program,
// in a test the bytes it sends, kept for `UART_HostSent` and handed to the device on the line.
// The fields of the chip are kept, so a board file builds as it is;
// `reg`, the pins, `dma`, `irq_priority` and `tim` stay unused.

#ifndef UART_H_
#define UART_H_

#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "gpio.h"
#include "tim.h"
#include "irq.h"
#include "dma.h"
#include "xdef.h"
#include "buff.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef UART_PORT_NAME_MAX
  #define UART_PORT_NAME_MAX 32
#endif

//----------------------------------------------------------------------------------------- Presets

#define UART_921600 baud = 921600, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_460800 baud = 460800, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_230400 baud = 230400, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_115200 baud = 115200, .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_76800  baud = 76800,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_57600  baud = 57600,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_38400  baud = 38400,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_19200  baud = 19200,  .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_9600   baud = 9600,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_4800   baud = 4800,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_2400   baud = 2400,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_1200   baud = 1200,   .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_600    baud = 600,    .parity = UART_Parity_None, .stop_bits = UART_StopBits_1
#define UART_300    baud = 300,    .parity = UART_Parity_None, .stop_bits = UART_StopBits_1

// Preset by its number, so a project constant picks it: `.UART_Preset(MODULE_BAUD)`
#define UART_Preset(baud) _UART_Preset(baud)
#define _UART_Preset(baud) UART_##baud

//------------------------------------------------------------------------------------------- Types

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

#if defined(HOST_TEST)
/**
 * @brief Device on the line, taking the bytes of every transfer as they go out.
 * @param[in] ctx Context given to `UART_HostAttach`
 * @param[in] data Bytes on the line
 * @param[in] len Number of bytes
 */
typedef void (*UART_HostLine_t)(void *ctx, const uint8_t *data, uint16_t len);
#endif

//---------------------------------------------------------------------------------- Family include

// Pin names of the family of the chip the build stands for
#if defined(STM32WB55xx)
  #include "../../stm32wb/itf/uart_wb.h"
#else
  #include "../../stm32g0/itf/uart_g0.h"
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief UART over the console, or over the bytes a test reads.
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
 * @param _tx_busy Transfer in progress
 * @param _init Initialization completed flag
 * @param _sent Bytes put on the line since `UART_HostSent` took them, in a test
 * @param _sent_len Number of bytes in `_sent`
 * @param _Line Device on the line, in a test, `NULL` = none
 * @param _ctx Context of `_Line`
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
  volatile bool _tx_busy;
  bool _init;
  #if defined(HOST_TEST)
  uint8_t _sent[256];
  uint16_t _sent_len;
  UART_HostLine_t _Line;
  void *_ctx;
  #endif
} UART_t;

//--------------------------------------------------------------------------------------------- API

status_t UART_Init(UART_t *uart);
void UART_DeInit(UART_t *uart);
status_t UART_ReInit(UART_t *uart);
void UART_SetTimeout(UART_t *uart, uint16_t timeout);

bool UART_SendCompleted(UART_t *uart);
bool UART_SendActive(UART_t *uart);
bool UART_IsBusy(UART_t *uart);
bool UART_IsFree(UART_t *uart);

status_t UART_Send(UART_t *uart, const uint8_t *data, uint16_t len);

uint16_t UART_Size(UART_t *uart);
uint16_t UART_MessageCount(UART_t *uart);
uint16_t UART_Read(UART_t *uart, uint8_t *data);
char *UART_ReadString(UART_t *uart);
bool UART_Skip(UART_t *uart);
void UART_Clear(UART_t *uart);

// Bytes of the frame still coming in, before the receiver timeout closes it
uint16_t UART_Receiving(UART_t *uart);

uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len);

//-------------------------------------------------------------------------------------------- Host
#if defined(HOST_TEST)

/**
 * @brief Bytes the port put on the line since the last call, at most 256.
 * @param[in] uart Port
 * @param[out] data Buffer for 256 bytes, `NULL` drops them
 * @return Byte count
 */
uint16_t UART_HostSent(UART_t *uart, uint8_t *data);

/**
 * @brief Put a device on the line.
 * @param[in,out] uart Port
 * @param[in] Line Handler taking the bytes of every transfer, `NULL` takes the device away
 * @param[in] ctx Context passed to `Line`
 */
void UART_HostAttach(UART_t *uart, UART_HostLine_t Line, void *ctx);

#endif
//------------------------------------------------------------------------------------------ Device

// Unique ID, the 96 bits the target reads at `UID_BASE`
static const uint8_t HOST_UID[12] = {
  0x48, 0x4F, 0x53, 0x54, // "HOST"
  0x50, 0x4C, 0x41, 0x54, // "PLAT"
  0x46, 0x4F, 0x52, 0x4D  // "FORM"
};
#define UID_BASE ((uintptr_t)(const void *)HOST_UID)

// CMSIS intrinsics, nothing to wait for off-target
#define __NOP() ((void)0)
#define __WFI() ((void)0)
#define __WFE() ((void)0)
#define __SEV() ((void)0)
#define __ISB() ((void)0)
#define __DSB() ((void)0)
#define __DMB() ((void)0)

//-------------------------------------------------------------------------------------------------
#endif

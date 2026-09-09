// hal/stm32wb/rf/wpan_wb.h

#ifndef WPAN_WB_H_
#define WPAN_WB_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "irq.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef WPAN_READY_TIMEOUT_ms
  // Wait for the CPU2 ready event after releasing its boot
  #define WPAN_READY_TIMEOUT_ms 2000
#endif

#ifndef WPAN_CMD_TIMEOUT_ms
  // Wait for a command response on the system or BLE channel
  #define WPAN_CMD_TIMEOUT_ms 1000
#endif

#ifndef WPAN_CLOCK_TIMEOUT_ms
  // Wait for HSE and LSE readiness; a cold 32kHz crystal can take seconds to swing
  #define WPAN_CLOCK_TIMEOUT_ms 5000
#endif

#ifndef WPAN_EVENT_LIMIT
  // Asynchronous BLE events held between interrupt and `WPAN_BleEvent`
  #define WPAN_EVENT_LIMIT 8
#endif

#ifndef WPAN_EVENT_QUEUE
  // Event buffers in the pool handed to the CPU2 memory manager
  #define WPAN_EVENT_QUEUE 5
#endif

#ifndef WPAN_RSP_SIZE
  // Command return parameters kept for the caller [B], the status byte excluded
  #define WPAN_RSP_SIZE 32
#endif

// Stack sizing passed in `SHCI_C2_BLE_INIT`, fields described in `wpan_abi.h`
#ifndef WPAN_BLE_ATT_MTU
  #define WPAN_BLE_ATT_MTU 247
#endif
#ifndef WPAN_BLE_LINKS
  #define WPAN_BLE_LINKS 1
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  WPAN_Fw_None = 0,     // CPU2 has not reported yet
  WPAN_Fw_Wireless = 1, // wireless stack is running
  WPAN_Fw_Fus = 2       // firmware upgrade services run instead of the stack
} WPAN_Fw_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Bring up the CPU2 mailbox: shared tables, IPCC, radio clocks, CPU2 boot.
 *   Blocks cooperatively until the CPU2 reports ready, so call it from a thread.
 * @param[in] priority IPCC interrupt priority
 * @return `OK` when the wireless stack runs, `ERR` on timeout, FUS or a misplaced table
 */
status_t WPAN_Start(IRQ_Priority_t priority);

// Firmware reported by the CPU2 ready event
WPAN_Fw_t WPAN_Firmware(void);

// Wireless stack version word: major.minor.sub.release, one byte each from the top
uint32_t WPAN_StackVersion(void);

/**
 * @brief Start the BLE stack on CPU2.
 * @param[in] attributes GATT records of the application, its own 9 included
 * @param[in] services GATT services, GAP and GATT services included
 * @param[in] values Attribute value storage [B]
 * @return Status byte of the response, `0` = success, `0xFF` = no response in time
 */
uint8_t WPAN_BleStackInit(uint16_t attributes, uint16_t services, uint16_t values);

/**
 * @brief Send one HCI or ACI command and wait for its response.
 *   One command at a time: call only from the thread that owns the BLE channel.
 * @param[in] opcode Command opcode
 * @param[in] param Command parameters, may be `NULL` when `len` is `0`
 * @param[in] len Parameter length [B]
 * @param[out] rsp Return parameters after the status byte, may be `NULL`
 * @param[in] limit Size of `rsp` [B]
 * @param[out] rsp_len Bytes stored in `rsp`, may be `NULL`
 * @return Status byte of the response, `0` = success, `0xFF` = no response in time
 */
uint8_t WPAN_BleCmd(uint16_t opcode, const uint8_t *param, uint8_t len,
  uint8_t *rsp, uint8_t limit, uint8_t *rsp_len);

/**
 * @brief Take oldest pending asynchronous BLE event.
 * @return Event bytes: `[0]` packet type, `[1]` event code, `[2]` payload length,
 *   `[3..]` payload; `NULL` when nothing is pending. Release with `WPAN_BleEventDone`.
 */
uint8_t *WPAN_BleEvent(void);

// Return an event taken with `WPAN_BleEvent` to the CPU2 buffer pool
void WPAN_BleEventDone(uint8_t *event);

// Events dropped on a full queue and error notifications from the CPU2
uint16_t WPAN_Errors(void);

/**
 * @brief Tell the CPU2 that flash erases begin or ended, so the radio stretches
 *   its timing around them. A quiet no-op before the mailbox runs.
 * @param[in] active `true` ahead of the erases, `false` after the last one
 */
void WPAN_FlashEraseActivity(bool active);

//-------------------------------------------------------------------------------------------------
#endif

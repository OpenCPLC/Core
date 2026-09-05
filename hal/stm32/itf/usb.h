// hal/stm32/itf/usb.h

#ifndef USB_H_
#define USB_H_

#include <stdbool.h>
#include <stdint.h>

#include "xdef.h"
#include "buff.h"
#include "irq.h"
#include "pwr.h"
#include "main.h"

//---------------------------------------------------------------------------------- Family Include

#if defined(STM32G0)
  #include "usb_g0.h"
#elif defined(STM32WB)
  #include "usb_wb.h"
#endif

#if(USB_PRESENT)

//------------------------------------------------------------------------------------------ Config

#ifndef USB_EP_LIMIT
  // Endpoints an instance describes, `EP0` included; array index is the endpoint number.
  // Three cover the bulk pair and the CDC notification endpoint.
  #define USB_EP_LIMIT 3
#endif

#ifndef USB_EVENT_LIMIT
  // Bus events held between interrupt and `USB_Event`
  #define USB_EVENT_LIMIT 4
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  USB_EpType_Control = 0,
  USB_EpType_Bulk = 1,
  USB_EpType_Interrupt = 2
} USB_EpType_t;

typedef enum {
  USB_Event_None = 0,
  USB_Event_Reset = 1,
  USB_Event_Suspend = 2,
  USB_Event_Resume = 3,
  USB_Event_Setup = 4
} USB_Event_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief One hardware endpoint, both directions.
 * @param[in] type Transfer type
 * @param[in] size Packet size [B], `8..64`, shared by both directions
 * @param[in] rx OUT stream, `NULL` = OUT packets are acknowledged and dropped
 * @param[in] tx IN direction in use
 * Internal:
 * @param _tx_data Transfer in flight
 * @param _tx_len Transfer length [B]
 * @param _tx_pos Bytes already handed to packet memory
 * @param _tx_zlp Zero-length packet closes the transfer
 * @param _tx_busy Transfer in flight flag
 * @param _pma_tx IN buffer offset in packet memory [B]
 * @param _pma_rx OUT buffer offset in packet memory [B]
 */
typedef struct {
  USB_EpType_t type;
  uint16_t size;
  BUFF_t *rx;
  bool tx;
  // internal
  const uint8_t *_tx_data;
  volatile uint16_t _tx_len;
  volatile uint16_t _tx_pos;
  volatile bool _tx_zlp;
  volatile bool _tx_busy;
  uint16_t _pma_tx;
  uint16_t _pma_rx;
} USB_Ep_t;

/**
 * @brief USB full-speed device controller.
 * Interrupt context moves packets between packet memory and the endpoint buffers,
 * everything else waits in the event queue for `USB_Event`.
 * @param[in] irq_priority Interrupt priority
 * @param[in] ep Endpoints by number, `ep[0]` is the control endpoint
 * @param[out] overflow OUT bytes dropped on a full `rx` buffer
 * @param[out] errors Bus errors, packet memory overruns and lost events
 * @param[out] resets Bus resets seen
 * Internal:
 * @param _event Pending events, oldest first
 * @param _event_head Write index
 * @param _event_tail Read index
 * @param _setup Last SETUP packet
 * @param _setup_pending SETUP packet not taken yet
 * @param _address Address applied once the status stage completes
 * @param _init Initialization completed flag
 */
typedef struct {
  IRQ_Priority_t irq_priority;
  USB_Ep_t ep[USB_EP_LIMIT];
  volatile uint16_t overflow;
  volatile uint16_t errors;
  volatile uint16_t resets;
  // internal
  volatile uint8_t _event[USB_EVENT_LIMIT];
  volatile uint8_t _event_head;
  volatile uint8_t _event_tail;
  uint8_t _setup[8];
  volatile bool _setup_pending;
  volatile uint8_t _address;
  bool _init;
} USB_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Start controller and attach to the bus.
 *   `EP0` comes up on every bus reset, other endpoints wait for `USB_EpEnable`.
 * @param[in,out] usb Pointer to USB structure
 * @return `OK` on success, `ERR` when endpoint sizes do not fit packet memory
 */
status_t USB_Init(USB_t *usb);

/**
 * @brief Take oldest pending event.
 * @param[in,out] usb Pointer to USB structure
 * @param[out] setup Receives 8 bytes on `USB_Event_Setup`
 * @return Event, `USB_Event_None` when nothing is pending
 */
USB_Event_t USB_Event(USB_t *usb, uint8_t *setup);

/**
 * @brief Queue event for `USB_Event`. Driver side, interrupt context.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] event Event to queue, a full queue counts in `errors`
 */
void USB_EventPush(USB_t *usb, USB_Event_t event);

/**
 * @brief Set device address.
 *   Takes effect after the next `EP0` IN completes: the status stage of `SET_ADDRESS`.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] address Address assigned by host, `1..127`
 */
void USB_SetAddress(USB_t *usb, uint8_t address);

/**
 * @brief Activate endpoint: OUT accepts packets, IN idles, data toggles cleared.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 */
void USB_EpEnable(USB_t *usb, uint8_t ep);

/**
 * @brief Deactivate endpoint. A transfer in flight is dropped.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 */
void USB_EpDisable(USB_t *usb, uint8_t ep);

/**
 * @brief Halt one direction. `EP0` halts both until the next SETUP.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @param[in] in `true` = IN direction, `false` = OUT direction
 */
void USB_EpStall(USB_t *usb, uint8_t ep, bool in);

/**
 * @brief Clear halt and reset data toggle of one direction.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @param[in] in `true` = IN direction, `false` = OUT direction
 */
void USB_EpClear(USB_t *usb, uint8_t ep, bool in);

/**
 * @brief Check halt state of one direction.
 * @param[in] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @param[in] in `true` = IN direction, `false` = OUT direction
 * @return `true` if halted
 */
bool USB_EpStalled(USB_t *usb, uint8_t ep, bool in);

/**
 * @brief Start IN transfer. Packets are copied from `data` in interrupt context
 *   until the transfer completes, so `data` must stay valid until `USB_IsFree`.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @param[in] data Pointer to data, may be `NULL` when `len` is `0`
 * @param[in] len Number of bytes, `0` sends one zero-length packet
 * @param[in] zlp Close a transfer ending on a packet boundary with a zero-length packet
 * @return `OK` if started, `ERR` if IN is unavailable or halted, `BUSY` if in flight
 */
status_t USB_Send(USB_t *usb, uint8_t ep, const uint8_t *data, uint16_t len, bool zlp);

/**
 * @brief Check if IN transfer is in flight.
 * @param[in] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @return `true` if busy
 */
bool USB_IsBusy(USB_t *usb, uint8_t ep);

/**
 * @brief Check if IN direction is free for `USB_Send`.
 * @param[in] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @return `true` if free
 */
bool USB_IsFree(USB_t *usb, uint8_t ep);

//-------------------------------------------------------------------------------------------------
#endif
#endif

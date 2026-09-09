// hal/host/itf/usb.h

// Contract of the USB device controller a host build compiles against, with the bus
// modelled in memory: what the device sends lands in a per-endpoint buffer the test reads,
// what the test writes lands in the endpoint stream the way packets would.

#ifndef USB_H_
#define USB_H_

#include <stdbool.h>
#include <stdint.h>

#include "xdef.h"
#include "buff.h"
#include "irq.h"
#include "main.h"

#define USB_PRESENT 1

//------------------------------------------------------------------------------------------ Config

#ifndef USB_EP_LIMIT
  // Endpoints an instance describes, `EP0` included; array index is the endpoint number.
  // Three cover the bulk pair and the CDC notification endpoint.
  #define USB_EP_LIMIT 3
#endif

#ifndef USB_EVENT_LIMIT
  // Bus events held between the model and `USB_Event`
  #define USB_EVENT_LIMIT 4
#endif

#ifndef USB_HOST_IN_SIZE
  // Bytes the modelled host holds per endpoint before the test reads them
  #define USB_HOST_IN_SIZE 512
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

typedef struct {
  USB_EpType_t type;
  uint16_t size;
  BUFF_t *rx;
  bool tx;
  // internal
  volatile bool _tx_busy;
  bool _enabled;
  bool _stall_tx;
  bool _stall_rx;
  uint8_t _in[USB_HOST_IN_SIZE]; // sent by the device, not read by the test yet
  uint16_t _in_len;
} USB_Ep_t;

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
  uint8_t _daddr;    // address on the bus
  bool _attached;
  bool _init;
} USB_t;

//--------------------------------------------------------------------------------------------- API

status_t USB_Init(USB_t *usb);
USB_Event_t USB_Event(USB_t *usb, uint8_t *setup);
void USB_SetAddress(USB_t *usb, uint8_t address);
void USB_EpEnable(USB_t *usb, uint8_t ep);
void USB_EpDisable(USB_t *usb, uint8_t ep);
void USB_EpStall(USB_t *usb, uint8_t ep, bool in);
void USB_EpClear(USB_t *usb, uint8_t ep, bool in);
bool USB_EpStalled(USB_t *usb, uint8_t ep, bool in);
status_t USB_Send(USB_t *usb, uint8_t ep, const uint8_t *data, uint16_t len, bool zlp);
bool USB_IsBusy(USB_t *usb, uint8_t ep);
bool USB_IsFree(USB_t *usb, uint8_t ep);

//--------------------------------------------------------------------------------------- Host side

// Bus reset from the host: endpoints above `0` drop, address returns to `0`
void USB_HostReset(USB_t *usb);

// Bus idle for 3ms or activity after it
void USB_HostSuspend(USB_t *usb, bool suspend);

/**
 * @brief SETUP packet on `EP0`. Abandons an answer still in flight, as the silicon does.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] setup 8 bytes, little-endian fields
 */
void USB_HostSetup(USB_t *usb, const uint8_t *setup);

/**
 * @brief OUT transfer: packets of endpoint size, a short or empty last packet closes it.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @param[in] data Pointer to data
 * @param[in] len Number of bytes
 * @return `true` if the endpoint took it, `false` if disabled, halted or without a stream
 */
bool USB_HostWrite(USB_t *usb, uint8_t ep, const uint8_t *data, uint16_t len);

/**
 * @brief Take what the device has sent on an IN endpoint since the last read.
 * @param[in,out] usb Pointer to USB structure
 * @param[in] ep Endpoint number
 * @param[out] dst Destination, may be `NULL` to discard
 * @param[in] limit Destination size [B]
 * @return Bytes taken
 */
uint16_t USB_HostRead(USB_t *usb, uint8_t ep, uint8_t *dst, uint16_t limit);

// Address the device answers to, `0` before `SET_ADDRESS` completes
uint8_t USB_HostAddress(USB_t *usb);

//-------------------------------------------------------------------------------------------------
#endif

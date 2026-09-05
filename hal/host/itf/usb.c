// hal/host/itf/usb.c

#include "usb.h"

#include <string.h>

//------------------------------------------------------------------------------------------ Events

static void event_push(USB_t *usb, USB_Event_t event)
{
  uint8_t next = (uint8_t)((usb->_event_head + 1) % USB_EVENT_LIMIT);
  if(next == usb->_event_tail) {
    usb->errors++;
    return;
  }
  usb->_event[usb->_event_head] = event;
  usb->_event_head = next;
}

USB_Event_t USB_Event(USB_t *usb, uint8_t *setup)
{
  if(usb->_event_head != usb->_event_tail) {
    USB_Event_t event = usb->_event[usb->_event_tail];
    usb->_event_tail = (uint8_t)((usb->_event_tail + 1) % USB_EVENT_LIMIT);
    if(event != USB_Event_Setup) return event;
  }
  if(!usb->_setup_pending) return USB_Event_None;
  memcpy(setup, usb->_setup, 8);
  usb->_setup_pending = false;
  return USB_Event_Setup;
}

//--------------------------------------------------------------------------------------- Endpoints

static void ep_activate(USB_t *usb, uint8_t ep)
{
  USB_Ep_t *endpoint = &usb->ep[ep];
  endpoint->_enabled = true;
  endpoint->_stall_tx = false;
  endpoint->_stall_rx = false;
  endpoint->_tx_busy = false;
  endpoint->_in_len = 0;
}

static void ep_deactivate(USB_t *usb, uint8_t ep)
{
  USB_Ep_t *endpoint = &usb->ep[ep];
  endpoint->_enabled = false;
  endpoint->_tx_busy = false;
}

status_t USB_Init(USB_t *usb)
{
  for(uint8_t ep = 0; ep < USB_EP_LIMIT; ep++) {
    USB_Ep_t *endpoint = &usb->ep[ep];
    if(!endpoint->tx && !endpoint->rx && ep) continue;
    if(endpoint->size < 8 || endpoint->size > 64) return ERR;
    ep_deactivate(usb, ep);
  }
  usb->_daddr = 0;
  usb->_attached = true;
  usb->_init = true;
  return OK;
}

// Bit `7` marks the address as pending, so address zero gets applied like any other
void USB_SetAddress(USB_t *usb, uint8_t address) { usb->_address = 0x80u | address; }

void USB_EpEnable(USB_t *usb, uint8_t ep)
{
  if(ep < USB_EP_LIMIT) ep_activate(usb, ep);
}

void USB_EpDisable(USB_t *usb, uint8_t ep)
{
  if(ep < USB_EP_LIMIT) ep_deactivate(usb, ep);
}

void USB_EpStall(USB_t *usb, uint8_t ep, bool in)
{
  if(ep >= USB_EP_LIMIT) return;
  USB_Ep_t *endpoint = &usb->ep[ep];
  if(in || !ep) endpoint->_stall_tx = true;
  if(!in || !ep) endpoint->_stall_rx = true;
  endpoint->_tx_busy = false;
}

void USB_EpClear(USB_t *usb, uint8_t ep, bool in)
{
  if(ep >= USB_EP_LIMIT) return;
  USB_Ep_t *endpoint = &usb->ep[ep];
  if(in) {
    endpoint->_stall_tx = false;
    endpoint->_tx_busy = false;
  }
  else endpoint->_stall_rx = false;
}

bool USB_EpStalled(USB_t *usb, uint8_t ep, bool in)
{
  if(ep >= USB_EP_LIMIT) return false;
  return in ? usb->ep[ep]._stall_tx : usb->ep[ep]._stall_rx;
}

//-------------------------------------------------------------------------------------------- Send

// The modelled host drains every IN packet at once, so a transfer completes inside the call.
status_t USB_Send(USB_t *usb, uint8_t ep, const uint8_t *data, uint16_t len, bool zlp)
{
  unused(zlp);
  if(!usb->_init || ep >= USB_EP_LIMIT || !(usb->ep[ep].tx || !ep)) return ERR;
  USB_Ep_t *endpoint = &usb->ep[ep];
  if(endpoint->_tx_busy) return BUSY;
  if(!endpoint->_enabled || endpoint->_stall_tx) return ERR;
  if(len > USB_HOST_IN_SIZE - endpoint->_in_len) {
    usb->errors++;
    return ERR;
  }
  if(len) memcpy(&endpoint->_in[endpoint->_in_len], data, len);
  endpoint->_in_len += len;
  // Status stage of `SET_ADDRESS` went out under the old address.
  if(!ep && usb->_address) {
    usb->_daddr = usb->_address & 0x7Fu;
    usb->_address = 0;
  }
  return OK;
}

bool USB_IsBusy(USB_t *usb, uint8_t ep) { return ep < USB_EP_LIMIT && usb->ep[ep]._tx_busy; }
bool USB_IsFree(USB_t *usb, uint8_t ep) { return !USB_IsBusy(usb, ep); }

//--------------------------------------------------------------------------------------- Host side

void USB_HostReset(USB_t *usb)
{
  for(uint8_t ep = 1; ep < USB_EP_LIMIT; ep++) ep_deactivate(usb, ep);
  usb->_address = 0;
  usb->_daddr = 0;
  ep_activate(usb, 0);
  usb->_event_head = 0;
  usb->_event_tail = 0;
  usb->_setup_pending = false;
  usb->resets++;
  event_push(usb, USB_Event_Reset);
}

void USB_HostSuspend(USB_t *usb, bool suspend)
{
  event_push(usb, suspend ? USB_Event_Suspend : USB_Event_Resume);
}

void USB_HostSetup(USB_t *usb, const uint8_t *setup)
{
  USB_Ep_t *endpoint = &usb->ep[0];
  memcpy(usb->_setup, setup, 8);
  usb->_address = 0; // an unapplied address dies with its request
  endpoint->_stall_tx = false;
  endpoint->_stall_rx = false;
  endpoint->_tx_busy = false;
  usb->_setup_pending = true;
  event_push(usb, USB_Event_Setup);
}

bool USB_HostWrite(USB_t *usb, uint8_t ep, const uint8_t *data, uint16_t len)
{
  if(ep >= USB_EP_LIMIT) return false;
  USB_Ep_t *endpoint = &usb->ep[ep];
  if(!endpoint->_enabled || endpoint->_stall_rx || !endpoint->rx) return false;
  for(uint16_t i = 0; i < len; i++) {
    if(!BUFF_Push(endpoint->rx, data[i]) && !endpoint->rx->console_mode) usb->overflow++;
  }
  if(!len || len % endpoint->size) BUFF_Break(endpoint->rx); // end of transfer
  return true;
}

uint16_t USB_HostRead(USB_t *usb, uint8_t ep, uint8_t *dst, uint16_t limit)
{
  if(ep >= USB_EP_LIMIT) return 0;
  USB_Ep_t *endpoint = &usb->ep[ep];
  uint16_t len = endpoint->_in_len < limit ? endpoint->_in_len : limit;
  if(dst && len) memcpy(dst, endpoint->_in, len);
  endpoint->_in_len = 0;
  return len;
}

uint8_t USB_HostAddress(USB_t *usb) { return usb->_daddr; }

//-------------------------------------------------------------------------------------------------

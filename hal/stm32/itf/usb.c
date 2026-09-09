// hal/stm32/itf/usb.c

#include "usb.h"

#if(USB_PRESENT)

#include <string.h>

//------------------------------------------------------------------------------------------ Events

void USB_EventPush(USB_t *usb, USB_Event_t event)
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
  // The pop is fenced whole: a bus reset lands mid-pop from the interrupt and rewinds
  // both indices, and a tail written from the stale value would desync the queue
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  USB_Event_t event = USB_Event_None;
  if(usb->_event_head != usb->_event_tail) {
    event = usb->_event[usb->_event_tail];
    usb->_event_tail = (uint8_t)((usb->_event_tail + 1) % USB_EVENT_LIMIT);
  }
  __set_PRIMASK(primask);
  if(event != USB_Event_None && event != USB_Event_Setup) return event;
  // SETUP is reported from its own flag: a bus reset clears it together with the queue,
  // so a stale packet never outlives the reset that made it stale. The copy is fenced,
  // or a SETUP arriving mid-copy would hand out a torn packet.
  if(!usb->_setup_pending) return USB_Event_None;
  primask = __get_PRIMASK();
  __disable_irq();
  memcpy(setup, usb->_setup, 8);
  usb->_setup_pending = false;
  __set_PRIMASK(primask);
  return USB_Event_Setup;
}

//------------------------------------------------------------------------------------------ Status

// Bit `7` marks the address as pending, so address zero gets applied like any other
void USB_SetAddress(USB_t *usb, uint8_t address) { usb->_address = 0x80u | address; }
bool USB_IsBusy(USB_t *usb, uint8_t ep) { return ep < USB_EP_LIMIT && usb->ep[ep]._tx_busy; }
bool USB_IsFree(USB_t *usb, uint8_t ep) { return !USB_IsBusy(usb, ep); }

//-------------------------------------------------------------------------------------------------
#endif

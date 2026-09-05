// hal/stm32wb/itf/usb_wb.c

#include "usb.h"

#include "gpio.h"

//----------------------------------------------------------------------------------- Packet memory

// Buffer table entry halfwords: `ADDR_TX`, `COUNT_TX`, `ADDR_RX`, `COUNT_RX`
#define PMA_ADDR_TX(ep)  (8 * (ep))
#define PMA_COUNT_TX(ep) (8 * (ep) + 2)
#define PMA_ADDR_RX(ep)  (8 * (ep) + 4)
#define PMA_COUNT_RX(ep) (8 * (ep) + 6)

#define PMA_COUNT_MASK 0x03FFu

static inline volatile uint16_t *pma(uint16_t offset) {
  return (volatile uint16_t *)(USB1_PMAADDR + offset);
}

static void pma_write(uint16_t offset, const uint8_t *src, uint16_t len)
{
  volatile uint16_t *dst = pma(offset);
  while(len >= 2) {
    *dst++ = (uint16_t)src[0] | ((uint16_t)src[1] << 8);
    src += 2;
    len -= 2;
  }
  if(len) *dst = src[0];
}

static void pma_read(uint16_t offset, uint8_t *dst, uint16_t len)
{
  volatile uint16_t *src = pma(offset);
  while(len >= 2) {
    uint16_t word = *src++;
    *dst++ = (uint8_t)word;
    *dst++ = (uint8_t)(word >> 8);
    len -= 2;
  }
  if(len) *dst = (uint8_t)*src;
}

// `COUNT_RX` block field: 2-byte blocks up to 62, 32-byte blocks above
static uint16_t pma_rx_blocks(uint16_t size)
{
  if(size <= 62) return (uint16_t)((size / 2) << 10);
  return (uint16_t)(0x8000u | ((size / 32 - 1) << 10));
}

//------------------------------------------------------------------------------ Endpoint registers

// `EPnR` mixes plain bits, toggle-on-write status fields and clear-on-write-0 flags.
// Every write goes through here: flags are written `1` to keep them,
// toggles are written with the change wanted, plain bits are written back as read.
static inline volatile uint16_t *epr(uint8_t ep) { return &USB->EP0R + 2 * ep; }

static void ep_toggle(uint8_t ep, uint16_t field, uint16_t value)
{
  volatile uint16_t *reg = epr(ep);
  *reg = ((*reg & (USB_EPREG_MASK | field)) ^ value) | USB_EP_CTR_RX | USB_EP_CTR_TX;
}

static inline void ep_rx_status(uint8_t ep, uint16_t status) {
  ep_toggle(ep, USB_EPRX_STAT, status);
}

static inline void ep_tx_status(uint8_t ep, uint16_t status) {
  ep_toggle(ep, USB_EPTX_STAT, status);
}

static inline void ep_clear_ctr_rx(uint8_t ep) {
  *epr(ep) = (*epr(ep) & USB_EPREG_MASK & ~USB_EP_CTR_RX) | USB_EP_CTR_TX;
}

static inline void ep_clear_ctr_tx(uint8_t ep) {
  *epr(ep) = (*epr(ep) & USB_EPREG_MASK & ~USB_EP_CTR_TX) | USB_EP_CTR_RX;
}

// Data toggle bits flip on a write of `1`, so a set bit is written back to clear it.
static void ep_clear_dtog(uint8_t ep, uint16_t dtog)
{
  if(*epr(ep) & dtog) ep_toggle(ep, 0, dtog);
}

static uint16_t ep_type_bits(USB_EpType_t type)
{
  switch(type) {
    case USB_EpType_Control: return USB_EP_CONTROL;
    case USB_EpType_Interrupt: return USB_EP_INTERRUPT;
    default: return USB_EP_BULK;
  }
}

static void ep_activate(USB_t *usb, uint8_t ep)
{
  USB_Ep_t *endpoint = &usb->ep[ep];
  *epr(ep) = ep_type_bits(endpoint->type) | ep | USB_EP_CTR_RX | USB_EP_CTR_TX;
  *pma(PMA_ADDR_TX(ep)) = endpoint->_pma_tx;
  *pma(PMA_COUNT_TX(ep)) = 0;
  *pma(PMA_ADDR_RX(ep)) = endpoint->_pma_rx;
  *pma(PMA_COUNT_RX(ep)) = pma_rx_blocks(endpoint->size);
  endpoint->_tx_busy = false;
  ep_clear_dtog(ep, USB_EP_DTOG_TX);
  ep_clear_dtog(ep, USB_EP_DTOG_RX);
  ep_tx_status(ep, endpoint->tx ? USB_EP_TX_NAK : 0);
  // Control endpoint receives SETUP and status packets even without a stream.
  ep_rx_status(ep, (endpoint->rx || !ep) ? USB_EP_RX_VALID : 0);
}

static void ep_deactivate(USB_t *usb, uint8_t ep)
{
  ep_tx_status(ep, 0);
  ep_rx_status(ep, 0);
  usb->ep[ep]._tx_busy = false;
}

//--------------------------------------------------------------------------------------- Interrupt

static void bus_reset(USB_t *usb)
{
  for(uint8_t ep = 1; ep < USB_EP_LIMIT; ep++) ep_deactivate(usb, ep);
  usb->_address = 0;
  USB->DADDR = USB_DADDR_EF;
  ep_activate(usb, 0);
  usb->_event_head = 0;
  usb->_event_tail = 0;
  usb->_setup_pending = false;
  usb->resets++;
  USB_EventPush(usb, USB_Event_Reset);
}

static void tx_load(USB_t *usb, uint8_t ep)
{
  USB_Ep_t *endpoint = &usb->ep[ep];
  uint16_t left = endpoint->_tx_len - endpoint->_tx_pos;
  uint16_t len = left < endpoint->size ? left : endpoint->size;
  if(!len) endpoint->_tx_zlp = false; // the closing zero-length packet
  else pma_write(endpoint->_pma_tx, endpoint->_tx_data + endpoint->_tx_pos, len);
  *pma(PMA_COUNT_TX(ep)) = len;
  endpoint->_tx_pos += len;
  ep_tx_status(ep, USB_EP_TX_VALID);
}

static void tx_done(USB_t *usb, uint8_t ep)
{
  USB_Ep_t *endpoint = &usb->ep[ep];
  ep_clear_ctr_tx(ep);
  // Status stage of `SET_ADDRESS` went out under the old address.
  if(!ep && usb->_address) {
    USB->DADDR = USB_DADDR_EF | (usb->_address & 0x7Fu);
    usb->_address = 0;
  }
  if(endpoint->_tx_pos < endpoint->_tx_len || endpoint->_tx_zlp) tx_load(usb, ep);
  else endpoint->_tx_busy = false;
}

static void rx_done(USB_t *usb, uint8_t ep, bool setup)
{
  USB_Ep_t *endpoint = &usb->ep[ep];
  uint16_t count = *pma(PMA_COUNT_RX(ep)) & PMA_COUNT_MASK;
  ep_clear_ctr_rx(ep);
  if(setup) {
    pma_read(endpoint->_pma_rx, usb->_setup, 8);
    // A new SETUP abandons whatever the previous one left in flight.
    ep_tx_status(ep, USB_EP_TX_NAK);
    endpoint->_tx_busy = false;
    usb->_address = 0; // an unapplied address dies with its request
    usb->_setup_pending = true;
    USB_EventPush(usb, USB_Event_Setup);
  }
  else if(endpoint->rx) {
    uint8_t packet[64];
    pma_read(endpoint->_pma_rx, packet, count);
    for(uint16_t i = 0; i < count; i++) {
      // Console mode drops escape sequences on purpose, only a full ring is an overflow.
      if(!BUFF_Push(endpoint->rx, packet[i]) && !endpoint->rx->console_mode) usb->overflow++;
    }
    if(count < endpoint->size) BUFF_Break(endpoint->rx); // end of transfer on a short packet
  }
  ep_rx_status(ep, USB_EP_RX_VALID);
}

static void transfer(USB_t *usb, uint8_t ep)
{
  uint16_t reg = *epr(ep);
  if(ep >= USB_EP_LIMIT) {
    // Endpoint outside the map never got a buffer, only its flags are cleared.
    *epr(ep) = reg & USB_EPREG_MASK & ~(USB_EP_CTR_RX | USB_EP_CTR_TX);
    usb->errors++;
    return;
  }
  if(reg & USB_EP_CTR_RX) rx_done(usb, ep, reg & USB_EP_SETUP);
  if(reg & USB_EP_CTR_TX) tx_done(usb, ep);
}

static void irq_handler(USB_t *usb)
{
  const uint16_t handled = USB_ISTR_CTR | USB_ISTR_RESET | USB_ISTR_SUSP | USB_ISTR_WKUP |
    USB_ISTR_ERR | USB_ISTR_PMAOVR;
  uint16_t istr;
  // `ISTR` flags clear on a write of `0`, a write of `1` leaves them.
  while((istr = USB->ISTR) & handled) {
    if(istr & USB_ISTR_RESET) {
      USB->ISTR = (uint16_t)~USB_ISTR_RESET;
      bus_reset(usb);
    }
    else if(istr & USB_ISTR_CTR) {
      transfer(usb, istr & USB_ISTR_EP_ID); // cleared together with the endpoint flags
    }
    else if(istr & USB_ISTR_WKUP) {
      USB->CNTR &= ~USB_CNTR_FSUSP;
      USB->ISTR = (uint16_t)~USB_ISTR_WKUP;
      USB_EventPush(usb, USB_Event_Resume);
    }
    else if(istr & USB_ISTR_SUSP) {
      USB->ISTR = (uint16_t)~USB_ISTR_SUSP;
      USB->CNTR |= USB_CNTR_FSUSP;
      USB_EventPush(usb, USB_Event_Suspend);
    }
    else {
      USB->ISTR = (uint16_t)~(USB_ISTR_ERR | USB_ISTR_PMAOVR);
      usb->errors++;
    }
  }
}

//-------------------------------------------------------------------------------------------- Init

// `PA11` and `PA12` on alternate function 10
static const GPIO_Map_t USB_DM_MAP = { .port = GPIOA, .pin = 11, .alternate = 10 };
static const GPIO_Map_t USB_DP_MAP = { .port = GPIOA, .pin = 12, .alternate = 10 };

// Packet memory map: buffer table first, then one IN and one OUT buffer per used endpoint
static status_t pma_map(USB_t *usb)
{
  uint16_t offset = USB_PMA_TABLE;
  for(uint8_t ep = 0; ep < USB_EP_LIMIT; ep++) {
    USB_Ep_t *endpoint = &usb->ep[ep];
    bool tx = endpoint->tx || !ep;
    bool rx = endpoint->rx || !ep;
    if(!tx && !rx) continue;
    if(endpoint->size < 8 || endpoint->size > 64 || (endpoint->size & 1)) return ERR;
    if(tx) {
      endpoint->_pma_tx = offset;
      offset += endpoint->size;
    }
    if(rx) {
      endpoint->_pma_rx = offset;
      offset += endpoint->size;
    }
    if(offset > USB_PMA_SIZE) return ERR;
  }
  return OK;
}

status_t USB_Init(USB_t *usb)
{
  if(pma_map(usb)) return ERR;
  usb->_event_head = 0;
  usb->_event_tail = 0;
  usb->_setup_pending = false;
  usb->_address = 0;
  RCC_EnableUSB();
  GPIO_InitAlternate(&USB_DM_MAP, false);
  GPIO_InitAlternate(&USB_DP_MAP, false);
  // Transceiver power-up takes 1us before reset can be released.
  USB->CNTR = USB_CNTR_FRES;
  for(volatile uint32_t i = 0; i < 128; i++);
  USB->CNTR = 0;
  USB->ISTR = 0;
  USB->BTABLE = 0;
  USB->DADDR = USB_DADDR_EF;
  USB->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_WKUPM |
    USB_CNTR_ERRM | USB_CNTR_PMAOVRM;
  IRQ_EnableUSB(usb->irq_priority, (IRQ_Handler_t)irq_handler, usb);
  usb->_init = true;
  USB->BCDR |= USB_BCDR_DPPU; // attach: `DP` pull-up
  return OK;
}

//--------------------------------------------------------------------------------------- Endpoints

// Thread-side endpoint register writes are fenced from the interrupt:
// a read-modify-write split by the handler would toggle status fields against stale bits.
void USB_EpEnable(USB_t *usb, uint8_t ep)
{
  if(ep >= USB_EP_LIMIT) return;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  ep_activate(usb, ep);
  __set_PRIMASK(primask);
}

void USB_EpDisable(USB_t *usb, uint8_t ep)
{
  if(ep >= USB_EP_LIMIT) return;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  ep_deactivate(usb, ep);
  __set_PRIMASK(primask);
}

void USB_EpStall(USB_t *usb, uint8_t ep, bool in)
{
  if(ep >= USB_EP_LIMIT) return;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if(in || !ep) ep_tx_status(ep, USB_EP_TX_STALL);
  if(!in || !ep) ep_rx_status(ep, USB_EP_RX_STALL);
  usb->ep[ep]._tx_busy = false;
  __set_PRIMASK(primask);
}

void USB_EpClear(USB_t *usb, uint8_t ep, bool in)
{
  if(ep >= USB_EP_LIMIT) return;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if(in) {
    ep_clear_dtog(ep, USB_EP_DTOG_TX);
    ep_tx_status(ep, USB_EP_TX_NAK);
    usb->ep[ep]._tx_busy = false;
  }
  else {
    ep_clear_dtog(ep, USB_EP_DTOG_RX);
    ep_rx_status(ep, USB_EP_RX_VALID);
  }
  __set_PRIMASK(primask);
}

bool USB_EpStalled(USB_t *usb, uint8_t ep, bool in)
{
  unused(usb);
  if(ep >= USB_EP_LIMIT) return false;
  uint16_t reg = *epr(ep);
  if(in) return (reg & USB_EPTX_STAT) == USB_EP_TX_STALL;
  return (reg & USB_EPRX_STAT) == USB_EP_RX_STALL;
}

//-------------------------------------------------------------------------------------------- Send

status_t USB_Send(USB_t *usb, uint8_t ep, const uint8_t *data, uint16_t len, bool zlp)
{
  if(!usb->_init || ep >= USB_EP_LIMIT || !(usb->ep[ep].tx || !ep)) return ERR;
  USB_Ep_t *endpoint = &usb->ep[ep];
  // Checks and state live in one critical section: a bus reset in between would
  // deactivate the endpoint and `tx_load` would arm it right back
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  status_t ret = OK;
  uint16_t stat = *epr(ep) & USB_EPTX_STAT;
  if(endpoint->_tx_busy) ret = BUSY;
  else if(stat == USB_EP_TX_STALL || stat == USB_EP_TX_DIS) ret = ERR; // halted or inactive
  else {
    endpoint->_tx_data = data;
    endpoint->_tx_len = len;
    endpoint->_tx_pos = 0;
    // An empty transfer is one zero-length packet, a full-packet tail gets one when asked.
    endpoint->_tx_zlp = !len || (zlp && !(len % endpoint->size));
    endpoint->_tx_busy = true;
    tx_load(usb, ep);
  }
  __set_PRIMASK(primask);
  return ret;
}

//-------------------------------------------------------------------------------------------------

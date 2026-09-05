// lib/usb/usbd.c

#include "usbd.h"

#if(USB_PRESENT)

#include <string.h>
#include "vrts.h"

_Static_assert(USBD_EP_BULK > 0 && USBD_EP_BULK < USB_EP_LIMIT,
  "USB_EP_LIMIT must cover USBD_EP_BULK");
_Static_assert(USBD_EP_NOTIFY > 0 && USBD_EP_NOTIFY < USB_EP_LIMIT &&
  USBD_EP_NOTIFY != USBD_EP_BULK, "USB_EP_LIMIT must cover USBD_EP_NOTIFY");

//--------------------------------------------------------------------------------------- Constants

#define USB_DESC_DEVICE        1
#define USB_DESC_CONFIG        2
#define USB_DESC_STRING        3
#define USB_DESC_INTERFACE     4
#define USB_DESC_ENDPOINT      5
#define USB_DESC_BOS           15
#define USB_DESC_CAPABILITY    16
#define USB_DESC_CS_INTERFACE  0x24

#define USB_REQ_GET_STATUS         0
#define USB_REQ_CLEAR_FEATURE      1
#define USB_REQ_SET_FEATURE        3
#define USB_REQ_SET_ADDRESS        5
#define USB_REQ_GET_DESCRIPTOR     6
#define USB_REQ_GET_CONFIGURATION  8
#define USB_REQ_SET_CONFIGURATION  9
#define USB_REQ_GET_INTERFACE      10
#define USB_REQ_SET_INTERFACE      11

#define USB_FEATURE_ENDPOINT_HALT  0
#define USB_FEATURE_REMOTE_WAKEUP  1

// `bmRequestType` fields
#define USB_DIR_IN               0x80
#define USB_TYPE_MASK            0x60
#define USB_TYPE_STANDARD        0x00
#define USB_TYPE_CLASS           0x20
#define USB_TYPE_VENDOR          0x40
#define USB_RECIPIENT_MASK       0x1F
#define USB_RECIPIENT_DEVICE     0
#define USB_RECIPIENT_INTERFACE  1
#define USB_RECIPIENT_ENDPOINT   2

// Vendor request codes announced in the BOS platform capabilities
#define USBD_VENDOR_WEBUSB       0x01
#define USBD_VENDOR_MSOS         0x02
#define WEBUSB_GET_URL           0x02
#define WEBUSB_LANDING_PAGE      1
#define MSOS_DESCRIPTOR_INDEX    0x07
#define MSOS_WINDOWS_VERSION     0x06030000  // Windows 8.1, first with MS OS 2.0

// CDC ACM class requests
#define CDC_SET_LINE_CODING         0x20
#define CDC_GET_LINE_CODING         0x21
#define CDC_SET_CONTROL_LINE_STATE  0x22
#define CDC_LINE_STATE_DTR          0x01

typedef struct {
  uint8_t type;
  uint8_t request;
  uint16_t value;
  uint16_t index;
  uint16_t length;
} setup_t;

static USBD_t *self;

static inline bool cdc(USBD_t *usbd) { return usbd->mode == USBD_Mode_CDC; }
static inline bool bos(USBD_t *usbd) { return !cdc(usbd) && (usbd->guid || usbd->url); }

//------------------------------------------------------------------------------------- Descriptors

static uint8_t *put16(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)value;
  dst[1] = (uint8_t)(value >> 8);
  return dst + 2;
}

static uint8_t *put32(uint8_t *dst, uint32_t value)
{
  dst = put16(dst, (uint16_t)value);
  return put16(dst, (uint16_t)(value >> 16));
}

static uint8_t *put_bytes(uint8_t *dst, const uint8_t *src, uint8_t len)
{
  memcpy(dst, src, len);
  return dst + len;
}

// ASCII to UTF-16LE, cut to what the scratch buffer holds behind `dst`
static uint8_t *put_utf16(USBD_t *usbd, uint8_t *dst, const char *str)
{
  const uint8_t *end = usbd->_desc + USBD_DESC_SIZE - 1;
  while(*str && dst < end) {
    *dst++ = (uint8_t)*str++;
    *dst++ = 0;
  }
  return dst;
}

static uint8_t *put_serial(USBD_t *usbd, uint8_t *dst)
{
  if(usbd->serial) return put_utf16(usbd, dst, usbd->serial);
  #ifdef UID_BASE
    const uint8_t *uid = (const uint8_t *)UID_BASE;
    static const char hex[] = "0123456789ABCDEF";
    for(uint8_t i = 0; i < 12; i++) {
      *dst++ = (uint8_t)hex[uid[i] >> 4];
      *dst++ = 0;
      *dst++ = (uint8_t)hex[uid[i] & 0x0F];
      *dst++ = 0;
    }
    return dst;
  #else
    return put_utf16(usbd, dst, "0");
  #endif
}

static uint8_t *desc_interface(uint8_t *dst, uint8_t number, uint8_t endpoints, uint8_t class,
  uint8_t subclass, uint8_t protocol)
{
  *dst++ = 9;
  *dst++ = USB_DESC_INTERFACE;
  *dst++ = number;
  *dst++ = 0; // alternate setting
  *dst++ = endpoints;
  *dst++ = class;
  *dst++ = subclass;
  *dst++ = protocol;
  *dst++ = 0; // no string
  return dst;
}

static uint8_t *desc_endpoint(uint8_t *dst, uint8_t address, uint8_t attributes, uint16_t size,
  uint8_t interval)
{
  *dst++ = 7;
  *dst++ = USB_DESC_ENDPOINT;
  *dst++ = address;
  *dst++ = attributes;
  dst = put16(dst, size);
  *dst++ = interval;
  return dst;
}

static uint16_t desc_device(USBD_t *usbd)
{
  uint8_t *dst = usbd->_desc;
  *dst++ = 18;
  *dst++ = USB_DESC_DEVICE;
  dst = put16(dst, bos(usbd) ? 0x0201 : 0x0200); // `bcdUSB`: 2.1 with BOS, 2.0 without
  *dst++ = cdc(usbd) ? 0x02 : 0; // CDC at device level, vendor class from interface
  *dst++ = 0;
  *dst++ = 0;
  *dst++ = 64; // `EP0` packet size
  dst = put16(dst, usbd->vid);
  dst = put16(dst, usbd->pid);
  dst = put16(dst, usbd->version);
  *dst++ = usbd->manufacturer ? 1 : 0;
  *dst++ = usbd->product ? 2 : 0;
  *dst++ = 3;
  *dst++ = 1; // configurations
  return 18;
}

static uint16_t desc_config(USBD_t *usbd)
{
  uint8_t *dst = usbd->_desc;
  uint8_t attributes = 0x80;
  if(usbd->self_powered) attributes |= 0x40;
  if(usbd->remote_wakeup) attributes |= 0x20;
  *dst++ = 9;
  *dst++ = USB_DESC_CONFIG;
  dst += 2; // total length, known at the end
  *dst++ = cdc(usbd) ? 2 : 1; // interfaces
  *dst++ = 1; // configuration value
  *dst++ = 0; // no string
  *dst++ = attributes;
  *dst++ = (uint8_t)(usbd->max_power_mA / 2); // `bMaxPower` in 2mA units
  if(cdc(usbd)) {
    // Interface 0: communication class, abstract control model, no protocol
    dst = desc_interface(dst, 0, 1, 0x02, 0x02, 0x00);
    static const uint8_t functional[] = {
      5, USB_DESC_CS_INTERFACE, 0x00, 0x10, 0x01, // header, CDC 1.10
      5, USB_DESC_CS_INTERFACE, 0x01, 0x00, 0x01, // call management: none, data interface 1
      4, USB_DESC_CS_INTERFACE, 0x02, 0x02,       // ACM: line coding and control line state
      5, USB_DESC_CS_INTERFACE, 0x06, 0x00, 0x01  // union: master 0, slave 1
    };
    dst = put_bytes(dst, functional, sizeof(functional));
    dst = desc_endpoint(dst, USBD_EP_NOTIFY | USB_DIR_IN, 3, 8, 16); // interrupt, 16ms
    // Interface 1: data class, the bulk pair
    dst = desc_interface(dst, 1, 2, 0x0A, 0x00, 0x00);
  }
  else dst = desc_interface(dst, 0, 2, 0xFF, 0x00, 0x00); // vendor class
  dst = desc_endpoint(dst, USBD_EP_BULK, 2, USBD_PACKET_SIZE, 0);
  dst = desc_endpoint(dst, USBD_EP_BULK | USB_DIR_IN, 2, USBD_PACKET_SIZE, 0);
  uint16_t len = (uint16_t)(dst - usbd->_desc);
  put16(&usbd->_desc[2], len);
  return len;
}

static uint16_t desc_string(USBD_t *usbd, uint8_t index)
{
  uint8_t *dst = usbd->_desc + 2;
  if(!index) dst = put16(dst, 0x0409); // English (US)
  else if(index == 1 && usbd->manufacturer) dst = put_utf16(usbd, dst, usbd->manufacturer);
  else if(index == 2 && usbd->product) dst = put_utf16(usbd, dst, usbd->product);
  else if(index == 3) dst = put_serial(usbd, dst);
  else return 0;
  uint16_t len = (uint16_t)(dst - usbd->_desc);
  usbd->_desc[0] = (uint8_t)len;
  usbd->_desc[1] = USB_DESC_STRING;
  return len;
}

// Registry value: GUID as UTF-16 with its own terminator and the `REG_MULTI_SZ` one
static uint16_t msos_data_size(USBD_t *usbd) {
  return (uint16_t)(2 * (strlen(usbd->guid) + 2));
}

// Set header, compatible ID and registry property
static uint16_t msos_size(USBD_t *usbd) {
  return (uint16_t)(10 + 20 + 10 + 42 + msos_data_size(usbd));
}

static uint16_t desc_bos(USBD_t *usbd)
{
  uint8_t *dst = usbd->_desc + 5;
  uint8_t capabilities = 0;
  if(usbd->url) {
    // WebUSB platform capability `3408B638-09A9-47A0-8BFD-A0768815B665`
    static const uint8_t uuid[16] = {
      0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
      0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65
    };
    *dst++ = 24;
    *dst++ = USB_DESC_CAPABILITY;
    *dst++ = 5; // platform capability
    *dst++ = 0;
    dst = put_bytes(dst, uuid, 16);
    dst = put16(dst, 0x0100); // WebUSB 1.0
    *dst++ = USBD_VENDOR_WEBUSB;
    *dst++ = WEBUSB_LANDING_PAGE;
    capabilities++;
  }
  if(usbd->guid) {
    // MS OS 2.0 platform capability `D8DD60DF-4589-4CC7-9CD2-659D9E648A9F`
    static const uint8_t uuid[16] = {
      0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
      0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F
    };
    *dst++ = 28;
    *dst++ = USB_DESC_CAPABILITY;
    *dst++ = 5;
    *dst++ = 0;
    dst = put_bytes(dst, uuid, 16);
    dst = put32(dst, MSOS_WINDOWS_VERSION);
    dst = put16(dst, msos_size(usbd));
    *dst++ = USBD_VENDOR_MSOS;
    *dst++ = 0; // no alternate enumeration
    capabilities++;
  }
  uint16_t len = (uint16_t)(dst - usbd->_desc);
  usbd->_desc[0] = 5;
  usbd->_desc[1] = USB_DESC_BOS;
  put16(&usbd->_desc[2], len);
  usbd->_desc[4] = capabilities;
  return len;
}

static uint16_t desc_msos(USBD_t *usbd)
{
  uint16_t total = msos_size(usbd);
  if(total > USBD_DESC_SIZE) return 0;
  uint8_t *dst = usbd->_desc;
  // Set header
  dst = put16(dst, 10);
  dst = put16(dst, 0);
  dst = put32(dst, MSOS_WINDOWS_VERSION);
  dst = put16(dst, total);
  // Compatible ID: WinUSB, no sub-compatible ID
  dst = put16(dst, 20);
  dst = put16(dst, 3);
  dst = put_bytes(dst, (const uint8_t *)"WINUSB\0\0", 8);
  memset(dst, 0, 8);
  dst += 8;
  // Registry property `DeviceInterfaceGUIDs` as `REG_MULTI_SZ`
  dst = put16(dst, (uint16_t)(10 + 42 + msos_data_size(usbd)));
  dst = put16(dst, 4);
  dst = put16(dst, 7);
  dst = put16(dst, 42);
  dst = put_utf16(usbd, dst, "DeviceInterfaceGUIDs");
  dst = put16(dst, 0);
  dst = put16(dst, msos_data_size(usbd));
  dst = put_utf16(usbd, dst, usbd->guid);
  dst = put16(dst, 0);
  dst = put16(dst, 0);
  return total;
}

static uint16_t desc_url(USBD_t *usbd)
{
  size_t n = strlen(usbd->url);
  if(n + 3 > USBD_DESC_SIZE) return 0;
  usbd->_desc[0] = (uint8_t)(n + 3);
  usbd->_desc[1] = 3; // URL descriptor
  usbd->_desc[2] = usbd->url_https ? 1 : 0;
  memcpy(&usbd->_desc[3], usbd->url, n);
  return (uint16_t)(n + 3);
}

//---------------------------------------------------------------------------------------- Requests

static void set_config(USBD_t *usbd, uint8_t value)
{
  if(usbd->_config) {
    USB_EpDisable(&usbd->_usb, USBD_EP_BULK);
    if(cdc(usbd)) USB_EpDisable(&usbd->_usb, USBD_EP_NOTIFY);
  }
  usbd->_config = value;
  usbd->_dtr = false;
  // A session ends here and its bytes end with it.
  BUFF_Clear(usbd->buff);
  if(value) {
    USB_EpEnable(&usbd->_usb, USBD_EP_BULK);
    if(cdc(usbd)) USB_EpEnable(&usbd->_usb, USBD_EP_NOTIFY);
  }
}

// Answer lands in `_desc` with its length in `len`, `false` stalls the request.
// `true` when the active configuration owns this endpoint direction; acting on any
// other would arm hardware with no buffer behind it
static bool ep_exists(USBD_t *usbd, uint8_t ep, bool in)
{
  if(ep >= USB_EP_LIMIT) return false;
  if(ep && !usbd->_config) return false; // past EP0 endpoints exist only configured
  return in ? usbd->_usb.ep[ep].tx : usbd->_usb.ep[ep].rx != NULL;
}

static bool request_standard(USBD_t *usbd, const setup_t *req, uint16_t *len)
{
  uint8_t recipient = req->type & USB_RECIPIENT_MASK;
  uint8_t interfaces = cdc(usbd) ? 2 : 1;
  switch(req->request) {
    case USB_REQ_GET_DESCRIPTOR: {
      if(recipient != USB_RECIPIENT_DEVICE) return false;
      switch(req->value >> 8) {
        case USB_DESC_DEVICE: *len = desc_device(usbd); break;
        case USB_DESC_CONFIG: *len = desc_config(usbd); break;
        case USB_DESC_STRING: *len = desc_string(usbd, (uint8_t)req->value); break;
        case USB_DESC_BOS: *len = bos(usbd) ? desc_bos(usbd) : 0; break;
        default: return false;
      }
      return *len != 0;
    }
    case USB_REQ_SET_ADDRESS:
      USB_SetAddress(&usbd->_usb, (uint8_t)req->value);
      return true;
    case USB_REQ_SET_CONFIGURATION:
      if(req->value > 1) return false;
      set_config(usbd, (uint8_t)req->value);
      return true;
    case USB_REQ_GET_CONFIGURATION:
      usbd->_desc[0] = usbd->_config;
      *len = 1;
      return true;
    case USB_REQ_GET_STATUS:
      usbd->_desc[0] = 0;
      usbd->_desc[1] = 0;
      if(recipient == USB_RECIPIENT_DEVICE) {
        usbd->_desc[0] = (usbd->self_powered ? 0x01 : 0) | (usbd->_wakeup ? 0x02 : 0);
      }
      else if(recipient == USB_RECIPIENT_ENDPOINT) {
        if(!ep_exists(usbd, req->index & 0x0F, req->index & USB_DIR_IN)) return false;
        usbd->_desc[0] = USB_EpStalled(&usbd->_usb, req->index & 0x0F,
          req->index & USB_DIR_IN);
      }
      else if(recipient != USB_RECIPIENT_INTERFACE) return false;
      *len = 2;
      return true;
    case USB_REQ_CLEAR_FEATURE:
    case USB_REQ_SET_FEATURE: {
      bool set = req->request == USB_REQ_SET_FEATURE;
      if(recipient == USB_RECIPIENT_ENDPOINT && req->value == USB_FEATURE_ENDPOINT_HALT) {
        uint8_t ep = req->index & 0x0F;
        if(!ep || !ep_exists(usbd, ep, req->index & USB_DIR_IN)) return false;
        if(set) USB_EpStall(&usbd->_usb, ep, req->index & USB_DIR_IN);
        else USB_EpClear(&usbd->_usb, ep, req->index & USB_DIR_IN);
        return true;
      }
      if(recipient == USB_RECIPIENT_DEVICE && req->value == USB_FEATURE_REMOTE_WAKEUP) {
        if(!usbd->remote_wakeup) return false;
        usbd->_wakeup = set;
        return true;
      }
      return false;
    }
    case USB_REQ_GET_INTERFACE:
      if(!usbd->_config || req->index >= interfaces) return false;
      usbd->_desc[0] = 0;
      *len = 1;
      return true;
    case USB_REQ_SET_INTERFACE:
      return usbd->_config && !req->value && req->index < interfaces;
    default:
      return false;
  }
}

static bool request_vendor(USBD_t *usbd, const setup_t *req, uint16_t *len)
{
  if(req->type != (USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIPIENT_DEVICE)) return false;
  if(req->request == USBD_VENDOR_WEBUSB && bos(usbd) && usbd->url) {
    if(req->index != WEBUSB_GET_URL || req->value != WEBUSB_LANDING_PAGE) return false;
    *len = desc_url(usbd);
  }
  else if(req->request == USBD_VENDOR_MSOS && bos(usbd) && usbd->guid) {
    if(req->index != MSOS_DESCRIPTOR_INDEX) return false;
    *len = desc_msos(usbd);
  }
  else return false;
  return *len != 0;
}

// CDC requests address the communication interface. The one carrying data is only accepted
// here: its bytes arrive on `EP0` afterwards and `request_data` finishes it.
static bool request_class(USBD_t *usbd, const setup_t *req, uint16_t *len)
{
  if(!cdc(usbd) || (req->type & USB_RECIPIENT_MASK) != USB_RECIPIENT_INTERFACE) return false;
  if(req->index != 0) return false;
  switch(req->request) {
    case CDC_SET_LINE_CODING:
      return req->length == sizeof(usbd->_line_coding);
    case CDC_GET_LINE_CODING:
      memcpy(usbd->_desc, usbd->_line_coding, sizeof(usbd->_line_coding));
      *len = sizeof(usbd->_line_coding);
      return true;
    case CDC_SET_CONTROL_LINE_STATE:
      usbd->_dtr = req->value & CDC_LINE_STATE_DTR;
      return true;
    default:
      return false;
  }
}

static void request_data(USBD_t *usbd)
{
  uint8_t data[USBD_CTRL_SIZE];
  uint16_t len = BUFF_Read(&usbd->_ctrl, data);
  bool ok = len == usbd->_out_length;
  if(ok && usbd->_out_request == CDC_SET_LINE_CODING) memcpy(usbd->_line_coding, data, len);
  usbd->_out_request = 0;
  if(ok) USB_Send(&usbd->_usb, 0, NULL, 0, false); // status stage
  else USB_EpStall(&usbd->_usb, 0, true);
}

static void request(USBD_t *usbd, const uint8_t *setup)
{
  setup_t req = {
    .type = setup[0],
    .request = setup[1],
    .value = (uint16_t)(setup[2] | (setup[3] << 8)),
    .index = (uint16_t)(setup[4] | (setup[5] << 8)),
    .length = (uint16_t)(setup[6] | (setup[7] << 8))
  };
  uint8_t type = req.type & USB_TYPE_MASK;
  uint16_t len = 0;
  bool ok;
  // Whatever a previous request left unfinished is gone with the new SETUP.
  usbd->_out_request = 0;
  BUFF_Clear(&usbd->_ctrl);
  switch(type) {
    case USB_TYPE_STANDARD: ok = request_standard(usbd, &req, &len); break;
    case USB_TYPE_CLASS: ok = request_class(usbd, &req, &len); break;
    case USB_TYPE_VENDOR: ok = request_vendor(usbd, &req, &len); break;
    default: ok = false;
  }
  bool out_data = !(req.type & USB_DIR_IN) && req.length;
  // Only a class request may carry data to the device, and no more than one packet.
  if(out_data && (type != USB_TYPE_CLASS || req.length > USBD_CTRL_SIZE)) ok = false;
  if(!ok) {
    USB_EpStall(&usbd->_usb, 0, true);
    return;
  }
  if(out_data) {
    usbd->_out_request = req.request;
    usbd->_out_length = req.length;
  }
  else if(req.type & USB_DIR_IN) {
    if(len > req.length) len = req.length;
    // An answer shorter than asked and ending on a packet boundary needs a zero-length packet.
    USB_Send(&usbd->_usb, 0, usbd->_desc, len, len < req.length);
  }
  else USB_Send(&usbd->_usb, 0, NULL, 0, false); // status stage
}

//-------------------------------------------------------------------------------------------- Init

status_t USBD_Init(USBD_t *usbd)
{
  if(!usbd->buff) return ERR;
  USB_t *usb = &usbd->_usb;
  usb->irq_priority = usbd->irq_priority;
  usbd->_ctrl.memory = usbd->_ctrl_memory;
  usbd->_ctrl.size = USBD_CTRL_SIZE;
  BUFF_Init(&usbd->_ctrl);
  usb->ep[0].type = USB_EpType_Control;
  usb->ep[0].size = 64;
  usb->ep[0].rx = &usbd->_ctrl;
  usb->ep[0].tx = true;
  usb->ep[USBD_EP_BULK].type = USB_EpType_Bulk;
  usb->ep[USBD_EP_BULK].size = USBD_PACKET_SIZE;
  usb->ep[USBD_EP_BULK].rx = usbd->buff;
  usb->ep[USBD_EP_BULK].tx = true;
  if(cdc(usbd)) {
    usb->ep[USBD_EP_NOTIFY].type = USB_EpType_Interrupt;
    usb->ep[USBD_EP_NOTIFY].size = 8;
    usb->ep[USBD_EP_NOTIFY].rx = NULL;
    usb->ep[USBD_EP_NOTIFY].tx = true;
  }
  BUFF_Init(usbd->buff);
  static const uint8_t line_coding[7] = { 0x00, 0xC2, 0x01, 0x00, 0, 0, 8 }; // 115200 8N1
  memcpy(usbd->_line_coding, line_coding, sizeof(line_coding));
  usbd->_out_request = 0;
  usbd->_config = 0;
  usbd->_suspended = false;
  usbd->_wakeup = false;
  usbd->_dtr = false;
  if(USB_Init(usb)) return ERR;
  self = usbd;
  usbd->_init = true;
  return OK;
}

//-------------------------------------------------------------------------------------------- Loop

bool USBD_Step(USBD_t *usbd)
{
  uint8_t setup[8];
  switch(USB_Event(&usbd->_usb, setup)) {
    case USB_Event_None:
      if(!usbd->_out_request || !BUFF_Size(&usbd->_ctrl)) return false;
      request_data(usbd);
      break;
    case USB_Event_Reset:
      set_config(usbd, 0);
      usbd->_out_request = 0;
      usbd->_suspended = false;
      usbd->_wakeup = false;
      break;
    case USB_Event_Suspend: usbd->_suspended = true; break;
    case USB_Event_Resume: usbd->_suspended = false; break;
    case USB_Event_Setup: request(usbd, setup); break;
  }
  return true;
}

void USBD_Loop(void)
{
  while(1) {
    while(USBD_Step(self));
    let();
  }
}

//-------------------------------------------------------------------------------------------- Link

bool USBD_IsReady(USBD_t *usbd) { return usbd->_init && usbd->_config && !usbd->_suspended; }
bool USBD_DTR(USBD_t *usbd) { return usbd->_dtr; }

uint32_t USBD_Baud(USBD_t *usbd)
{
  const uint8_t *coding = usbd->_line_coding;
  return (uint32_t)coding[0] | ((uint32_t)coding[1] << 8) |
    ((uint32_t)coding[2] << 16) | ((uint32_t)coding[3] << 24);
}

status_t USBD_Send(USBD_t *usbd, const uint8_t *data, uint16_t len)
{
  if(!USBD_IsReady(usbd)) return ERR;
  return USB_Send(&usbd->_usb, USBD_EP_BULK, data, len, true);
}

bool USBD_IsBusy(USBD_t *usbd) { return USB_IsBusy(&usbd->_usb, USBD_EP_BULK); }
bool USBD_IsFree(USBD_t *usbd) { return USB_IsFree(&usbd->_usb, USBD_EP_BULK); }

uint16_t USBD_Size(USBD_t *usbd) { return BUFF_Size(usbd->buff); }
uint16_t USBD_MessageCount(USBD_t *usbd) { return BUFF_MessageCount(usbd->buff); }
uint16_t USBD_Read(USBD_t *usbd, uint8_t *data) { return BUFF_Read(usbd->buff, data); }
char *USBD_ReadString(USBD_t *usbd) { return BUFF_ReadString(usbd->buff); }
bool USBD_Skip(USBD_t *usbd) { return BUFF_Skip(usbd->buff); }
void USBD_Clear(USBD_t *usbd) { BUFF_Clear(usbd->buff); }

//-------------------------------------------------------------------------------------------------
#endif

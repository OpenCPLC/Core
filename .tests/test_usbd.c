// .tests/test_usbd.c
// USB device core `USBD_t` against the in-memory controller model `USB_Host*`:
// descriptors, BOS with MS OS 2.0 and WebUSB, enumeration,
// addressing with zero and with a cancelled pending address, bulk transfers on ZLP boundaries,
// CDC line coding and DTR, endpoint halt,
// requests before configuration and for directions that do not exist.
// The model reproduces neither PMA registers nor instruction interleaving with a real IRQ.

#include <string.h>

#include "test.h"
#include "usbd.c"

static uint8_t rx_memory[1024];
static BUFF_t rx_buff = { .memory = rx_memory, .size = sizeof(rx_memory) };

static USBD_t dev = {
  .vid = 0x2FE3,
  .pid = 0x000A,
  .version = 0x0100,
  .manufacturer = "Xaeian",
  .product = "OpenCPLC",
  .guid = "{23456469-7068-6F72-FFFF-58616569616E}",
  .url = "localhost:8000",
  .max_power_mA = 250,
  .self_powered = true,
  .buff = &rx_buff
};

static uint8_t cdc_memory[256];
static BUFF_t cdc_buff = { .memory = cdc_memory, .size = sizeof(cdc_memory) };

static USBD_t com = {
  .mode = USBD_Mode_CDC,
  .vid = 0x2FE3,
  .pid = 0x000B,
  .guid = "{23456469-7068-6F72-FFFF-58616569616E}", // ignored in CDC mode
  .buff = &cdc_buff
};

static uint8_t reply[512];

// One control transfer: SETUP, device services it, host takes the IN data
static uint16_t control_on(USBD_t *usbd, uint8_t type, uint8_t request, uint16_t value,
  uint16_t index, uint16_t length)
{
  uint8_t setup[8] = {
    type, request, value & 0xFF, value >> 8,
    index & 0xFF, index >> 8, length & 0xFF, length >> 8
  };
  USB_HostSetup(&usbd->_usb, setup);
  while(USBD_Step(usbd));
  return USB_HostRead(&usbd->_usb, 0, reply, sizeof(reply));
}

#define control(...) control_on(&dev, __VA_ARGS__)

static bool stalled(USBD_t *usbd) { return USB_EpStalled(&usbd->_usb, 0, true); }
static bool ep0_stalled(void) { return stalled(&dev); }

static void vendor_device(void)
{
  TEST_Check(USBD_Init(&dev) == OK);
  TEST_Check(!USBD_IsReady(&dev));
  USB_HostReset(&dev._usb);
  TEST_Check(USBD_Step(&dev));
  TEST_Check(!USBD_Step(&dev));
  // Device descriptor, USB 2.1 because BOS is present
  TEST_Check(control(0x80, 6, 0x0100, 0, 64) == 18);
  TEST_Check(reply[0] == 18 && reply[1] == 1);
  TEST_Check(reply[2] == 0x01 && reply[3] == 0x02 && reply[4] == 0);
  TEST_Check(reply[8] == 0xE3 && reply[9] == 0x2F && reply[10] == 0x0A && reply[11] == 0x00);
  TEST_Check(reply[14] == 1 && reply[15] == 2 && reply[16] == 3);
  // Short read honours `wLength`
  TEST_Check(control(0x80, 6, 0x0100, 0, 8) == 8);
  // Address takes effect after the status stage
  TEST_Check(USB_HostAddress(&dev._usb) == 0);
  TEST_Check(control(0x00, 5, 5, 0, 0) == 0);
  TEST_Check(USB_HostAddress(&dev._usb) == 5);
  TEST_Check(!ep0_stalled());
  // Configuration: 9 + 9 + 7 + 7, vendor interface with bulk `0x01` and `0x81`
  TEST_Check(control(0x80, 6, 0x0200, 0, 255) == 32);
  TEST_Check(reply[2] == 32 && reply[3] == 0 && reply[4] == 1);
  TEST_Check(reply[7] == 0xC0 && reply[8] == 125);
  TEST_Check(reply[14] == 0xFF && reply[20] == 0x01 && reply[21] == 2 && reply[22] == 64);
  TEST_Check(reply[27] == 0x81);
  // Strings
  TEST_Check(control(0x80, 6, 0x0300, 0, 255) == 4);
  TEST_Check(reply[2] == 0x09 && reply[3] == 0x04);
  TEST_Check(control(0x80, 6, 0x0302, 0x0409, 255) == 2 + 2 * 8);
  TEST_Check(reply[2] == 'O' && reply[3] == 0 && reply[16] == 'C');
  TEST_Check(control(0x80, 6, 0x0303, 0x0409, 255) == 4); // serial "0" without a UID on host
  TEST_Check(control(0x80, 6, 0x0304, 0x0409, 255) == 0);
  TEST_Check(ep0_stalled());
  // BOS: header plus WebUSB and MS OS 2.0 platform capabilities
  TEST_Check(control(0x80, 6, 0x0F00, 0, 255) == 57);
  TEST_Check(reply[2] == 57 && reply[4] == 2);
  TEST_Check(reply[5] == 24 && reply[6] == 16 && reply[7] == 5);
  TEST_Check(reply[27] == 0x01 && reply[28] == 1); // WebUSB vendor code, landing page
  TEST_Check(reply[29] == 28 && reply[49] == 0x00 && reply[50] == 0x00 &&
    reply[51] == 0x03 && reply[52] == 0x06);
  TEST_Check(reply[53] == 162 && reply[54] == 0 && reply[55] == 0x02);
  // Device qualifier exists only at high speed
  TEST_Check(control(0x80, 6, 0x0600, 0, 10) == 0);
  TEST_Check(ep0_stalled());
  // MS OS 2.0 descriptor set
  TEST_Check(control(0xC0, 0x02, 0, 7, 255) == 162);
  TEST_Check(reply[0] == 10 && reply[8] == 162 && reply[9] == 0);
  TEST_Check(memcmp(&reply[14], "WINUSB\0\0", 8) == 0);
  TEST_Check(reply[30] == 132 && reply[32] == 4 && reply[34] == 7 && reply[36] == 42);
  TEST_Check(reply[38] == 'D' && reply[80] == 80 && reply[82] == '{' &&
    reply[160] == 0 && reply[161] == 0);
  // WebUSB URL
  TEST_Check(control(0xC0, 0x01, 1, 2, 255) == 3 + 14);
  TEST_Check(reply[0] == 17 && reply[1] == 3 && reply[2] == 0 && reply[3] == 'l');
  TEST_Check(control(0xC0, 0x01, 2, 2, 255) == 0);
  TEST_Check(ep0_stalled());
  TEST_Check(control(0xC0, 0x02, 0, 8, 255) == 0);
  TEST_Check(ep0_stalled());
  // Class requests belong to CDC mode only
  TEST_Check(control(0x21, 0x22, 1, 0, 0) == 0);
  TEST_Check(ep0_stalled());
  TEST_Check(!USBD_DTR(&dev));
  // Status before configuration
  TEST_Check(control(0x80, 0, 0, 0, 2) == 2);
  TEST_Check(reply[0] == 0x01 && reply[1] == 0);
  TEST_Check(control(0x80, 10, 0, 0, 1) == 0); // GET_INTERFACE needs a configuration
  TEST_Check(ep0_stalled());
  TEST_Check(USBD_Send(&dev, reply, 4) == ERR);
  // Configuration enables the bulk link
  TEST_Check(control(0x00, 9, 1, 0, 0) == 0);
  TEST_Check(USBD_IsReady(&dev));
  TEST_Check(control(0x80, 8, 0, 0, 1) == 1 && reply[0] == 1);
  TEST_Check(control(0x80, 10, 0, 0, 1) == 1 && reply[0] == 0);
  TEST_Check(control(0x80, 10, 0, 1, 1) == 0 && ep0_stalled()); // one interface only
  TEST_Check(control(0x00, 11, 0, 0, 0) == 0 && !ep0_stalled());
  TEST_Check(control(0x00, 11, 1, 0, 0) == 0 && ep0_stalled());
  // Host to device: a short last packet closes the transfer, a full one leaves it open
  uint8_t out[130];
  for(uint16_t i = 0; i < sizeof(out); i++) out[i] = (uint8_t)i;
  TEST_Check(USB_HostWrite(&dev._usb, 1, out, 100));
  TEST_Check(USBD_MessageCount(&dev) == 1 && USBD_Size(&dev) == 100);
  uint8_t got[256];
  TEST_Check(USBD_Read(&dev, got) == 100 && memcmp(got, out, 100) == 0);
  TEST_Check(USB_HostWrite(&dev._usb, 1, out, 64));
  TEST_Check(USBD_MessageCount(&dev) == 0);
  TEST_Check(USB_HostWrite(&dev._usb, 1, out, 0));
  TEST_Check(USBD_MessageCount(&dev) == 1 && USBD_Size(&dev) == 64);
  TEST_Check(USBD_Skip(&dev));
  // Device to host
  TEST_Check(USBD_Send(&dev, out, 130) == OK);
  TEST_Check(USB_HostRead(&dev._usb, 1, got, sizeof(got)) == 130 && memcmp(got, out, 130) == 0);
  // Endpoint halt through the standard feature
  TEST_Check(control(0x02, 3, 0, 0x81, 0) == 0);
  TEST_Check(USB_EpStalled(&dev._usb, 1, true));
  TEST_Check(control(0x82, 0, 0, 0x81, 2) == 2 && reply[0] == 1);
  TEST_Check(USBD_Send(&dev, out, 4) == ERR);
  TEST_Check(control(0x02, 1, 0, 0x81, 0) == 0);
  TEST_Check(!USB_EpStalled(&dev._usb, 1, true));
  TEST_Check(control(0x02, 3, 0, 0x03, 0) == 0); // no such endpoint
  TEST_Check(ep0_stalled());
  // Remote wakeup is not offered, so the feature is refused
  TEST_Check(control(0x00, 3, 1, 0, 0) == 0);
  TEST_Check(ep0_stalled());
  // Data to the device is a class matter, a vendor request carrying it is refused
  TEST_Check(control(0x40, 0x10, 0, 0, 4) == 0);
  TEST_Check(ep0_stalled());
  // Suspend and resume
  USB_HostSuspend(&dev._usb, true);
  while(USBD_Step(&dev));
  TEST_Check(!USBD_IsReady(&dev));
  USB_HostSuspend(&dev._usb, false);
  while(USBD_Step(&dev));
  TEST_Check(USBD_IsReady(&dev));
  // Bus reset drops the configuration and anything queued
  USB_HostWrite(&dev._usb, 1, out, 10);
  USB_HostReset(&dev._usb);
  while(USBD_Step(&dev));
  TEST_Check(!USBD_IsReady(&dev));
  TEST_Check(USBD_MessageCount(&dev) == 0);
  TEST_Check(!USB_HostWrite(&dev._usb, 1, out, 10));
  TEST_Check(dev._usb.resets == 2 && dev._usb.errors == 0 && dev._usb.overflow == 0);
}

static void device_without_bos(void)
{
  // Device without BOS strings speaks USB 2.0
  static uint8_t memory[64];
  static BUFF_t buff = { .memory = memory, .size = sizeof(memory) };
  static USBD_t plain = { .vid = 1, .pid = 2, .buff = &buff };
  TEST_Check(USBD_Init(&plain) == OK);
  USB_HostReset(&plain._usb);
  while(USBD_Step(&plain));
  TEST_Check(control_on(&plain, 0x80, 6, 0x0100, 0, 18) == 18);
  TEST_Check(reply[2] == 0x00 && reply[3] == 0x02 &&
    reply[14] == 0 && reply[15] == 0 && reply[16] == 3);
  TEST_Check(control_on(&plain, 0x80, 6, 0x0F00, 0, 255) == 0);
  TEST_Check(stalled(&plain));
}

static void cdc_device(void)
{
  TEST_Check(USBD_Init(&com) == OK);
  USB_HostReset(&com._usb);
  while(USBD_Step(&com));
  // Device class CDC, USB 2.0: no BOS even though a GUID is set
  TEST_Check(control_on(&com, 0x80, 6, 0x0100, 0, 18) == 18);
  TEST_Check(reply[2] == 0x00 && reply[3] == 0x02 && reply[4] == 0x02);
  TEST_Check(control_on(&com, 0x80, 6, 0x0F00, 0, 255) == 0);
  TEST_Check(stalled(&com));
  TEST_Check(control_on(&com, 0xC0, 0x02, 0, 7, 255) == 0);
  // Configuration: communication interface, functional set, notify EP, data interface
  TEST_Check(control_on(&com, 0x80, 6, 0x0200, 0, 255) == 67);
  TEST_Check(reply[2] == 67 && reply[3] == 0 && reply[4] == 2);
  TEST_Check(reply[9] == 9 && reply[14] == 0x02 && reply[15] == 0x02 && reply[16] == 0x00);
  TEST_Check(reply[18] == 5 && reply[19] == 0x24 && reply[20] == 0x00 &&
    reply[21] == 0x10 && reply[22] == 0x01);
  TEST_Check(reply[23] == 5 && reply[25] == 0x01 && reply[27] == 1);
  TEST_Check(reply[28] == 4 && reply[30] == 0x02 && reply[31] == 0x02);
  TEST_Check(reply[32] == 5 && reply[34] == 0x06 && reply[35] == 0 && reply[36] == 1);
  TEST_Check(reply[37] == 7 && reply[39] == 0x82 && reply[40] == 3 &&
    reply[41] == 8 && reply[43] == 16);
  TEST_Check(reply[44] == 9 && reply[46] == 1 && reply[48] == 2 && reply[49] == 0x0A);
  TEST_Check(reply[55] == 0x01 && reply[56] == 2 && reply[62] == 0x81);
  // Line coding defaults to 115200 8N1 and follows the host
  TEST_Check(control_on(&com, 0xA1, 0x21, 0, 0, 7) == 7);
  TEST_Check(reply[0] == 0x00 && reply[1] == 0xC2 && reply[2] == 0x01 &&
    reply[3] == 0 && reply[6] == 8);
  TEST_Check(USBD_Baud(&com) == 115200);
  TEST_Check(control_on(&com, 0x00, 9, 1, 0, 0) == 0);
  TEST_Check(USBD_IsReady(&com) && !USBD_DTR(&com));
  TEST_Check(control_on(&com, 0x21, 0x22, 1, 0, 0) == 0 && !stalled(&com));
  TEST_Check(USBD_DTR(&com));
  TEST_Check(control_on(&com, 0x21, 0x22, 0, 0, 0) == 0);
  TEST_Check(!USBD_DTR(&com));
  // SET_LINE_CODING: SETUP first, the data packet later, status only after the data
  static const uint8_t coding[7] = { 0x00, 0x96, 0x00, 0x00, 0, 0, 8 }; // 38400 8N1
  TEST_Check(control_on(&com, 0x21, 0x20, 0, 0, 7) == 0 && !stalled(&com));
  TEST_Check(USBD_Baud(&com) == 115200);
  TEST_Check(!USBD_Step(&com));
  TEST_Check(USB_HostWrite(&com._usb, 0, coding, 7));
  TEST_Check(USBD_Step(&com) && !USBD_Step(&com));
  TEST_Check(!stalled(&com));
  TEST_Check(USBD_Baud(&com) == 38400);
  TEST_Check(control_on(&com, 0xA1, 0x21, 0, 0, 7) == 7 && reply[1] == 0x96);
  // Wrong length, wrong interface, unknown request
  TEST_Check(control_on(&com, 0x21, 0x20, 0, 0, 8) == 0 && stalled(&com));
  TEST_Check(control_on(&com, 0x21, 0x22, 1, 1, 0) == 0 && stalled(&com));
  TEST_Check(control_on(&com, 0x21, 0x23, 0, 0, 0) == 0 && stalled(&com));
  // A SETUP cutting a data stage short cancels it
  TEST_Check(control_on(&com, 0x21, 0x20, 0, 0, 7) == 0);
  TEST_Check(control_on(&com, 0xA1, 0x21, 0, 0, 7) == 7 && reply[1] == 0x96);
  TEST_Check(USB_HostWrite(&com._usb, 0, coding, 7));
  TEST_Check(!USBD_Step(&com));
  // Two interfaces answer GET_INTERFACE, a third does not
  TEST_Check(control_on(&com, 0x81, 10, 0, 1, 1) == 1 && reply[0] == 0);
  TEST_Check(control_on(&com, 0x81, 10, 0, 2, 1) == 0 && stalled(&com));
  // The bulk link works as in vendor mode
  uint8_t out[5] = { 1, 2, 3, 4, 5 };
  TEST_Check(USB_HostWrite(&com._usb, 1, out, 5));
  TEST_Check(USBD_MessageCount(&com) == 1 && USBD_Size(&com) == 5);
  TEST_Check(USBD_Send(&com, out, 5) == OK);
  TEST_Check(USB_HostRead(&com._usb, 1, reply, sizeof(reply)) == 5 && reply[4] == 5);
  // Reset closes the port
  USB_HostReset(&com._usb);
  while(USBD_Step(&com));
  TEST_Check(!USBD_IsReady(&com) && !USBD_DTR(&com));
  TEST_Check(com._usb.errors == 0 && com._usb.overflow == 0);
}

// Address zero, endpoints before configuration and directions the configuration never had
static void requests_at_the_edges(void)
{
  // Both devices sit reset: endpoint feature requests reach nothing yet
  TEST_Check(control(0x02, 1, 0, 0x01, 0) == 0);
  TEST_Check(ep0_stalled());
  TEST_Check(control(0x02, 3, 0, 0x81, 0) == 0);
  TEST_Check(ep0_stalled());
  TEST_Check(control(0x82, 0, 0, 0x81, 2) == 0);
  TEST_Check(ep0_stalled());
  // Address zero returns the device to default like any other address
  TEST_Check(control(0x00, 5, 9, 0, 0) == 0);
  TEST_Check(USB_HostAddress(&dev._usb) == 9);
  // A new SETUP abandons an address whose status stage has not completed
  static const uint8_t get_config[8] = { 0x80, 8, 0, 0, 0, 0, 1, 0 };
  USB_SetAddress(&dev._usb, 12);
  USB_HostSetup(&dev._usb, get_config);
  while(USBD_Step(&dev));
  TEST_Check(USB_HostRead(&dev._usb, 0, reply, sizeof(reply)) == 1);
  TEST_Check(USB_HostAddress(&dev._usb) == 9);
  TEST_Check(control(0x00, 5, 0, 0, 0) == 0);
  TEST_Check(USB_HostAddress(&dev._usb) == 0);
  // A stale high-level configuration must not send through disabled hardware
  TEST_Check(control(0x00, 9, 1, 0, 0) == 0);
  USB_EpDisable(&dev._usb, 1);
  TEST_Check(USBD_IsReady(&dev));
  TEST_Check(USBD_Send(&dev, reply, 1) == ERR);
  USB_EpEnable(&dev._usb, 1);
  TEST_Check(USBD_Send(&dev, reply, 1) == OK);
  TEST_Check(USB_HostRead(&dev._usb, 1, reply, sizeof(reply)) == 1);
  TEST_Check(control(0x00, 9, 0, 0, 0) == 0);
  // CDC owns EP2 as IN only: the OUT side of it does not exist
  TEST_Check(control_on(&com, 0x00, 9, 1, 0, 0) == 0);
  TEST_Check(control_on(&com, 0x02, 3, 0, 0x02, 0) == 0);
  TEST_Check(stalled(&com));
  TEST_Check(control_on(&com, 0x02, 1, 0, 0x02, 0) == 0);
  TEST_Check(stalled(&com));
  TEST_Check(control_on(&com, 0x82, 0, 0, 0x02, 2) == 0); // GET_STATUS neither
  TEST_Check(stalled(&com));
  TEST_Check(control_on(&com, 0x02, 3, 0, 0x82, 0) == 0 && !stalled(&com));
  TEST_Check(USB_EpStalled(&com._usb, 2, true));
  TEST_Check(control_on(&com, 0x02, 1, 0, 0x82, 0) == 0);
  TEST_Check(!USB_EpStalled(&com._usb, 2, true));
}

void TEST_Cases(void)
{
  TEST(vendor_device);
  TEST(device_without_bos);
  TEST(cdc_device);
  TEST(requests_at_the_edges);
}

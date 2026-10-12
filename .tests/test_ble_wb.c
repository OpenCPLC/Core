// .tests/test_ble_wb.c
// GATT library `BLE_t` against the CPU2 mailbox model `WPAN_Host*`:
// init command sequence with sizing and UUID layout, fault reporting per step,
// connection lifecycle, client writes with truncation guards, CCCD bits,
// notification and chunked indication flows, procedure timeout,
// and advertising that keeps the service UUID beside a shortened long name.
// The model scripts replies and injects events the way the radio does.
// It reproduces neither time nor IPCC registers, so buffer ownership after a timeout
// stays a test of the mailbox layer or of the hardware.

#include <string.h>

#include "test.h"
#include "ble_wb.c"
#include "wpan_host.h"

//----------------------------------------------------------------------------------------- Service

static uint8_t rx_memory[256];
static BUFF_t rx_buff = { .memory = rx_memory, .size = sizeof(rx_memory) };

static uint16_t written_len;

static void on_write(const uint8_t *data, uint16_t len)
{
  unused(data);
  written_len = len;
}

enum { CH_RX, CH_TX, CH_REG, CH_NTF };

static BLE_Char_t chars[] = {
  [CH_RX] = { .number = 1, .properties = BLE_Prop_Write, .size = 64, .buff = &rx_buff },
  [CH_TX] = { .number = 2, .properties = BLE_Prop_Indicate, .size = 20 },
  [CH_REG] = { .number = 3, .properties = BLE_Prop_Read | BLE_Prop_Write, .size = 4,
    .OnWrite = on_write },
  [CH_NTF] = { .number = 4, .properties = BLE_Prop_Notify, .size = 4 }
};

static BLE_t ble = {
  .name = "OpenCPLC",
  .uuid = {
    0x4F, 0x70, 0x65, 0x6E, 0x43, 0x50, 0x4C, 0x43,
    0x00, 0x00, 0x58, 0x61, 0x65, 0x69, 0x61, 0x6E
  },
  .chars = chars,
  .count = array_len(chars)
};

// 4f70656e-4350-4c43-0000-58616569616e, encoded least significant byte first
static const uint8_t service_uuid[16] = {
  0x6E, 0x61, 0x69, 0x65, 0x61, 0x58, 0x00, 0x00,
  0x43, 0x4C, 0x50, 0x43, 0x6E, 0x65, 0x70, 0x4F
};

//----------------------------------------------------------------------------------------- Helpers

static uint8_t param[256];
static uint8_t plen;

static uint16_t cmd(void) { return WPAN_HostCommand(param, &plen); }

// One event the way the CPU2 frames it: packet type, event code, payload length, payload
static void event(uint8_t code, const uint8_t *payload, uint8_t len)
{
  uint8_t buf[3 + 255];
  buf[0] = 0x04;
  buf[1] = code;
  buf[2] = len;
  if(len) memcpy(&buf[3], payload, len);
  WPAN_HostEvent(buf, (uint8_t)(3 + len));
}

static void vendor(uint16_t code, const uint8_t *data, uint8_t len)
{
  uint8_t payload[255];
  payload[0] = (uint8_t)code;
  payload[1] = (uint8_t)(code >> 8);
  if(len) memcpy(&payload[2], data, len);
  event(0xFF, payload, (uint8_t)(2 + len));
}

// Client write of `attribute`, `more` keeps bit 15 of the offset up
static void remote_write(uint16_t attribute, const uint8_t *data, uint8_t len, bool more)
{
  uint8_t payload[255] = { 0x40, 0x00 };
  payload[2] = (uint8_t)attribute;
  payload[3] = (uint8_t)(attribute >> 8);
  payload[5] = more ? 0x80 : 0x00;
  payload[6] = len;
  memcpy(&payload[8], data, len);
  vendor(0x0C01, payload, (uint8_t)(8 + len));
}

static void confirmation(void)
{
  static const uint8_t handle[2] = { 0x40, 0x00 };
  vendor(0x0C17, handle, sizeof(handle));
}

//------------------------------------------------------------------------------------------- Tests

static void faults_name_their_step(void)
{
  static BLE_Char_t zero[] = { { .number = 1, .properties = BLE_Prop_Read, .size = 0 } };
  static BLE_t bad = { .name = "x", .uuid = { 1 }, .chars = zero, .count = 1 };
  TEST_Check(BLE_Init(&bad) == ERR);
  TEST_Check(bad.fault == BLE_Fault_Config);
  static BLE_Char_t no_number[] = { { .properties = BLE_Prop_Read, .size = 1 } };
  static BLE_t bad_number = {
    .name = "x", .uuid = { 1 }, .chars = no_number, .count = 1
  };
  TEST_Check(BLE_Init(&bad_number) == ERR && bad_number.fault == BLE_Fault_Config);
  static BLE_Char_t too_large[] = {
    { .number = 1, .properties = BLE_Prop_Read, .size = BLE_CHUNK_SIZE + 1 }
  };
  static BLE_t bad_size = { .name = "x", .uuid = { 1 }, .chars = too_large, .count = 1 };
  TEST_Check(BLE_Init(&bad_size) == ERR && bad_size.fault == BLE_Fault_Config);
  // Service record count is one byte in `ACI_GATT_ADD_SERVICE`
  static BLE_Char_t many[85];
  static BLE_t bad_records = { .name = "x", .uuid = { 1 }, .chars = many, .count = 85 };
  for(uint8_t i = 0; i < array_len(many); i++) {
    many[i].number = i + 1;
    many[i].properties = BLE_Prop_Notify;
    many[i].size = 1;
  }
  TEST_Check(BLE_Init(&bad_records) == ERR && bad_records.fault == BLE_Fault_Config);
  // The stack refuses `BLE_INIT`: the step and its status byte come back
  static BLE_Char_t one[] = { { .number = 1, .properties = BLE_Prop_Read, .size = 4 } };
  static BLE_t refused = { .name = "x", .uuid = { 1 }, .chars = one, .count = 1 };
  WPAN_HostRespond(0x42, NULL, 0);
  TEST_Check(BLE_Init(&refused) == ERR);
  TEST_Check(refused.fault == BLE_Fault_StackInit && refused.fault_status == 0x42);
  WPAN_HostReset();
}

static void init_responses(void)
{
  // Responses in call order, the data-bearing ones give out the handles
  static const uint8_t gap[6] = { 0x01, 0x00, 0x02, 0x00, 0x03, 0x00 };
  static const uint8_t service[2] = { 0x0C, 0x00 };
  static const uint8_t handle[4][2] = { { 0x10 }, { 0x20 }, { 0x30 }, { 0x40 } };
  WPAN_HostRespond(0, NULL, 0); // stack init
  WPAN_HostRespond(0, NULL, 0); // reset
  WPAN_HostRespond(0, NULL, 0); // address
  WPAN_HostRespond(0, NULL, 0); // power
  WPAN_HostRespond(0, NULL, 0); // gatt init
  WPAN_HostRespond(0, gap, sizeof(gap));
  WPAN_HostRespond(0, NULL, 0); // name
  WPAN_HostRespond(0, service, sizeof(service));
  for(uint8_t i = 0; i < 4; i++) WPAN_HostRespond(0, handle[i], 2);
}

static void init_sequence(void)
{
  init_responses();
  TEST_Check(BLE_Init(&ble) == OK);
  // Sizing from the declaration: 11 records, 9 + 10 attributes, value storage
  TEST_Check(WPAN_HostAttributes() == 19);
  TEST_Check(WPAN_HostValues() == 64 + (19 + 64) + (19 + 20 + 2) + (19 + 4) + (19 + 4 + 2));
  // The command sequence of the bring-up
  TEST_Check(cmd() == 0x0C03); // HCI reset
  TEST_Check(cmd() == 0xFC0C && plen == 8 && param[1] == 6); // public address from the UID
  TEST_Check(param[2] == 1 && param[3] == 2 && param[4] == 5 && param[7] == 8);
  TEST_Check(cmd() == 0xFC0F); // TX power
  TEST_Check(cmd() == 0xFD01); // GATT init
  TEST_Check(cmd() == 0xFC8A && plen == 3 && param[2] == 8); // GAP init, name length
  TEST_Check(cmd() == 0xFD06 && param[0] == 1 && param[2] == 2); // name into the GAP service
  TEST_Check(memcmp(&param[6], "OpenCPLC", 8) == 0);
  TEST_Check(cmd() == 0xFD02 && plen == 19 && param[18] == 11); // service with its records
  TEST_Check(memcmp(&param[1], service_uuid, 16) == 0);
  TEST_Check(param[7] == 0 && param[8] == 0); // fourth group zeroed for the service
  TEST_Check(cmd() == 0xFD04 && param[9] == 1 && param[10] == 0); // characteristic number
  TEST_Check(memcmp(&param[3], service_uuid, 6) == 0);
  TEST_Check(memcmp(&param[11], &service_uuid[8], 8) == 0);
  TEST_Check(param[19] == 64 && param[21] == BLE_Prop_Write); // size and properties
  TEST_Check(cmd() == 0xFD04 && param[9] == 2);
  TEST_Check(cmd() == 0xFD04 && param[9] == 3);
  TEST_Check(cmd() == 0xFD04 && param[9] == 4);
  TEST_Check(cmd() == 0xFC83); // discoverable with the advertised name
  TEST_Check(memcmp(&param[9], "OpenCPLC", 8) == 0); // name entry after its AD type byte
  TEST_Check(cmd() == 0xFC8F); // TX power entry dropped from advertising
  TEST_Check(cmd() == 0xFC8E && param[2] == 0x07); // complete service UUID appended
  TEST_Check(plen == 19 && param[0] == 18 && param[1] == 17);
  TEST_Check(memcmp(&param[3], service_uuid, 16) == 0);
  TEST_Check(cmd() == 0x2009 && plen == 32 && param[0] == 10 && param[1] == 9);
  TEST_Check(param[2] == 0x09 && memcmp(&param[3], "OpenCPLC", 8) == 0);
  TEST_Check(ble.adv_status.start == 0 && ble.adv_status.tx_power == 0);
  TEST_Check(ble.adv_status.uuid == 0 && ble.adv_status.scan_response == 0);
  TEST_Check(cmd() == 0);
  TEST_Check(!BLE_Step()); // nothing pending after the bring-up
}

static void connect_and_mtu(void)
{
  static const uint8_t cut[4] = { 0x01, 0x00, 0x40, 0x00 };
  TEST_Check(!BLE_IsConnected(&ble));
  event(0x3E, cut, sizeof(cut));
  TEST_Check(BLE_Step() && !BLE_IsConnected(&ble));
  static const uint8_t connect[19] = { 0x01, 0x00, 0x40, 0x00 };
  event(0x3E, connect, sizeof(connect));
  TEST_Check(BLE_Step() && BLE_IsConnected(&ble) && BLE_Mtu(&ble) == 23);
  static const uint8_t mtu[4] = { 0x40, 0x00, 0x9E, 0x00 };
  vendor(0x0C03, mtu, sizeof(mtu));
  TEST_Check(BLE_Step() && BLE_Mtu(&ble) == 158);
}

static void client_writes(void)
{
  uint8_t got[64];
  remote_write(0x11, (const uint8_t *)"hello", 5, false);
  TEST_Check(BLE_Step());
  TEST_Check(BLE_Size(&chars[CH_RX]) == 5);
  TEST_Check(BLE_Read(&chars[CH_RX], got) == 5 && memcmp(got, "hello", 5) == 0);
  // Long write in two pieces becomes one message
  remote_write(0x11, (const uint8_t *)"ab", 2, true);
  remote_write(0x11, (const uint8_t *)"cd", 2, false);
  TEST_Check(BLE_Step() && BLE_MessageCount(&chars[CH_RX]) == 1);
  TEST_Check(BLE_Read(&chars[CH_RX], got) == 4 && memcmp(got, "abcd", 4) == 0);
  // A truncated event delivers nothing
  uint8_t bad[9] = { 0x01, 0x0C, 0x40, 0x00, 0x11, 0x00, 0x00, 0x00, 0x40 };
  event(0xFF, bad, sizeof(bad));
  BLE_Step();
  TEST_Check(BLE_Size(&chars[CH_RX]) == 0);
  uint8_t cut_data[10] = { 0x01, 0x0C, 0x40, 0x00, 0x11, 0x00, 0, 0, 4, 0 };
  event(0xFF, cut_data, sizeof(cut_data));
  TEST_Check(BLE_Step() && BLE_Size(&chars[CH_RX]) == 0);
  vendor(0x0C03, NULL, 0);
  TEST_Check(BLE_Step() && BLE_Mtu(&ble) == 158);
  // The register characteristic reports through its callback
  remote_write(0x31, (const uint8_t *)"\x12\x34", 2, false);
  TEST_Check(BLE_Step() && written_len == 2);
}

static void notify_and_indicate(void)
{
  static const uint8_t data[50] = { [0] = 7, [49] = 9 };
  // No subscription, the send is refused
  TEST_Check(BLE_Send(&ble, &chars[CH_TX], data, 4) == ERR);
  static const uint8_t on[2] = { 0x02, 0x00 };
  remote_write(0x22, on, 2, false);
  TEST_Check(BLE_Step() && BLE_Subscribed(&chars[CH_TX]));
  // Indication stream: chunks of the characteristic size, each awaits confirmation
  TEST_Check(BLE_Send(&ble, &chars[CH_TX], data, sizeof(data)) == OK && BLE_IsBusy(&ble));
  BLE_Step();
  TEST_Check(cmd() == 0xFD06 && param[5] == 20 && param[6] == 7);
  vendor(0x0C17, NULL, 0);
  TEST_Check(BLE_Step());
  BLE_Step();
  TEST_Check(cmd() == 0); // confirmation still pending, nothing new goes out
  confirmation();
  BLE_Step();
  TEST_Check(cmd() == 0xFD06 && param[5] == 20);
  confirmation();
  BLE_Step();
  TEST_Check(cmd() == 0xFD06 && param[5] == 10);
  TEST_Check(BLE_IsBusy(&ble));
  confirmation();
  TEST_Check(BLE_Step() && BLE_IsFree(&ble));
  // Notifications go out without a confirmation
  static const uint8_t ntf_on[2] = { 0x01, 0x00 };
  remote_write(0x42, ntf_on, 2, false);
  BLE_Step();
  TEST_Check(BLE_Send(&ble, &chars[CH_NTF], data, 4) == OK);
  BLE_Step();
  TEST_Check(cmd() == 0xFD06 && param[5] == 4 && BLE_IsFree(&ble));
  // A read-only value update runs through the loop as well
  TEST_Check(BLE_Send(&ble, &chars[CH_REG], data, 2) == OK);
  BLE_Step();
  TEST_Check(cmd() == 0xFD06 && param[2] == 0x30 && BLE_IsFree(&ble));
  // Unsubscribing mid-transfer abandons it
  uint16_t errors = ble.errors;
  TEST_Check(BLE_Send(&ble, &chars[CH_TX], data, sizeof(data)) == OK);
  BLE_Step();
  TEST_Check(cmd() == 0xFD06);
  static const uint8_t off[2] = { 0x00, 0x00 };
  remote_write(0x22, off, 2, false);
  BLE_Step();
  TEST_Check(BLE_IsFree(&ble) && ble.errors == errors + 1);
}

static void timeout_and_disconnect(void)
{
  // Header without the connection handle is not a complete procedure timeout
  vendor(0x0C02, NULL, 0);
  TEST_Check(BLE_Step() && BLE_IsConnected(&ble));
  TEST_Check(cmd() == 0);
  // Procedure timeout leaves a dead GATT channel: the link goes down on purpose
  static const uint8_t timeout[2] = { 0x40, 0x00 };
  vendor(0x0C02, timeout, sizeof(timeout));
  BLE_Step();
  TEST_Check(cmd() == 0xFC93 && param[0] == 0x40 && param[2] == 0x13);
  event(0x05, NULL, 0);
  TEST_Check(BLE_Step() && BLE_IsConnected(&ble));
  // Disconnect clears the session and restarts advertising
  static const uint8_t gone[4] = { 0x00, 0x40, 0x00, 0x13 };
  event(0x05, gone, sizeof(gone));
  TEST_Check(BLE_Step());
  TEST_Check(!BLE_IsConnected(&ble) && !BLE_Subscribed(&chars[CH_TX]));
  TEST_Check(cmd() == 0xFC83 && cmd() == 0xFC8F && cmd() == 0xFC8E);
  TEST_Check(cmd() == 0x2009 && cmd() == 0);
  TEST_Check(ble.resets == 1);
}

static void advertising_statuses_and_limits(void)
{
  // Each optional failure remains observable without preventing initialization.
  // The combined failure preserves the cause as well as the refused UUID update.
  static const uint8_t replies[][4] = {
    { 0, 0, 0, 0 }, { 0, 0x42, 0, 0 }, { 0, 0, 0x07, 0 },
    { 0, 0, 0, 0x0C }, { 0, 0x42, 0x07, 0x0C }, { 0x0C, 0, 0, 0 }
  };
  static BLE_t probe;
  for(uint8_t i = 0; i < array_len(replies); i++) {
    WPAN_HostReset();
    probe = (BLE_t){ .name = "OpenCPLC", .chars = chars, .count = array_len(chars) };
    memcpy(probe.uuid, ble.uuid, sizeof(probe.uuid));
    init_responses();
    for(uint8_t j = 0; j < 4; j++) WPAN_HostRespond(replies[i][j], NULL, 0);
    TEST_Check(BLE_Init(&probe) == (replies[i][0] ? ERR : OK));
    TEST_Check(probe.adv_status.start == replies[i][0]);
    if(replies[i][0]) {
      TEST_Check(probe.fault == BLE_Fault_Advertise && probe.fault_status == replies[i][0]);
      TEST_Check(!probe._init && probe.errors == 0);
      TEST_Check(probe.adv_status.tx_power == 0xFF && probe.adv_status.uuid == 0xFF);
      TEST_Check(probe.adv_status.scan_response == 0xFF);
      uint16_t opcode;
      while((opcode = cmd()) && opcode != 0xFC83) {}
      TEST_Check(opcode == 0xFC83);
      TEST_Check(cmd() == 0); // no updates after the start was refused
      continue;
    }
    TEST_Check(probe._init && probe.fault == BLE_Fault_None);
    TEST_Check(probe.adv_status.tx_power == replies[i][1]);
    TEST_Check(probe.adv_status.uuid == replies[i][2]);
    TEST_Check(probe.adv_status.scan_response == replies[i][3]);
    uint16_t errors = !!replies[i][1] + !!replies[i][2] + !!replies[i][3];
    TEST_Check(probe.errors == errors);
    // An accepted restart replaces all previous statuses but keeps the counter
    WPAN_HostReset();
    static const uint8_t gone[4] = { 0, 0x40, 0, 0x13 };
    event(0x05, gone, sizeof(gone));
    TEST_Check(BLE_Step());
    TEST_Check(probe.adv_status.start == 0 && probe.adv_status.tx_power == 0);
    TEST_Check(probe.adv_status.uuid == 0 && probe.adv_status.scan_response == 0);
    TEST_Check(probe.errors == errors);
    TEST_Check(cmd() == 0xFC83 && cmd() == 0xFC8F && cmd() == 0xFC8E);
    TEST_Check(cmd() == 0x2009 && cmd() == 0);
    // A refused restart also invalidates the old successful update statuses
    WPAN_HostRespond(0x0C, NULL, 0);
    event(0x05, gone, sizeof(gone));
    TEST_Check(BLE_Step() && probe.errors == errors + 1);
    TEST_Check(probe.adv_status.start == 0x0C && probe.adv_status.tx_power == 0xFF);
    TEST_Check(probe.adv_status.uuid == 0xFF && probe.adv_status.scan_response == 0xFF);
    TEST_Check(probe.fault == BLE_Fault_None);
    TEST_Check(cmd() == 0xFC83 && cmd() == 0);
  }

  // Byte limits, including a name whose length would wrap in a `uint8_t`
  static const uint16_t lengths[] = { 1, 8, 9, 23, 24, 256 };
  static char name[257];
  for(uint8_t i = 0; i < array_len(lengths); i++) {
    WPAN_HostReset();
    memset(name, 'N', lengths[i]);
    name[lengths[i]] = 0;
    probe = (BLE_t){ .name = name, .chars = chars, .count = array_len(chars) };
    memcpy(probe.uuid, ble.uuid, sizeof(probe.uuid));
    init_responses();
    TEST_Check(BLE_Init(&probe) == OK);
    uint8_t full = lengths[i] > 23 ? 23 : (uint8_t)lengths[i];
    uint8_t adv = full > 8 ? 8 : full;
    uint16_t opcode;
    uint8_t seen = 0;
    while((opcode = cmd())) {
      if(opcode == 0xFC8A) {
        TEST_Check(plen == 3 && param[2] == full);
        seen++;
      }
      else if(opcode == 0xFD06) {
        TEST_Check(plen == 6 + full && param[5] == full);
        TEST_Check(memcmp(&param[6], name, full) == 0);
        seen++;
      }
      else if(opcode == 0xFC83) {
        TEST_Check(plen == 14 + adv && param[7] == adv + 1);
        TEST_Check(param[8] == (full > adv ? 0x08 : 0x09));
        TEST_Check(memcmp(&param[9], name, adv) == 0 && param[9 + adv] == 0);
        TEST_Check(3 + param[7] + 1 + 18 <= 31, "flags, name and service within 31 bytes");
        TEST_Check(cmd() == 0xFC8F && plen == 1 && param[0] == 0x0A);
        TEST_Check(cmd() == 0xFC8E && plen == 19 && param[0] == 18 && param[1] == 17);
        TEST_Check(param[2] == 0x07 && memcmp(&param[3], service_uuid, 16) == 0);
        TEST_Check(cmd() == 0x2009 && plen == 32);
        TEST_Check(param[0] == full + 2 && param[1] == full + 1 && param[2] == 0x09);
        TEST_Check(memcmp(&param[3], name, full) == 0);
        for(uint8_t j = full + 3; j < 32; j++) TEST_Check(param[j] == 0);
        seen++;
      }
    }
    TEST_Check(seen == 3);
  }
}

void TEST_Cases(void)
{
  TEST(faults_name_their_step);
  TEST(init_sequence);
  TEST(connect_and_mtu);
  TEST(client_writes);
  TEST(notify_and_indicate);
  TEST(timeout_and_disconnect);
  TEST(advertising_statuses_and_limits);
}

// .tests/test_ble_wb_pin.c

// GATT library built with `BLE_PIN_ENABLE` against the CPU2 mailbox model `WPAN_Host*`:
// pairing set-up bytes at start, the passkey only inside the window and after the delay,
// the bond limit with a client pairing again, the security level gating `BLE_IsConnected`
// and the pushes, and `BLE_Forget` dropping the link before the table goes.

#define BLE_PIN_ENABLE ON

#include <string.h>

#include "test.h"
#include "ble_wb.c"
#include "wpan_host.h"

//----------------------------------------------------------------------------------------- Service

enum { CH_RX, CH_TX };

static BLE_Char_t chars[] = {
  [CH_RX] = { .number = 1, .properties = BLE_Prop_Write, .size = 20 },
  [CH_TX] = { .number = 2, .properties = BLE_Prop_Notify, .size = 20 }
};

static BLE_t ble;

static const uint8_t uuid[16] = {
  0x4F, 0x70, 0x65, 0x6E, 0x43, 0x50, 0x4C, 0x43,
  0x00, 0x00, 0x58, 0x61, 0x65, 0x69, 0x61, 0x6E
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

// Vendor event of connection `0x0040`, `extra` follows the handle
static void vendor(uint16_t code, const uint8_t *extra, uint8_t len)
{
  uint8_t payload[255] = { (uint8_t)code, (uint8_t)(code >> 8), 0x40, 0x00 };
  if(len) memcpy(&payload[4], extra, len);
  event(0xFF, payload, (uint8_t)(4 + len));
}

// Responses in call order: the stack and the GAP handles, the pairing set-up with `bonds`
// in the table, then the service and characteristic handles.
static void init_responses(uint8_t bonds)
{
  static const uint8_t gap[6] = { 0x01, 0x00, 0x02, 0x00, 0x03, 0x00 };
  static const uint8_t service[2] = { 0x0C, 0x00 };
  static const uint8_t handle[2][2] = { { 0x10 }, { 0x20 } };
  for(uint8_t i = 0; i < 5; i++) WPAN_HostRespond(0, NULL, 0); // stack init up to GATT init
  WPAN_HostRespond(0, gap, sizeof(gap));
  for(uint8_t i = 0; i < 4; i++) WPAN_HostRespond(0, NULL, 0); // name, its permission, IO, auth
  WPAN_HostRespond(0, &bonds, 1);
  WPAN_HostRespond(0, service, sizeof(service));
  for(uint8_t i = 0; i < 2; i++) WPAN_HostRespond(0, handle[i], 2);
}

static void instance(uint32_t pin)
{
  WPAN_HostReset();
  ble = (BLE_t){ .name = "OpenCPLC", .chars = chars, .count = array_len(chars), .pin = pin };
  memcpy(ble.uuid, uuid, sizeof(uuid));
}

// Instance with `bonds` in the table, its start-up commands already taken
static void start(uint8_t bonds)
{
  instance(123456);
  init_responses(bonds);
  TEST_Check(BLE_Init(&ble) == OK);
  while(cmd()) {}
}

// Client connected on handle `0x0040`, the security request taken
static void connect(void)
{
  static const uint8_t connected[19] = { 0x01, 0x00, 0x40, 0x00 };
  event(0x3E, connected, sizeof(connected));
  BLE_Step();
  TEST_Check(cmd() == 0xFC8D && plen == 2 && param[0] == 0x40, "security request on connect");
}

// The stack asks for the passkey: `true` when given, `false` when the link drops instead
static bool passkey_asked(void)
{
  vendor(0x0402, NULL, 0);
  BLE_Step();
  uint16_t opcode = cmd();
  if(opcode == 0xFC88) {
    static const uint8_t answer[6] = { 0x40, 0x00, 0x40, 0xE2, 0x01, 0x00 }; // 123456
    TEST_Check(plen == sizeof(answer));
    TEST_Bytes(param, answer, sizeof(answer), "handle and passkey, least significant first");
    return true;
  }
  TEST_Check(opcode == 0xFC93 && param[2] == 0x05, "dropped with authentication failure");
  return false;
}

// Pairing ends with `status`, `0` = success, a failure gives the confirm value as reason
static void pairing_complete(uint8_t status)
{
  uint8_t result[2] = { status, status ? 0x04 : 0x00 };
  vendor(0x0401, result, sizeof(result));
  BLE_Step();
}

// Client enables notifications of a characteristic through its CCCD
static void subscribe(uint16_t cccd)
{
  uint8_t extra[8] = { (uint8_t)cccd, (uint8_t)(cccd >> 8), 0, 0, 2, 0, 0x01, 0x00 };
  vendor(0x0C01, extra, sizeof(extra));
  BLE_Step();
}

// Link encrypted, the stack then reporting `level` for it
static void encrypted(uint8_t level)
{
  static const uint8_t on[4] = { 0x00, 0x40, 0x00, 0x01 };
  uint8_t security[2] = { 0x01, level };
  WPAN_HostRespond(0, security, sizeof(security));
  event(0x08, on, sizeof(on));
  BLE_Step();
  TEST_Check(cmd() == 0xFC90 && plen == 2 && param[0] == 0x40, "level asked for the link");
}

//------------------------------------------------------------------------------------------- Tests

static void setup_bytes_at_start(void)
{
  instance(123456);
  init_responses(2);
  TEST_Check(BLE_Init(&ble) == OK);
  uint16_t opcode;
  while((opcode = cmd()) && opcode != 0xFD06) {} // start-up up to the GAP name
  TEST_Check(opcode == 0xFD06);
  static const uint8_t name[5] = { 0x01, 0x00, 0x03, 0x00, 0x08 };
  TEST_Check(cmd() == 0xFD28 && plen == sizeof(name));
  TEST_Bytes(param, name, sizeof(name), "name value written by an authenticated client only");
  TEST_Check(cmd() == 0xFC85 && plen == 1 && param[0] == 0x00, "display only");
  static const uint8_t auth[12] = { 1, 1, 2, 0, 16, 16, 1, 0, 0, 0, 0, 0 };
  TEST_Check(cmd() == 0xFC86 && plen == sizeof(auth));
  TEST_Bytes(param, auth, sizeof(auth), "bonding, MITM, SC only, full key, passkey by event");
  TEST_Check(cmd() == 0xFCA3 && plen == 0 && ble.bonds == 2, "bond count read at start");
  TEST_Check(cmd() == 0xFD02);
  for(uint8_t i = 0; i < array_len(chars); i++) {
    TEST_Check(cmd() == 0xFD04 && param[22] == 0x09 && param[24] == 16, "paired link, full key");
  }
  TEST_Check(cmd() == 0xFC83, "advertising as in the open mode");
}

static void setup_faults(void)
{
  instance(1000000);
  TEST_Check(BLE_Init(&ble) == ERR && ble.fault == BLE_Fault_Config, "passkey over six digits");
  TEST_Check(cmd() == 0, "refused before touching the CPU2");
  // The stack refuses the name permission: the pairing step and its status come back
  static const uint8_t gap[6] = { 0x01, 0x00, 0x02, 0x00, 0x03, 0x00 };
  instance(0);
  for(uint8_t i = 0; i < 5; i++) WPAN_HostRespond(0, NULL, 0);
  WPAN_HostRespond(0, gap, sizeof(gap));
  WPAN_HostRespond(0, NULL, 0);
  WPAN_HostRespond(0x12, NULL, 0);
  TEST_Check(BLE_Init(&ble) == ERR);
  TEST_Check(ble.fault == BLE_Fault_Security && ble.fault_status == 0x12);
}

static void security_level_opens_the_link(void)
{
  start(0);
  connect();
  TEST_Check(!BLE_IsConnected(&ble), "connected link unusable before encryption");
  subscribe(0x22);
  static const uint8_t data[4] = { 1, 2, 3, 4 };
  TEST_Check(BLE_Send(&ble, &chars[CH_TX], data, sizeof(data)) == ERR, "no push in clear");
  encrypted(2);
  TEST_Check(!BLE_IsConnected(&ble), "key without MITM does not open the link");
  TEST_Check(BLE_Send(&ble, &chars[CH_TX], data, sizeof(data)) == ERR);
  encrypted(4);
  TEST_Check(BLE_IsConnected(&ble), "authenticated LE Secure Connections key opens it");
  TEST_Check(BLE_Send(&ble, &chars[CH_TX], data, sizeof(data)) == OK);
  BLE_Step();
  TEST_Check(cmd() == 0xFD06 && param[5] == sizeof(data), "push goes out");
  static const uint8_t gone[4] = { 0x00, 0x40, 0x00, 0x13 };
  event(0x05, gone, sizeof(gone));
  BLE_Step();
  TEST_Check(!ble._secured && !BLE_IsConnected(&ble), "disconnection ends the session");
}

static void passkey_only_in_window(void)
{
  start(0);
  connect();
  TEST_Check(!BLE_IsPairing(&ble) && !passkey_asked(), "closed window gives no passkey");
  BLE_Pairing(&ble, 1000);
  TEST_Check(BLE_IsPairing(&ble) && passkey_asked(), "open window gives the passkey");
  pairing_complete(0);
  TEST_Check(cmd() == 0xFCA3, "bond count after the pairing");
  TEST_Check(ble.pairings == 1 && !BLE_IsPairing(&ble), "one new client per window");
  TEST_Check(!passkey_asked(), "window closed by the pairing");
  BLE_Pairing(&ble, 1000);
  TEST_Run(999);
  TEST_Check(BLE_IsPairing(&ble), "window open a tick before its end");
  TEST_Run(1);
  TEST_Check(!BLE_IsPairing(&ble) && !passkey_asked(), "window runs out");
  BLE_Pairing(&ble, 1000);
  BLE_Pairing(&ble, 0);
  TEST_Check(!BLE_IsPairing(&ble), "zero closes it");
}

static void failed_guesses_grow_the_delay(void)
{
  start(0);
  connect();
  BLE_Pairing(&ble, 60000);
  // A pairing failing before any passkey, Just Works refused for one, guesses nothing
  pairing_complete(0x02);
  TEST_Check(cmd() == 0 && ble.pair_fails == 0, "failure without passkey is no guess");
  TEST_Check(passkey_asked());
  pairing_complete(0x02);
  TEST_Check(cmd() == 0xFC93 && param[2] == 0x05, "failed guess drops the link");
  TEST_Check(ble.pair_fails == 1 && BLE_IsPairing(&ble), "window stays open");
  TEST_Check(!passkey_asked(), "no passkey during the delay");
  TEST_Run(BLE_PAIR_DELAY_ms - 1);
  TEST_Check(!passkey_asked(), "delay still running a tick before its end");
  TEST_Run(1);
  TEST_Check(passkey_asked(), "passkey again after the delay");
  pairing_complete(0x02);
  while(cmd()) {}
  TEST_Run(2 * BLE_PAIR_DELAY_ms - 1);
  TEST_Check(!passkey_asked(), "second failure doubles the delay");
  TEST_Run(1);
  TEST_Check(passkey_asked());
  // The delay stops growing at 512 times the first one
  BLE_Pairing(&ble, 1000 * BLE_PAIR_DELAY_ms);
  ble._pair_streak = 20;
  pairing_complete(0x02);
  while(cmd()) {}
  TEST_Run(512 * BLE_PAIR_DELAY_ms - 1);
  TEST_Check(!passkey_asked(), "capped delay still running");
  TEST_Run(1);
  TEST_Check(passkey_asked(), "capped delay over");
  // A successful pairing starts the doubling from the first delay again
  pairing_complete(0);
  while(cmd()) {}
  TEST_Check(ble.pair_fails == 3 && ble._pair_streak == 0, "failures counted, streak reset");
}

static void bond_limit_keeps_returning_clients(void)
{
  start(BLE_BOND_LIMIT);
  connect();
  BLE_Pairing(&ble, 60000);
  TEST_Check(!passkey_asked(), "full table takes no new client");
  TEST_Check(ble.pair_fails == 0 && BLE_IsPairing(&ble), "a refusal is no guess");
  // A bonded client that lost its keys pairs again into its own place
  vendor(0x0405, NULL, 0);
  BLE_Step();
  TEST_Check(cmd() == 0xFC95 && plen == 2 && param[0] == 0x40, "rebond allowed");
  TEST_Check(passkey_asked(), "client pairing again gets the passkey");
  static const uint8_t count[1] = { BLE_BOND_LIMIT };
  WPAN_HostRespond(0, count, sizeof(count));
  pairing_complete(0);
  TEST_Check(cmd() == 0xFCA3 && ble.bonds == BLE_BOND_LIMIT && !ble._rebond);
}

static void forget_drops_the_link_first(void)
{
  start(2);
  connect();
  BLE_Forget(&ble);
  BLE_Step();
  TEST_Check(cmd() == 0xFC93 && param[2] == 0x13, "link dropped before the table goes");
  TEST_Check(!BLE_Step() && cmd() == 0, "table waits for the disconnection");
  static const uint8_t gone[4] = { 0x00, 0x40, 0x00, 0x13 };
  event(0x05, gone, sizeof(gone));
  BLE_Step();
  TEST_Check(cmd() == 0xFC94 && ble.bonds == 0, "table cleared once the link is down");
  TEST_Check(cmd() == 0xFC83, "advertising restarts as after any disconnection");
  while(cmd()) {}
  // Without a client the table goes at once
  ble.bonds = 3;
  BLE_Forget(&ble);
  BLE_Step();
  TEST_Check(cmd() == 0xFC94 && ble.bonds == 0 && cmd() == 0);
}

void TEST_Cases(void)
{
  TEST(setup_bytes_at_start);
  TEST(setup_faults);
  TEST(security_level_opens_the_link);
  TEST(passkey_only_in_window);
  TEST(failed_guesses_grow_the_delay);
  TEST(bond_limit_keeps_returning_clients);
  TEST(forget_drops_the_link_first);
}

// hal/stm32wb/rf/ble_wb.c

#include "ble_wb.h"

#include <string.h>

#include "stm32wbxx.h"
#include "vrts.h"
#include "log.h"

//--------------------------------------------------------------------------------------- Constants

// HCI and ACI opcodes as the CPU2 stack defines them
#define HCI_RESET                     0x0C03
#define ACI_HAL_WRITE_CONFIG_DATA     0xFC0C
#define ACI_HAL_SET_TX_POWER_LEVEL    0xFC0F
#define ACI_GAP_SET_DISCOVERABLE      0xFC83
#define ACI_GAP_INIT                  0xFC8A
#define ACI_GAP_UPDATE_ADV_DATA       0xFC8E
#define ACI_GAP_DELETE_AD_TYPE        0xFC8F
#define ACI_GATT_INIT                 0xFD01
#define ACI_GATT_ADD_SERVICE          0xFD02
#define ACI_GATT_ADD_CHAR             0xFD04
#define ACI_GATT_UPDATE_CHAR_VALUE    0xFD06
#define ACI_GAP_TERMINATE             0xFC93
#define HCI_LE_SET_SCAN_RESPONSE_DATA 0x2009

// Vendor event codes of the CPU2 stack
#define ACI_EVT_ATTRIBUTE_MODIFIED    0x0C01
#define ACI_EVT_PROC_TIMEOUT          0x0C02
#define ACI_EVT_EXCHANGE_MTU          0x0C03
#define ACI_EVT_SERVER_CONFIRMATION   0x0C17

// Standard HCI event codes and the Bluetooth assigned numbers used below
#define HCI_EVT_DISCONNECTION         0x05
#define HCI_EVT_LE_META               0x3E
#define HCI_LE_CONNECTION_COMPLETE    0x01
#define HCI_LE_ENHANCED_CONNECTION    0x0A
#define HCI_LE_CONNECTION_SIZE        19
#define HCI_LE_ENHANCED_SIZE          31
#define AD_TYPE_UUID128_COMPLETE      0x07
#define AD_TYPE_LOCAL_NAME_SHORTENED  0x08
#define AD_TYPE_LOCAL_NAME_COMPLETE   0x09
#define AD_TYPE_TX_POWER              0x0A
#define GAP_ROLE_PERIPHERAL           0x01
#define ADV_TYPE_CONNECTABLE          0x00
#define UUID_TYPE_128                 0x02
#define SERVICE_PRIMARY               0x01
#define GATT_NOTIFY_ATTRIBUTE_WRITE   0x01
#define CCCD_SUBSCRIBED               0x0003 // notification or indication bit
#define CCCD_INDICATE                 0x0002 // indication alone, the confirmed push

// A refused push when the TX pool is momentarily full, retried by the loop.
#define BLE_STATUS_INSUFFICIENT_RESOURCES 0x64

#define BLE_CONN_NONE  0xFFFF
#define BLE_MTU_DEFAULT 23

// Pause before advertising again after the stack refused, a single refusal never ends it
#define BLE_ADV_RETRY_ms 1000

#if(BLE_PIN_ENABLE)
  // Pairing commands and events of the CPU2 stack
  #define ACI_GAP_SET_IO_CAPABILITY              0xFC85
  #define ACI_GAP_SET_AUTHENTICATION_REQUIREMENT 0xFC86
  #define ACI_GAP_PASS_KEY_RESP                  0xFC88
  #define ACI_GAP_PERIPHERAL_SECURITY_REQ        0xFC8D
  #define ACI_GAP_GET_SECURITY_LEVEL             0xFC90
  #define ACI_GAP_CLEAR_SECURITY_DB              0xFC94
  #define ACI_GAP_ALLOW_REBOND                   0xFC95
  #define ACI_GAP_GET_BONDED_DEVICES             0xFCA3
  #define ACI_GATT_SET_SECURITY_PERMISSION       0xFD28
  #define ACI_EVT_PAIRING_COMPLETE               0x0401
  #define ACI_EVT_PASS_KEY_REQ                   0x0402
  #define ACI_EVT_BOND_LOST                      0x0405
  #define HCI_EVT_ENCRYPTION_CHANGE              0x08
  #define HCI_AUTHENTICATION_FAILURE             0x05 // reason of a link dropped in pairing
  #define IO_CAP_DISPLAY_ONLY                    0x00 // passkey given by the device
  #define ATTR_PERMISSION_AUTHEN_READ            0x01
  #define ATTR_PERMISSION_AUTHEN_WRITE           0x08
  #define SECURITY_LEVEL_SC_MITM                 0x04 // authenticated LE Secure Connections
  // Characteristics need the paired link and the full key of LE Secure Connections
  #define BLE_CHAR_SECURITY (ATTR_PERMISSION_AUTHEN_READ | ATTR_PERMISSION_AUTHEN_WRITE)
  #define BLE_KEY_SIZE 16
#else
  #define BLE_CHAR_SECURITY 0 // open to any client
  #define BLE_KEY_SIZE 10
#endif

static BLE_t *self;

//----------------------------------------------------------------------------------------- Helpers

static inline uint16_t get16(const uint8_t *src) {
  return (uint16_t)(src[0] | (src[1] << 8));
}

static uint8_t *put16(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)value;
  dst[1] = (uint8_t)(value >> 8);
  return dst + 2;
}

// ACI takes UUIDs least significant byte first;
// the fourth textual group lands at bytes 6..7 and numbers the characteristic seen by the client.
static void ble_uuid(const uint8_t *text, uint16_t number, uint8_t *uuid)
{
  for(uint8_t i = 0; i < 16; i++) uuid[i] = text[15 - i];
  uuid[6] = (uint8_t)number;
  uuid[7] = (uint8_t)(number >> 8);
}

static inline bool ble_pushes(const BLE_Char_t *chr) {
  return chr->properties & (BLE_Prop_Notify | BLE_Prop_Indicate);
}

static BLE_Char_t *ble_find(BLE_t *ble, uint16_t attribute, uint16_t offset)
{
  for(uint8_t i = 0; i < ble->count; i++) {
    if(ble->chars[i]._handle + offset == attribute) return &ble->chars[i];
  }
  return NULL;
}

// Failed init step recorded for the application, the return feeds `BLE_Init` directly
static status_t ble_fault(BLE_t *ble, BLE_Fault_t fault, uint8_t status)
{
  ble->fault = fault;
  ble->fault_status = status;
  return ERR;
}

//---------------------------------------------------------------------------------------- Commands

static uint8_t gatt_add_service(const uint8_t *uuid, uint8_t records, uint16_t *handle)
{
  uint8_t cmd[19];
  uint8_t rsp[2];
  uint8_t rsp_len;
  cmd[0] = UUID_TYPE_128;
  memcpy(&cmd[1], uuid, 16);
  cmd[17] = SERVICE_PRIMARY;
  cmd[18] = records;
  uint8_t status = WPAN_BleCmd(ACI_GATT_ADD_SERVICE, cmd, sizeof(cmd),
    rsp, sizeof(rsp), &rsp_len);
  if(status) return status;
  if(rsp_len < sizeof(rsp)) return 0xFF; // no handle in the response
  *handle = get16(rsp);
  return 0;
}

static uint8_t gatt_add_char(uint16_t service, const uint8_t *uuid, const BLE_Char_t *chr,
  uint16_t *handle)
{
  uint8_t cmd[26];
  uint8_t rsp[2];
  uint8_t rsp_len;
  uint8_t *dst = put16(cmd, service);
  *dst++ = UUID_TYPE_128;
  memcpy(dst, uuid, 16);
  dst += 16;
  dst = put16(dst, chr->size);
  *dst++ = chr->properties;
  *dst++ = BLE_CHAR_SECURITY; // client access
  *dst++ = GATT_NOTIFY_ATTRIBUTE_WRITE; // client writes reported, descriptors included
  *dst++ = BLE_KEY_SIZE; // encryption key size
  *dst++ = 1; // variable length
  uint8_t status = WPAN_BleCmd(ACI_GATT_ADD_CHAR, cmd, sizeof(cmd),
    rsp, sizeof(rsp), &rsp_len);
  if(status) return status;
  if(rsp_len < sizeof(rsp)) return 0xFF; // no handle in the response
  *handle = get16(rsp);
  return 0;
}

static uint8_t gatt_update_char_value(uint16_t service, uint16_t char_handle,
  const uint8_t *data, uint8_t len)
{
  uint8_t cmd[6 + BLE_CHUNK_SIZE];
  uint8_t *dst = put16(cmd, service);
  dst = put16(dst, char_handle);
  *dst++ = 0; // value offset
  *dst++ = len;
  memcpy(dst, data, len);
  return WPAN_BleCmd(ACI_GATT_UPDATE_CHAR_VALUE, cmd, (uint8_t)(6 + len), NULL, 0, NULL);
}

// Advertising: the service UUID and as much of the name as `BLE_ADV_NAME_SIZE` allows,
// the TX power entry dropped to make room.
// A client filters on the service, so the UUID is what has to be in the advertisement;
// an active scanner can get the whole name from the scan response.
static uint8_t gap_advertise(BLE_t *ble)
{
  ble->adv_status = (BLE_AdvStatus_t){
    .start = 0xFF,
    .tx_power = 0xFF,
    .uuid = 0xFF,
    .scan_response = 0xFF
  };
  size_t full_len = strlen(ble->name);
  uint8_t name_len = full_len > BLE_NAME_SIZE ? BLE_NAME_SIZE : (uint8_t)full_len;
  uint8_t adv_len = name_len > BLE_ADV_NAME_SIZE ? BLE_ADV_NAME_SIZE : name_len;
  uint8_t cmd[14 + BLE_NAME_SIZE]; // 14 fixed bytes plus the advertised name
  uint8_t *dst = cmd;
  *dst++ = ADV_TYPE_CONNECTABLE;
  dst = put16(dst, BLE_ADV_INTERVAL_MIN);
  dst = put16(dst, BLE_ADV_INTERVAL_MAX);
  *dst++ = 0; // public address
  *dst++ = 0; // no white list
  *dst++ = (uint8_t)(adv_len + 1);
  *dst++ = adv_len < name_len ? AD_TYPE_LOCAL_NAME_SHORTENED : AD_TYPE_LOCAL_NAME_COMPLETE;
  memcpy(dst, ble->name, adv_len);
  dst += adv_len;
  *dst++ = 0; // no service UUID list here, appended below
  dst = put16(dst, 0); // no connection interval hint
  dst = put16(dst, 0);
  uint8_t status = WPAN_BleCmd(ACI_GAP_SET_DISCOVERABLE, cmd, (uint8_t)(dst - cmd),
    NULL, 0, NULL);
  ble->adv_status.start = status;
  if(status) return status;
  uint8_t drop = AD_TYPE_TX_POWER;
  ble->adv_status.tx_power = WPAN_BleCmd(ACI_GAP_DELETE_AD_TYPE, &drop, 1, NULL, 0, NULL);
  if(ble->adv_status.tx_power) ble->errors++;
  uint8_t uuid_ad[19];
  uuid_ad[0] = 18;
  uuid_ad[1] = 17;
  uuid_ad[2] = AD_TYPE_UUID128_COMPLETE;
  ble_uuid(ble->uuid, 0x0000, &uuid_ad[3]);
  // Keep advertising after an optional update fails; preserve each reply,
  // so the application can distinguish a missing UUID from a refused TX power deletion.
  ble->adv_status.uuid = WPAN_BleCmd(ACI_GAP_UPDATE_ADV_DATA, uuid_ad, sizeof(uuid_ad),
    NULL, 0, NULL);
  if(ble->adv_status.uuid) ble->errors++;
  uint8_t rsp[32] = { 0 }; // one length byte over the 31B the response carries
  rsp[0] = (uint8_t)(name_len + 2);
  rsp[1] = (uint8_t)(name_len + 1);
  rsp[2] = AD_TYPE_LOCAL_NAME_COMPLETE;
  memcpy(&rsp[3], ble->name, name_len);
  ble->adv_status.scan_response = WPAN_BleCmd(HCI_LE_SET_SCAN_RESPONSE_DATA, rsp, sizeof(rsp),
    NULL, 0, NULL);
  if(ble->adv_status.scan_response) ble->errors++;
  return 0;
}

//----------------------------------------------------------------------------------------- Pairing
#if(BLE_PIN_ENABLE)

// Drop the link with `reason`, the HCI error code the client receives
static void ble_drop(BLE_t *ble, uint8_t reason)
{
  uint8_t param[3];
  put16(param, ble->_conn);
  param[2] = reason;
  LOG_Debug("BLE link dropped, reason 0x%02X", reason);
  if(WPAN_BleCmd(ACI_GAP_TERMINATE, param, sizeof(param), NULL, 0, NULL)) ble->errors++;
}

// Bond count from the first byte of the answer, the addresses behind it are not needed
static uint8_t ble_bonds(BLE_t *ble)
{
  uint8_t count;
  uint8_t len;
  uint8_t status = WPAN_BleCmd(ACI_GAP_GET_BONDED_DEVICES, NULL, 0, &count, 1, &len);
  if(!status && len) ble->bonds = count;
  return status;
}

// Pairing set-up: device name written by paired clients only,
// passkey entry with MITM protection and the bond count for the limit.
static uint8_t ble_pin_init(BLE_t *ble, uint16_t gap_service, uint16_t gap_name)
{
  static const uint8_t auth[12] = {
    1,           // bonding: keys kept in the CPU2 flash
    1,           // MITM mandatory, Just Works refused
    2,           // LE Secure Connections only
    0,           // no keypress notifications
    16,          // encryption key size, minimum
    16,          // and maximum
    1,           // passkey asked through `ACI_GAP_PASS_KEY_REQ`, no fixed pin
    0, 0, 0, 0,  // fixed pin, unused
    0            // public identity address
  };
  uint8_t name[5];
  uint8_t *dst = put16(name, gap_service);
  dst = put16(dst, gap_name + 1); // value attribute of the name characteristic
  *dst = ATTR_PERMISSION_AUTHEN_WRITE;
  uint8_t status = WPAN_BleCmd(ACI_GATT_SET_SECURITY_PERMISSION, name, sizeof(name),
    NULL, 0, NULL);
  if(status) return status;
  uint8_t io = IO_CAP_DISPLAY_ONLY;
  if((status = WPAN_BleCmd(ACI_GAP_SET_IO_CAPABILITY, &io, 1, NULL, 0, NULL))) return status;
  status = WPAN_BleCmd(ACI_GAP_SET_AUTHENTICATION_REQUIREMENT, auth, sizeof(auth),
    NULL, 0, NULL);
  if(status) return status;
  return ble_bonds(ble);
}

// Ask the client for security at once: it pairs, or encrypts with the bonded key
static void ble_secure(BLE_t *ble)
{
  uint8_t param[2];
  put16(param, ble->_conn);
  uint8_t status = WPAN_BleCmd(ACI_GAP_PERIPHERAL_SECURITY_REQ, param, sizeof(param),
    NULL, 0, NULL);
  LOG_Debug("BLE security request, status 0x%02X", status);
  if(status) ble->errors++;
}

void BLE_Pairing(BLE_t *ble, uint32_t window_ms)
{
  ble->_pair_end = window_ms ? tick_keep(window_ms) : 0;
}

bool BLE_IsPairing(BLE_t *ble)
{
  return tick_away(&ble->_pair_end);
}

void BLE_Forget(BLE_t *ble)
{
  ble->_forget = 1; // `BLE_Step` drops the link, then clears the table
}

#endif
//------------------------------------------------------------------------------------------ Events

static void client_write(BLE_t *ble, BLE_Char_t *chr, const uint8_t *data, uint16_t len,
  bool last)
{
  if(chr->buff) {
    for(uint16_t i = 0; i < len; i++) {
      // Console mode drops escape sequences on purpose, only a full ring is an overflow
      if(!BUFF_Push(chr->buff, data[i]) && !chr->buff->console_mode) ble->overflow++;
    }
    if(last) BUFF_Break(chr->buff);
  }
  if(chr->OnWrite) chr->OnWrite(data, len);
}

static void ble_vendor_event(BLE_t *ble, const uint8_t *payload, uint8_t plen)
{
  uint16_t code = get16(payload);
  BLE_Char_t *chr;
  switch(code) {
    case ACI_EVT_ATTRIBUTE_MODIFIED: {
      if(plen < 10) break; // truncated event
      uint16_t attribute = get16(payload + 4);
      uint16_t offset = get16(payload + 6);
      uint16_t len = get16(payload + 8);
      if(10u + len > plen) break; // data cut short
      // Value attribute sits one handle past the characteristic, its descriptor two.
      if((chr = ble_find(ble, attribute, 1))) {
        client_write(ble, chr, payload + 10, len, !(offset & 0x8000)); // bit 15 = more to come
      }
      else if((chr = ble_find(ble, attribute, 2)) && len >= 2) {
        chr->_subscribed = (uint8_t)(get16(payload + 10) & CCCD_SUBSCRIBED);
        if(!chr->_subscribed && ble->_tx_busy && ble->_tx == chr) { // push lost its receiver
          ble->_tx_busy = false;
          ble->_tx_wait = false;
          ble->errors++;
        }
      }
      break;
    }
    case ACI_EVT_PROC_TIMEOUT: {
      if(plen < 4) break;
      // The GATT channel is dead by specification, only a disconnect starts it clean
      uint8_t param[3];
      put16(param, ble->_conn);
      param[2] = 0x13; // remote user terminated connection
      (void)WPAN_BleCmd(ACI_GAP_TERMINATE, param, sizeof(param), NULL, 0, NULL);
      ble->errors++;
      break;
    }
    case ACI_EVT_EXCHANGE_MTU: {
      if(plen < 6) break;
      uint16_t mtu = get16(payload + 4);
      // The spec floor is 23; anything under it would zero the push chunks below
      if(mtu >= BLE_MTU_DEFAULT) ble->_mtu = mtu;
      break;
    }
    case ACI_EVT_SERVER_CONFIRMATION:
      if(plen < 4) break;
      ble->_tx_wait = false;
      if(ble->_tx_pos >= ble->_tx_len) ble->_tx_busy = false;
      break;
    #if(BLE_PIN_ENABLE)
    case ACI_EVT_PASS_KEY_REQ: {
      if(plen < 4) break;
      // Each failed attempt gives away the passkey bits up to the round that failed,
      // so outside the window and during the delay the link drops instead.
      bool window = tick_away(&ble->_pair_end);
      bool held = tick_away(&ble->_pair_hold);
      bool refused = !window || held;
      // A full table takes no new client, a client pairing again keeps its place
      if(ble->bonds >= BLE_BOND_LIMIT && !ble->_rebond) refused = true;
      LOG_Debug("BLE passkey request %s: window %u, delay %u, bonds %u/%u",
        refused ? "refused" : "answered", window, held, ble->bonds, BLE_BOND_LIMIT);
      if(refused) {
        ble_drop(ble, HCI_AUTHENTICATION_FAILURE);
        break;
      }
      uint8_t param[6];
      uint8_t *dst = put16(param, get16(payload + 2));
      put16(put16(dst, (uint16_t)ble->pin), (uint16_t)(ble->pin >> 16));
      ble->_pin_given = true;
      if(WPAN_BleCmd(ACI_GAP_PASS_KEY_RESP, param, sizeof(param), NULL, 0, NULL)) ble->errors++;
      break;
    }
    case ACI_EVT_BOND_LOST: {
      if(plen < 4) break;
      uint8_t param[2];
      put16(param, get16(payload + 2));
      ble->_rebond = true;
      LOG_Debug("BLE bond lost, pairing again allowed");
      if(WPAN_BleCmd(ACI_GAP_ALLOW_REBOND, param, sizeof(param), NULL, 0, NULL)) ble->errors++;
      break;
    }
    case ACI_EVT_PAIRING_COMPLETE: {
      if(plen < 6) break;
      bool guessed = ble->_pin_given; // a refused method gives nothing away
      LOG_Debug("BLE pairing complete, status 0x%02X reason 0x%02X", payload[4], payload[5]);
      ble->_pin_given = false;
      ble->_rebond = false;
      if(!payload[4]) {
        ble->_pair_streak = 0;
        ble->_pair_end = 0; // one new client per window
        if(ble_bonds(ble)) ble->errors++;
        ble->pairings++; // last, so whoever sees the count finds `bonds` current
      }
      else if(guessed) {
        ble->pair_fails++;
        // Nine doublings cap the delay at 512 × `BLE_PAIR_DELAY_ms`
        ble->_pair_hold = tick_keep(BLE_PAIR_DELAY_ms << minv(ble->_pair_streak, 9));
        ble->_pair_streak++;
        ble_drop(ble, HCI_AUTHENTICATION_FAILURE);
      }
      break;
    }
    #endif
    default:
      LOG_Debug("BLE vendor event 0x%04X", code);
      break;
  }
}

static void ble_event(BLE_t *ble, const uint8_t *event)
{
  uint8_t code = event[1];
  uint8_t plen = event[2];
  const uint8_t *payload = event + 3;
  switch(code) {
    case HCI_EVT_DISCONNECTION:
      if(plen < 4) break;
      ble->_connected = false;
      ble->_conn = BLE_CONN_NONE;
      ble->_advertise = true;
      ble->resets++;
      LOG_Debug("BLE disconnected, reason 0x%02X", payload[3]);
      for(uint8_t i = 0; i < ble->count; i++) ble->chars[i]._subscribed = false;
      if(ble->_tx_busy && ble_pushes(ble->_tx)) { // push lost its receiver, updates go on
        ble->_tx_busy = false;
        ble->errors++;
      }
      #if(BLE_PIN_ENABLE)
      ble->_secured = false;
      ble->_pin_given = false;
      ble->_rebond = false;
      #endif
      break;
    case HCI_EVT_LE_META:
      if(!plen) break;
      if(payload[0] == HCI_LE_CONNECTION_COMPLETE) {
        if(plen < HCI_LE_CONNECTION_SIZE) break;
        if(payload[1]) break; // failed connection attempt
        ble->_conn = get16(payload + 2);
        ble->_mtu = BLE_MTU_DEFAULT;
        ble->_connected = true;
        LOG_Debug("BLE connected, handle %u", ble->_conn);
        #if(BLE_PIN_ENABLE)
        ble_secure(ble);
        #endif
      }
      else if(payload[0] == HCI_LE_ENHANCED_CONNECTION) {
        if(plen < HCI_LE_ENHANCED_SIZE) break;
        if(payload[1]) break; // failed connection attempt
        ble->_conn = get16(payload + 2);
        ble->_mtu = BLE_MTU_DEFAULT;
        ble->_connected = true;
        LOG_Debug("BLE connected, handle %u", ble->_conn);
        #if(BLE_PIN_ENABLE)
        ble_secure(ble);
        #endif
      }
      else LOG_Debug("BLE LE event 0x%02X", payload[0]);
      break;
    #if(BLE_PIN_ENABLE)
    case HCI_EVT_ENCRYPTION_CHANGE: {
      if(plen < 4) break;
      ble->_secured = false;
      LOG_Debug("BLE encryption, status 0x%02X enabled %u", payload[0], payload[3]);
      if(payload[0] || !payload[3]) break; // failed, or encryption switched off
      // Encryption alone is not enough, the key has to come from a pairing with the passkey
      uint8_t param[2];
      uint8_t level[2];
      uint8_t len;
      put16(param, get16(payload + 1));
      if(!WPAN_BleCmd(ACI_GAP_GET_SECURITY_LEVEL, param, sizeof(param),
        level, sizeof(level), &len) && len == sizeof(level)) {
        ble->_secured = level[1] == SECURITY_LEVEL_SC_MITM;
        LOG_Debug("BLE security mode %u level %u", level[0], level[1]);
      }
      break;
    }
    #endif
    case 0xFF: // vendor event, the code in the first payload halfword
      if(plen >= 2) ble_vendor_event(ble, payload, plen);
      break;
    default:
      LOG_Debug("BLE event 0x%02X", code);
      break;
  }
}

//-------------------------------------------------------------------------------------------- Init

status_t BLE_Init(BLE_t *ble)
{
  uint8_t status;
  if(!ble->name || !ble->chars || !ble->count) return ble_fault(ble, BLE_Fault_Config, 0);
  #if(BLE_PIN_ENABLE)
  if(ble->pin > 999999) return ble_fault(ble, BLE_Fault_Config, 0); // six digits
  #endif
  // Sizing for the CPU2: its own GAP and GATT records come on top of the declared ones.
  uint32_t attributes = 9;
  uint32_t records = 1;
  uint32_t values = 64; // GAP name and appearance storage
  for(uint8_t i = 0; i < ble->count; i++) {
    BLE_Char_t *chr = &ble->chars[i];
    if(!chr->number || !chr->size || chr->size > BLE_CHUNK_SIZE) {
      return ble_fault(ble, BLE_Fault_Config, 0);
    }
    if(chr->buff) BUFF_Init(chr->buff);
    records += 2 + (ble_pushes(chr) ? 1 : 0);
    values += 19 + chr->size + (ble_pushes(chr) ? 2 : 0);
  }
  attributes += records - 1;
  if(records > UINT8_MAX || attributes > UINT16_MAX || values > UINT16_MAX) {
    return ble_fault(ble, BLE_Fault_Config, 0);
  }
  ble->_conn = BLE_CONN_NONE;
  ble->_mtu = BLE_MTU_DEFAULT;
  if(WPAN_Start(ble->irq_priority)) {
    return ble_fault(ble, BLE_Fault_Start, (uint8_t)WPAN_Firmware());
  }
  if((status = WPAN_BleStackInit((uint16_t)attributes, 3, (uint16_t)values))) {
    return ble_fault(ble, BLE_Fault_StackInit, status);
  }
  if((status = WPAN_BleCmd(HCI_RESET, NULL, 0, NULL, 0, NULL))) {
    return ble_fault(ble, BLE_Fault_Reset, status);
  }
  // Public address derived from the device unique number, the way ST examples wire it.
  uint32_t udn = *(const uint32_t *)UID64_BASE;
  uint32_t company = *(const uint32_t *)(UID64_BASE + 4);
  uint8_t cfg[8] = { 0x00, 6 }; // config data offset, length
  cfg[2] = (uint8_t)udn;
  cfg[3] = (uint8_t)(udn >> 8);
  cfg[4] = (uint8_t)company; // device ID byte
  cfg[5] = (uint8_t)(company >> 8);
  cfg[6] = (uint8_t)(company >> 16);
  cfg[7] = (uint8_t)(company >> 24);
  if((status = WPAN_BleCmd(ACI_HAL_WRITE_CONFIG_DATA, cfg, sizeof(cfg), NULL, 0, NULL))) {
    return ble_fault(ble, BLE_Fault_Address, status);
  }
  uint8_t power[2] = { 1, BLE_TX_POWER };
  if((status = WPAN_BleCmd(ACI_HAL_SET_TX_POWER_LEVEL, power, sizeof(power), NULL, 0, NULL))) {
    return ble_fault(ble, BLE_Fault_Power, status);
  }
  if((status = WPAN_BleCmd(ACI_GATT_INIT, NULL, 0, NULL, 0, NULL))) {
    return ble_fault(ble, BLE_Fault_GattInit, status);
  }
  size_t full_len = strlen(ble->name);
  uint8_t name_len = full_len > BLE_NAME_SIZE ? BLE_NAME_SIZE : (uint8_t)full_len;
  uint8_t gap[3] = { GAP_ROLE_PERIPHERAL, 0, name_len }; // no privacy
  uint8_t handles[6];
  uint8_t rsp_len = 0;
  if((status = WPAN_BleCmd(ACI_GAP_INIT, gap, sizeof(gap),
    handles, sizeof(handles), &rsp_len))) {
    return ble_fault(ble, BLE_Fault_GapInit, status);
  }
  if(rsp_len < sizeof(handles)) return ble_fault(ble, BLE_Fault_GapInit, 0xFF);
  uint16_t gap_service = get16(handles);
  uint16_t gap_name = get16(handles + 2);
  status = gatt_update_char_value(gap_service, gap_name, (const uint8_t *)ble->name, name_len);
  if(status) return ble_fault(ble, BLE_Fault_Name, status);
  #if(BLE_PIN_ENABLE)
  if((status = ble_pin_init(ble, gap_service, gap_name))) {
    return ble_fault(ble, BLE_Fault_Security, status);
  }
  #endif
  uint8_t uuid[16];
  ble_uuid(ble->uuid, 0x0000, uuid);
  if((status = gatt_add_service(uuid, (uint8_t)records, &ble->_service))) {
    return ble_fault(ble, BLE_Fault_Service, status);
  }
  for(uint8_t i = 0; i < ble->count; i++) {
    BLE_Char_t *chr = &ble->chars[i];
    ble_uuid(ble->uuid, chr->number, uuid);
    if((status = gatt_add_char(ble->_service, uuid, chr, &chr->_handle))) {
      return ble_fault(ble, BLE_Fault_Char, status);
    }
  }
  if((status = gap_advertise(ble))) return ble_fault(ble, BLE_Fault_Advertise, status);
  self = ble;
  ble->_init = true;
  return OK;
}

//-------------------------------------------------------------------------------------------- Loop

static void tx_step(BLE_t *ble)
{
  BLE_Char_t *chr = ble->_tx;
  if(!ble->_tx_busy || ble->_tx_wait) return;
  if(!ble_pushes(chr)) {
    // Value update for reads: no client involved, one chunk, retried on a full pool
    uint8_t status = gatt_update_char_value(ble->_service, chr->_handle,
      ble->_tx_data, (uint8_t)ble->_tx_len);
    if(status == BLE_STATUS_INSUFFICIENT_RESOURCES) return;
    if(status) ble->errors++;
    ble->_tx_busy = false;
    return;
  }
  if(!ble->_connected || !chr->_subscribed) return;
  #if(BLE_PIN_ENABLE)
  if(!ble->_secured) return; // pushes go over the paired link only
  #endif
  uint16_t chunk = ble->_mtu - 3;
  if(chunk > chr->size) chunk = chr->size;
  uint16_t left = ble->_tx_len - ble->_tx_pos;
  if(chunk > left) chunk = left;
  // Confirmation of an indication may beat the command status, so arm the wait first;
  // a client that only enabled notifications never confirms.
  ble->_tx_wait = (chr->_subscribed & CCCD_INDICATE) != 0;
  uint8_t status = gatt_update_char_value(ble->_service, chr->_handle,
    ble->_tx_data + ble->_tx_pos, (uint8_t)chunk);
  if(!status) {
    ble->_tx_pos += chunk;
    if(!ble->_tx_wait && ble->_tx_pos >= ble->_tx_len) ble->_tx_busy = false;
    return;
  }
  ble->_tx_wait = false;
  if(status != BLE_STATUS_INSUFFICIENT_RESOURCES) { // full TX pool retries, the rest gives up
    ble->_tx_busy = false;
    ble->errors++;
  }
}

bool BLE_Step(void)
{
  BLE_t *ble = self;
  if(!ble || !ble->_init) return false;
  bool work = false;
  uint8_t *event;
  while((event = WPAN_BleEvent())) {
    ble_event(ble, event);
    WPAN_BleEventDone(event);
    work = true;
  }
  #if(BLE_PIN_ENABLE)
  // Bonds go once the link is down, so no session runs on deleted keys
  if(ble->_forget == 1 && ble->_connected) {
    ble_drop(ble, 0x13); // remote user terminated connection
    ble->_forget = 2;
    work = true;
  }
  else if(ble->_forget && !ble->_connected) {
    if(WPAN_BleCmd(ACI_GAP_CLEAR_SECURITY_DB, NULL, 0, NULL, 0, NULL)) ble->errors++;
    else ble->bonds = 0;
    ble->_forget = 0;
    work = true;
  }
  #endif
  if(ble->_advertise && !ble->_connected && !tick_away(&ble->_adv_retry)) {
    if(gap_advertise(ble)) {
      ble->errors++;
      ble->_adv_retry = tick_keep(BLE_ADV_RETRY_ms);
    }
    else ble->_advertise = false;
    work = true;
  }
  tx_step(ble);
  return work;
}

void BLE_Loop(void)
{
  while(1) {
    BLE_Step();
    let();
  }
}

//-------------------------------------------------------------------------------------------- Link

bool BLE_IsConnected(BLE_t *ble)
{
  #if(BLE_PIN_ENABLE)
  return ble->_init && ble->_connected && ble->_secured;
  #else
  return ble->_init && ble->_connected;
  #endif
}

void BLE_Sleep(BLE_t *ble)
{
  ble->_init = false; // the reset takes the GATT database along, `BLE_Init` cannot run twice
  if(WPAN_Firmware() != WPAN_Fw_Wireless) return; // no radio to park
  (void)WPAN_BleCmd(HCI_RESET, NULL, 0, NULL, 0, NULL);
  ble->_conn = BLE_CONN_NONE;
}

bool BLE_Subscribed(BLE_Char_t *chr)
{
  return chr->_subscribed != 0;
}

uint16_t BLE_Mtu(BLE_t *ble)
{
  return ble->_mtu;
}

status_t BLE_Send(BLE_t *ble, BLE_Char_t *chr, const uint8_t *data, uint16_t len)
{
  if(!ble->_init || !len) return ERR;
  if(ble->_tx_busy) return BUSY;
  if(ble_pushes(chr)) {
    if(!ble->_connected || !chr->_subscribed) return ERR;
    #if(BLE_PIN_ENABLE)
    if(!ble->_secured) return ERR; // pushes go over the paired link only
    #endif
  }
  else if(len > chr->size) return ERR; // value only waits in the database for a read
  ble->_tx = chr;
  ble->_tx_data = data;
  ble->_tx_len = len;
  ble->_tx_pos = 0;
  ble->_tx_wait = false;
  ble->_tx_busy = true; // hands the transfer to `BLE_Loop`
  return OK;
}

bool BLE_IsBusy(BLE_t *ble)
{
  return ble->_tx_busy;
}

bool BLE_IsFree(BLE_t *ble)
{
  return !ble->_tx_busy;
}

uint16_t BLE_Size(BLE_Char_t *chr)
{
  return chr->buff ? BUFF_Size(chr->buff) : 0;
}

uint16_t BLE_MessageCount(BLE_Char_t *chr)
{
  return chr->buff ? BUFF_MessageCount(chr->buff) : 0;
}

uint16_t BLE_Read(BLE_Char_t *chr, uint8_t *data)
{
  return chr->buff ? BUFF_Read(chr->buff, data) : 0;
}

char *BLE_ReadString(BLE_Char_t *chr)
{
  return chr->buff ? BUFF_ReadString(chr->buff) : NULL;
}

bool BLE_Skip(BLE_Char_t *chr)
{
  return chr->buff ? BUFF_Skip(chr->buff) : false;
}

void BLE_Clear(BLE_Char_t *chr)
{
  if(chr->buff) BUFF_Clear(chr->buff);
}

//-------------------------------------------------------------------------------------------------

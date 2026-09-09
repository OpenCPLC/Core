// hal/stm32wb/rf/ble_wb.c

#include "ble_wb.h"

#include <string.h>

#include "stm32wbxx.h"
#include "vrts.h"

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

// ACI takes UUIDs least significant byte first; the fourth textual group lands
// at bytes 6..7 and numbers the characteristic seen by the client.
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
  uint8_t status = WPAN_BleCmd(ACI_GATT_ADD_SERVICE, cmd, sizeof(cmd), rsp, sizeof(rsp),
    &rsp_len);
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
  *dst++ = 0; // no security
  *dst++ = GATT_NOTIFY_ATTRIBUTE_WRITE; // client writes reported, descriptors included
  *dst++ = 10; // encryption key size
  *dst++ = 1; // variable length
  uint8_t status = WPAN_BleCmd(ACI_GATT_ADD_CHAR, cmd, sizeof(cmd), rsp, sizeof(rsp),
    &rsp_len);
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

// Advertising: flags and complete name go through the GAP command, the TX power entry
// is dropped to make room and the service UUID entry is appended when it still fits.
static uint8_t gap_advertise(BLE_t *ble)
{
  uint8_t name_len = (uint8_t)strlen(ble->name);
  if(name_len > BLE_NAME_SIZE) name_len = BLE_NAME_SIZE;
  uint8_t cmd[14 + BLE_NAME_SIZE]; // 14 fixed bytes plus the advertised name
  uint8_t *dst = cmd;
  *dst++ = ADV_TYPE_CONNECTABLE;
  dst = put16(dst, BLE_ADV_INTERVAL_MIN);
  dst = put16(dst, BLE_ADV_INTERVAL_MAX);
  *dst++ = 0; // public address
  *dst++ = 0; // no white list
  *dst++ = (uint8_t)(name_len + 1);
  *dst++ = AD_TYPE_LOCAL_NAME_COMPLETE;
  memcpy(dst, ble->name, name_len);
  dst += name_len;
  *dst++ = 0; // no service UUID list here, appended below when it fits
  dst = put16(dst, 0); // no connection interval hint
  dst = put16(dst, 0);
  uint8_t status = WPAN_BleCmd(ACI_GAP_SET_DISCOVERABLE, cmd, (uint8_t)(dst - cmd),
    NULL, 0, NULL);
  if(status) return status;
  uint8_t drop = AD_TYPE_TX_POWER;
  (void)WPAN_BleCmd(ACI_GAP_DELETE_AD_TYPE, &drop, 1, NULL, 0, NULL);
  uint8_t uuid_ad[19];
  uuid_ad[0] = 18;
  uuid_ad[1] = 17;
  uuid_ad[2] = AD_TYPE_UUID128_COMPLETE;
  ble_uuid(ble->uuid, 0x0000, &uuid_ad[3]);
  // 31B of advertising is tight: a long name leaves no room and the entry is skipped.
  (void)WPAN_BleCmd(ACI_GAP_UPDATE_ADV_DATA, uuid_ad, sizeof(uuid_ad), NULL, 0, NULL);
  return 0;
}

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
    default:
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
      for(uint8_t i = 0; i < ble->count; i++) ble->chars[i]._subscribed = false;
      if(ble->_tx_busy && ble_pushes(ble->_tx)) { // push lost its receiver, updates go on
        ble->_tx_busy = false;
        ble->errors++;
      }
      break;
    case HCI_EVT_LE_META:
      if(!plen) break;
      if(payload[0] == HCI_LE_CONNECTION_COMPLETE) {
        if(plen < HCI_LE_CONNECTION_SIZE) break;
        if(payload[1]) break; // failed connection attempt
        ble->_conn = get16(payload + 2);
        ble->_mtu = BLE_MTU_DEFAULT;
        ble->_connected = true;
      }
      else if(payload[0] == HCI_LE_ENHANCED_CONNECTION) {
        if(plen < HCI_LE_ENHANCED_SIZE) break;
        if(payload[1]) break; // failed connection attempt
        ble->_conn = get16(payload + 2);
        ble->_mtu = BLE_MTU_DEFAULT;
        ble->_connected = true;
      }
      break;
    case 0xFF: // vendor event, the code in the first payload halfword
      if(plen >= 2) ble_vendor_event(ble, payload, plen);
      break;
    default:
      break;
  }
}

//-------------------------------------------------------------------------------------------- Init

status_t BLE_Init(BLE_t *ble)
{
  uint8_t status;
  if(!ble->name || !ble->chars || !ble->count) return ble_fault(ble, BLE_Fault_Config, 0);
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
  uint8_t name_len = (uint8_t)strlen(ble->name);
  if(name_len > BLE_NAME_SIZE) name_len = BLE_NAME_SIZE;
  uint8_t gap[3] = { GAP_ROLE_PERIPHERAL, 0, name_len }; // no privacy
  uint8_t handles[6];
  uint8_t rsp_len = 0;
  if((status = WPAN_BleCmd(ACI_GAP_INIT, gap, sizeof(gap), handles, sizeof(handles),
    &rsp_len))) {
    return ble_fault(ble, BLE_Fault_GapInit, status);
  }
  if(rsp_len < sizeof(handles)) return ble_fault(ble, BLE_Fault_GapInit, 0xFF);
  uint16_t gap_service = get16(handles);
  uint16_t gap_name = get16(handles + 2);
  status = gatt_update_char_value(gap_service, gap_name, (const uint8_t *)ble->name, name_len);
  if(status) return ble_fault(ble, BLE_Fault_Name, status);
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
    uint8_t status = gatt_update_char_value(ble->_service, chr->_handle, ble->_tx_data,
      (uint8_t)ble->_tx_len);
    if(status == BLE_STATUS_INSUFFICIENT_RESOURCES) return;
    if(status) ble->errors++;
    ble->_tx_busy = false;
    return;
  }
  if(!ble->_connected || !chr->_subscribed) return;
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
  if(ble->_advertise && !ble->_connected) {
    ble->_advertise = false;
    if(gap_advertise(ble)) ble->errors++;
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
  return ble->_init && ble->_connected;
}

void BLE_Sleep(BLE_t *ble)
{
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

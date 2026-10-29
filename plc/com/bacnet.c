// plc/com/bacnet.c

#include "bacnet.h"

#include <string.h>

#include "bacnet_tag.h"
#include "crc.h"
#include "heap.h"
#include "vrts.h"

//------------------------------------------------------------------------------------------ Config

// Revision 12 of 135-2010, the last before `Property_List`
#define BACNET_PROTOCOL_VERSION 1
#define BACNET_PROTOCOL_REVISION 12

// Largest APDU of MS/TP [B], no segmentation
#define BACNET_APDU_MAX 480

// Wait for an answer the device reports, and no retries:
// a request is repeated by the application, not by the client.
#define BACNET_APDU_TIMEOUT_ms 3000
#define BACNET_APDU_RETRIES 0

// No instance: a Who-Is limit past it is invalid,
// and Device of this instance in ReadProperty is the device read,
// so a client learns its instance.
#define BACNET_WILDCARD 4194303

// MS/TP limits the Device object reports, the link keeps to them
#define BACNET_MAX_MASTER 127
#define BACNET_MAX_INFO_FRAMES 1

// Enumerations of Device: operational, no segmentation
#define BACNET_SYSTEM_STATUS 0
#define BACNET_SEGMENTATION 3

//------------------------------------------------------------------------------------------- Types

typedef enum {
  BACNET_PDU_ConfirmedRequest = 0,
  BACNET_PDU_UnconfirmedRequest = 1,
  BACNET_PDU_SimpleACK = 2,
  BACNET_PDU_ComplexACK = 3,
  BACNET_PDU_Error = 5,
  BACNET_PDU_Reject = 6,
  BACNET_PDU_Abort = 7,
} BACNET_PDU_t;

// Service choices, unconfirmed I-Am and Who-Is, confirmed ReadProperty and WriteProperty
typedef enum {
  BACNET_Service_IAm = 0,
  BACNET_Service_WhoIs = 8,
  BACNET_Service_ReadProperty = 12,
  BACNET_Service_WriteProperty = 15,
} BACNET_Service_t;

typedef enum {
  BACNET_Prop_APDUTimeout = 11,
  BACNET_Prop_ApplicationSoftwareVersion = 12,
  BACNET_Prop_DeviceAddressBinding = 30,
  BACNET_Prop_EventState = 36,
  BACNET_Prop_FirmwareRevision = 44,
  BACNET_Prop_MaxAPDULengthAccepted = 62,
  BACNET_Prop_MaxInfoFrames = 63,
  BACNET_Prop_MaxMaster = 64,
  BACNET_Prop_ModelName = 70,
  BACNET_Prop_NumberOfAPDURetries = 73,
  BACNET_Prop_ObjectIdentifier = 75,
  BACNET_Prop_ObjectList = 76,
  BACNET_Prop_ObjectName = 77,
  BACNET_Prop_ObjectType = 79,
  BACNET_Prop_OutOfService = 81,
  BACNET_Prop_Polarity = 84,
  BACNET_Prop_PresentValue = 85,
  BACNET_Prop_ProtocolObjectTypesSupported = 96,
  BACNET_Prop_ProtocolServicesSupported = 97,
  BACNET_Prop_ProtocolVersion = 98,
  BACNET_Prop_SegmentationSupported = 107,
  BACNET_Prop_StatusFlags = 111,
  BACNET_Prop_SystemStatus = 112,
  BACNET_Prop_Units = 117,
  BACNET_Prop_VendorIdentifier = 120,
  BACNET_Prop_VendorName = 121,
  BACNET_Prop_ProtocolRevision = 139,
  BACNET_Prop_DatabaseRevision = 155,
} BACNET_Prop_t;

// Error class in the high byte, OBJECT `1` or PROPERTY `2`, and error code in the low one
typedef enum {
  BACNET_Error_None = 0,
  BACNET_Error_InvalidDataType = (2 << 8) | 9,
  BACNET_Error_UnknownObject = (1 << 8) | 31,
  BACNET_Error_UnknownProperty = (2 << 8) | 32,
  BACNET_Error_ValueOutOfRange = (2 << 8) | 37,
  BACNET_Error_WriteAccessDenied = (2 << 8) | 40,
  BACNET_Error_InvalidArrayIndex = (2 << 8) | 42,
  BACNET_Error_NotAnArray = (2 << 8) | 50,
} BACNET_Error_t;

// Reject reasons, `0` stands for none here: the reason "other" is never sent
typedef enum {
  BACNET_Reject_None = 0,
  BACNET_Reject_InvalidTag = 4,
  BACNET_Reject_MissingRequiredParameter = 5,
  BACNET_Reject_ParameterOutOfRange = 6,
  BACNET_Reject_TooManyArguments = 7,
  BACNET_Reject_UnrecognizedService = 9,
} BACNET_Reject_t;

#define BACNET_ABORT_SEGMENTATION_NOT_SUPPORTED 4

// Request being served: where its answer goes and how much of one the client takes
typedef struct {
  bool global;        // global broadcast, to every network
  bool routed;        // through a router, the client on network `snet`
  uint16_t snet;
  uint8_t slen;
  uint8_t sadr[8];    // client address on its network
  uint8_t priority;   // network priority, bits 0–1 of the control octet
  uint8_t invoke;
  uint8_t service;
  uint16_t max_apdu;  // longest APDU the client takes [B]
} BACNET_Request_t;

//------------------------------------------------------------------------------------------ Tables

// Services executed, bits 12, 15 and 34 of the 40 of revision 12:
// ReadProperty, WriteProperty, Who-Is; I-Am is only sent, so it has no bit.
static const uint8_t services[] = { 0x00, 0x09, 0x00, 0x00, 0x20 };

// Object types, bits 0, 2, 3, 5 and 8 of the 51 of revision 12:
// Analog Input, Analog Value, Binary Input, Binary Value, Device.
static const uint8_t object_types[] = { 0xB4, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00 };

// `Status_Flags`: in alarm, fault, overridden, out of service, all clear
static const uint8_t status_flags[] = { 0x00 };

//----------------------------------------------------------------------------------------- Objects

static BACNET_Object_t *find(BACNET_t *bacnet, uint32_t type, uint32_t instance)
{
  for(uint16_t i = 0; i < bacnet->object_count; i++) {
    BACNET_Object_t *object = &bacnet->objects[i];
    if(object->type == type && object->instance == instance) return object;
  }
  return NULL;
}

static bool analog(const BACNET_Object_t *object) {
  return object->type == BACNET_Type_AnalogInput || object->type == BACNET_Type_AnalogValue;
}

// `Object_List`: Device first, then the table; index `0` gives the element count
static BACNET_Error_t object_list(const BACNET_t *bacnet, const uint32_t *index,
  BACNET_Tag_Out_t *out)
{
  uint32_t count = bacnet->object_count + 1u;
  if(index && !*index) {
    BACNET_Tag_Unsigned(out, count);
    return BACNET_Error_None;
  }
  if(index && *index > count) return BACNET_Error_InvalidArrayIndex;
  for(uint32_t i = 1; i <= count; i++) {
    if(index && i != *index) continue;
    if(i == 1) {
      BACNET_Tag_Object(out, BACNET_Type_Device, bacnet->instance);
      continue;
    }
    const BACNET_Object_t *object = &bacnet->objects[i - 2];
    BACNET_Tag_Object(out, object->type, object->instance);
  }
  return BACNET_Error_None;
}

// Device property into `out`, `false` for one the Device does not hold.
// `Object_List` is read apart for its index;
// `Device_Address_Binding` is an empty list, the device binds to no other.
static bool device_value(const BACNET_t *bacnet, uint32_t property, BACNET_Tag_Out_t *out)
{
  switch(property) {
    case BACNET_Prop_ObjectIdentifier:
      BACNET_Tag_Object(out, BACNET_Type_Device, bacnet->instance);
      return true;
    case BACNET_Prop_ObjectName:
      BACNET_Tag_String(out, bacnet->name);
      return true;
    case BACNET_Prop_ObjectType:
      BACNET_Tag_Enumerated(out, BACNET_Type_Device);
      return true;
    case BACNET_Prop_SystemStatus:
      BACNET_Tag_Enumerated(out, BACNET_SYSTEM_STATUS);
      return true;
    case BACNET_Prop_VendorName:
      BACNET_Tag_String(out, BACNET_VENDOR_NAME);
      return true;
    case BACNET_Prop_VendorIdentifier:
      BACNET_Tag_Unsigned(out, BACNET_VENDOR_ID);
      return true;
    case BACNET_Prop_ModelName:
      BACNET_Tag_String(out, bacnet->model);
      return true;
    case BACNET_Prop_FirmwareRevision:
      BACNET_Tag_String(out, bacnet->firmware);
      return true;
    case BACNET_Prop_ApplicationSoftwareVersion:
      BACNET_Tag_String(out, bacnet->version);
      return true;
    case BACNET_Prop_ProtocolVersion:
      BACNET_Tag_Unsigned(out, BACNET_PROTOCOL_VERSION);
      return true;
    case BACNET_Prop_ProtocolRevision:
      BACNET_Tag_Unsigned(out, BACNET_PROTOCOL_REVISION);
      return true;
    case BACNET_Prop_ProtocolServicesSupported:
      BACNET_Tag_Bits(out, services, 40);
      return true;
    case BACNET_Prop_ProtocolObjectTypesSupported:
      BACNET_Tag_Bits(out, object_types, 51);
      return true;
    case BACNET_Prop_MaxAPDULengthAccepted:
      BACNET_Tag_Unsigned(out, BACNET_APDU_MAX);
      return true;
    case BACNET_Prop_SegmentationSupported:
      BACNET_Tag_Enumerated(out, BACNET_SEGMENTATION);
      return true;
    case BACNET_Prop_APDUTimeout:
      BACNET_Tag_Unsigned(out, BACNET_APDU_TIMEOUT_ms);
      return true;
    case BACNET_Prop_NumberOfAPDURetries:
      BACNET_Tag_Unsigned(out, BACNET_APDU_RETRIES);
      return true;
    case BACNET_Prop_DeviceAddressBinding:
      return true;
    case BACNET_Prop_DatabaseRevision:
      BACNET_Tag_Unsigned(out, bacnet->_revision);
      return true;
    case BACNET_Prop_MaxMaster:
      BACNET_Tag_Unsigned(out, BACNET_MAX_MASTER);
      return true;
    case BACNET_Prop_MaxInfoFrames:
      BACNET_Tag_Unsigned(out, BACNET_MAX_INFO_FRAMES);
      return true;
  }
  return false;
}

// Object property into `out`, `false` for one its type does not hold.
// `Event_State`, `Out_Of_Service` and `Polarity` are not modeled: normal, false and normal.
static bool object_value(const BACNET_Object_t *object, uint32_t property, BACNET_Tag_Out_t *out)
{
  switch(property) {
    case BACNET_Prop_ObjectIdentifier:
      BACNET_Tag_Object(out, object->type, object->instance);
      return true;
    case BACNET_Prop_ObjectName:
      BACNET_Tag_String(out, object->name);
      return true;
    case BACNET_Prop_ObjectType:
      BACNET_Tag_Enumerated(out, object->type);
      return true;
    case BACNET_Prop_PresentValue:
      if(analog(object)) BACNET_Tag_Real(out, object->value);
      else BACNET_Tag_Enumerated(out, object->value != 0);
      return true;
    case BACNET_Prop_StatusFlags:
      BACNET_Tag_Bits(out, status_flags, 4);
      return true;
    case BACNET_Prop_EventState:
      BACNET_Tag_Enumerated(out, 0);
      return true;
    case BACNET_Prop_OutOfService:
      BACNET_Tag_Boolean(out, false);
      return true;
    case BACNET_Prop_Units:
      if(!analog(object)) return false;
      BACNET_Tag_Enumerated(out, object->units);
      return true;
    case BACNET_Prop_Polarity:
      if(object->type != BACNET_Type_BinaryInput) return false;
      BACNET_Tag_Enumerated(out, 0);
      return true;
  }
  return false;
}

// Value of a property read, `object` `NULL` for the Device, or the error that answers instead
static BACNET_Error_t read_value(const BACNET_t *bacnet, const BACNET_Object_t *object,
  uint32_t property, const uint32_t *index, BACNET_Tag_Out_t *out)
{
  if(!object && property == BACNET_Prop_ObjectList) return object_list(bacnet, index, out);
  bool held = object ? object_value(object, property, out) : device_value(bacnet, property, out);
  if(!held) return BACNET_Error_UnknownProperty;
  if(index) return BACNET_Error_NotAnArray;
  return BACNET_Error_None;
}

// `Present_Value` of a writable Value object, from a Real or an Enumerated `0`/`1`
static BACNET_Error_t write_value(const BACNET_t *bacnet, BACNET_Object_t *object,
  uint32_t property, bool indexed, const BACNET_Tag_t *value, bool single)
{
  // A property is held when it reads, here into no room
  BACNET_Tag_Out_t probe = { 0 };
  if(read_value(bacnet, object, property, NULL, &probe)) return BACNET_Error_UnknownProperty;
  if(indexed && property != BACNET_Prop_ObjectList) return BACNET_Error_NotAnArray;
  BACNET_Type_t type = object ? object->type : BACNET_Type_Device;
  bool value_object = type == BACNET_Type_AnalogValue || type == BACNET_Type_BinaryValue;
  if(!value_object || !object->writable || property != BACNET_Prop_PresentValue) {
    return BACNET_Error_WriteAccessDenied;
  }
  if(!single || value->context) return BACNET_Error_InvalidDataType;
  if(type == BACNET_Type_AnalogValue) {
    if(value->number != BACNET_App_Real || value->len != 4) return BACNET_Error_InvalidDataType;
    memcpy(&object->value, &value->value, sizeof(object->value));
  }
  else {
    if(value->number != BACNET_App_Enumerated) return BACNET_Error_InvalidDataType;
    if(value->len > 4 || value->value > 1) return BACNET_Error_ValueOutOfRange;
    object->value = value->value;
  }
  object->update = true;
  return BACNET_Error_None;
}

//---------------------------------------------------------------------------------------- Requests

// Context tag `number` of a request, a value or an opening tag: read when there,
// `in` left where it was when another tag comes, `bad` when the bytes are malformed.
static bool field(BACNET_Tag_In_t *in, uint8_t number, bool opening, BACNET_Tag_t *tag)
{
  BACNET_Tag_In_t ahead = *in;
  if(!BACNET_Tag_Read(&ahead, tag)) {
    in->bad = ahead.bad;
    return false;
  }
  if(!tag->context || tag->number != number || tag->opening != opening || tag->closing) {
    return false;
  }
  *in = ahead;
  return true;
}

// Unsigned field: one to four bytes
static bool unsigned_field(BACNET_Tag_In_t *in, uint8_t number, BACNET_Tag_t *tag)
{
  if(!field(in, number, false, tag)) return false;
  if(tag->len >= 1 && tag->len <= 4) return true;
  in->bad = true;
  return false;
}

// Object identifier field: four bytes, type in the top ten bits
static bool object_field(BACNET_Tag_In_t *in, uint8_t number, uint32_t *type, uint32_t *instance)
{
  BACNET_Tag_t tag;
  if(!field(in, number, false, &tag)) return false;
  if(tag.len != 4) {
    in->bad = true;
    return false;
  }
  *type = tag.value >> 22;
  *instance = tag.value & 0x3FFFFF;
  return true;
}

// Value of a WriteProperty up to closing tag 3, `single` when it is one primitive value
static bool property_value(BACNET_Tag_In_t *in, BACNET_Tag_t *value, bool *single)
{
  BACNET_Tag_t tag;
  if(!field(in, 3, true, &tag)) return false;
  uint16_t count = 0;
  uint16_t depth = 0;
  while(BACNET_Tag_Read(in, &tag)) {
    if(tag.closing && !depth) {
      if(tag.number != 3) break;
      *single = count == 1 && !value->opening;
      return count > 0;
    }
    if(!count) *value = tag;
    count++;
    if(tag.opening) depth++;
    if(tag.closing) depth--;
  }
  in->bad = true; // no closing tag 3
  return false;
}

// Reject for a request whose fields stopped at `in`, `BACNET_Reject_None` when it parsed whole
static BACNET_Reject_t check(const BACNET_Tag_In_t *in, bool complete)
{
  if(in->bad) return BACNET_Reject_InvalidTag;
  if(!complete) {
    return in->pos < in->len ? BACNET_Reject_InvalidTag : BACNET_Reject_MissingRequiredParameter;
  }
  if(in->pos < in->len) return BACNET_Reject_TooManyArguments;
  return BACNET_Reject_None;
}

//----------------------------------------------------------------------------------------- Answers

// Head shared by SimpleACK, ComplexACK and Error
static void head(BACNET_Tag_Out_t *out, const BACNET_Request_t *request, BACNET_PDU_t pdu)
{
  BACNET_Tag_Byte(out, (uint8_t)(pdu << 4));
  BACNET_Tag_Byte(out, request->invoke);
  BACNET_Tag_Byte(out, request->service);
}

static void error(BACNET_Tag_Out_t *out, const BACNET_Request_t *request, BACNET_Error_t code)
{
  head(out, request, BACNET_PDU_Error);
  BACNET_Tag_Enumerated(out, code >> 8);
  BACNET_Tag_Enumerated(out, code & 0xFF);
}

static void reject(BACNET_Tag_Out_t *out, const BACNET_Request_t *request, BACNET_Reject_t reason)
{
  BACNET_Tag_Byte(out, BACNET_PDU_Reject << 4);
  BACNET_Tag_Byte(out, request->invoke);
  BACNET_Tag_Byte(out, reason);
}

static void read_property(BACNET_t *bacnet, const BACNET_Request_t *request,
  BACNET_Tag_In_t *in, BACNET_Tag_Out_t *out)
{
  uint32_t type, instance;
  BACNET_Tag_t property, index;
  bool complete = object_field(in, 0, &type, &instance) && unsigned_field(in, 1, &property);
  bool indexed = complete && unsigned_field(in, 2, &index);
  BACNET_Reject_t reason = check(in, complete);
  if(reason) {
    reject(out, request, reason);
    return;
  }
  if(type == BACNET_Type_Device && instance == BACNET_WILDCARD) instance = bacnet->instance;
  const BACNET_Object_t *object = NULL;
  if(type != BACNET_Type_Device || instance != bacnet->instance) {
    object = find(bacnet, type, instance);
    if(!object) {
      error(out, request, BACNET_Error_UnknownObject);
      return;
    }
  }
  uint16_t mark = out->len;
  head(out, request, BACNET_PDU_ComplexACK);
  BACNET_Tag_ContextObject(out, 0, type, instance);
  BACNET_Tag_ContextUnsigned(out, 1, property.value);
  if(indexed) BACNET_Tag_ContextUnsigned(out, 2, index.value);
  BACNET_Tag_Open(out, 3);
  BACNET_Error_t code =
    read_value(bacnet, object, property.value, indexed ? &index.value : NULL, out);
  BACNET_Tag_Close(out, 3);
  if(!code) return;
  out->len = mark;
  out->full = false;
  error(out, request, code);
}

static void write_property(BACNET_t *bacnet, const BACNET_Request_t *request,
  BACNET_Tag_In_t *in, BACNET_Tag_Out_t *out)
{
  uint32_t type, instance;
  BACNET_Tag_t property, index, value, priority;
  bool single = false;
  bool complete = object_field(in, 0, &type, &instance) && unsigned_field(in, 1, &property);
  bool indexed = complete && unsigned_field(in, 2, &index);
  complete = complete && property_value(in, &value, &single);
  bool prioritized = complete && unsigned_field(in, 4, &priority);
  BACNET_Reject_t reason = check(in, complete);
  if(!reason && prioritized && (priority.value < 1 || priority.value > 16)) {
    reason = BACNET_Reject_ParameterOutOfRange;
  }
  if(reason) {
    reject(out, request, reason);
    return;
  }
  // Priority is ignored, no object here has a priority array
  BACNET_Object_t *object = NULL;
  if(type != BACNET_Type_Device || instance != bacnet->instance) {
    object = find(bacnet, type, instance);
    if(!object) {
      error(out, request, BACNET_Error_UnknownObject);
      return;
    }
  }
  BACNET_Error_t code = write_value(bacnet, object, property.value, indexed, &value, single);
  if(code) error(out, request, code);
  else head(out, request, BACNET_PDU_SimpleACK);
}

//------------------------------------------------------------------------------------------ Client

// Value of an application tag as a number: a REAL, an Enumerated or an Unsigned
static bool number(const BACNET_Tag_t *tag, float *value)
{
  if(tag->context) return false;
  if(tag->number == BACNET_App_Real && tag->len == 4) {
    memcpy(value, &tag->value, sizeof(*value));
    return true;
  }
  if(tag->number != BACNET_App_Enumerated && tag->number != BACNET_App_Unsigned) return false;
  *value = (float)tag->value;
  return true;
}

// ReadProperty ack: object, property and maybe an index, then the value inside context tag 3
static bool read_ack(BACNET_Tag_In_t *in, float *value)
{
  uint32_t type, instance;
  BACNET_Tag_t tag;
  if(!object_field(in, 0, &type, &instance) || !unsigned_field(in, 1, &tag)) return false;
  unsigned_field(in, 2, &tag);
  if(!field(in, 3, true, &tag) || !BACNET_Tag_Read(in, &tag) || !number(&tag, value)) {
    return false;
  }
  return BACNET_Tag_Read(in, &tag) && tag.closing && tag.number == 3;
}

// Answer from `source` with the invoke ID of the request waiting:
// the value of a read or the ack of a write gives `OK`,
// an error, a reject, an abort or a broken ack `ERR`.
static void answer(BACNET_t *bacnet, uint8_t source, const BACNET_Tag_In_t *apdu)
{
  const uint8_t *data = apdu->data;
  if(!bacnet->_asking || bacnet->_answered || source != bacnet->_peer) return;
  if(apdu->len < 3 || data[1] != bacnet->_invoke) return;
  bool write = bacnet->_service == BACNET_Service_WriteProperty;
  uint8_t ack = (write ? BACNET_PDU_SimpleACK : BACNET_PDU_ComplexACK) << 4;
  BACNET_Tag_In_t in = { .data = &data[3], .len = apdu->len - 3 };
  bacnet->_status = ERR;
  if(data[0] == ack && data[2] == bacnet->_service && (write || read_ack(&in, &bacnet->_value))) {
    bacnet->_status = OK;
  }
  bacnet->_answered = true;
}

// Request for `Present_Value` of an object: NPDU expecting a reply,
// a confirmed APDU taking up to 480 bytes back, a new invoke ID.
static void request(BACNET_t *bacnet, BACNET_Tag_Out_t *out, BACNET_Service_t service,
  BACNET_Type_t type, uint32_t instance)
{
  bacnet->_invoke++;
  bacnet->_service = service;
  BACNET_Tag_Byte(out, BACNET_PROTOCOL_VERSION);
  BACNET_Tag_Byte(out, 0x04); // expecting reply
  BACNET_Tag_Byte(out, BACNET_PDU_ConfirmedRequest << 4);
  BACNET_Tag_Byte(out, 0x03); // max-APDU code of 480 bytes
  BACNET_Tag_Byte(out, bacnet->_invoke);
  BACNET_Tag_Byte(out, service);
  BACNET_Tag_ContextObject(out, 0, type, instance);
  BACNET_Tag_ContextUnsigned(out, 1, BACNET_Prop_PresentValue);
}

// Request sent with the token of this device and its answer awaited, other threads running;
// no answer within `timeout_ms` is an `ERR`.
static status_t ask(BACNET_t *bacnet, uint8_t mac, const BACNET_Tag_Out_t *out,
  uint32_t timeout_ms)
{
  uint64_t deadline = tick_keep(timeout_ms);
  bacnet->_asking = true;
  bacnet->_answered = false;
  bacnet->_peer = mac;
  while(MSTP_Send(bacnet->mstp, mac, out->data, out->len, true) && tick_away(&deadline)) let();
  while(!bacnet->_answered && tick_away(&deadline)) let();
  bacnet->_asking = false;
  return bacnet->_answered ? bacnet->_status : ERR;
}

//----------------------------------------------------------------------------------------- Network

// NPDU of a frame: `false` when not meant for this device, `apdu` set to what follows it
static bool npdu(const MSTP_Frame_t *frame, BACNET_Request_t *request, BACNET_Tag_In_t *apdu)
{
  const uint8_t *data = frame->data;
  uint16_t len = frame->len;
  if(len < 2 || data[0] != BACNET_PROTOCOL_VERSION) return false;
  uint8_t control = data[1];
  if(control & 0x80) return false; // network layer message, for routers
  bool destination = control & 0x20;
  request->priority = control & 0x03;
  uint16_t pos = 2;
  if(destination) {
    if(len < pos + 3) return false;
    uint16_t dnet = (uint16_t)((data[pos] << 8) | data[pos + 1]);
    pos += 3 + data[pos + 2];
    if(dnet != 0xFFFF) return false; // another network, this device routes nothing
    request->global = true;
  }
  if(control & 0x08) {
    if(len < pos + 3) return false;
    request->routed = true;
    request->snet = (uint16_t)((data[pos] << 8) | data[pos + 1]);
    request->slen = data[pos + 2];
    if(!request->slen || request->slen > sizeof(request->sadr)) return false;
    if(len < pos + 3 + request->slen) return false;
    memcpy(request->sadr, &data[pos + 3], request->slen);
    pos += 3 + request->slen;
  }
  if(destination) pos++; // hop count
  if(pos > len) return false;
  apdu->data = &data[pos];
  apdu->len = len - pos;
  apdu->pos = 0;
  apdu->bad = false;
  return true;
}

// Back through the router the request came by: its source as the destination,
// hop count `255`, no source of this device; the request's network priority kept.
static void npdu_reply(BACNET_Tag_Out_t *out, const BACNET_Request_t *request)
{
  BACNET_Tag_Byte(out, BACNET_PROTOCOL_VERSION);
  if(!request->routed) {
    BACNET_Tag_Byte(out, request->priority);
    return;
  }
  BACNET_Tag_Byte(out, 0x20 | request->priority);
  BACNET_Tag_Byte(out, (uint8_t)(request->snet >> 8));
  BACNET_Tag_Byte(out, (uint8_t)request->snet);
  BACNET_Tag_Byte(out, request->slen);
  for(uint8_t i = 0; i < request->slen; i++) BACNET_Tag_Byte(out, request->sadr[i]);
  BACNET_Tag_Byte(out, 255);
}

// Longest APDU the client takes, by the code of its request; reserved codes take none
static uint16_t max_apdu(uint8_t code)
{
  static const uint16_t sizes[] = { 50, 128, 206, 480, 1024, 1476 };
  return code < array_len(sizes) ? sizes[code] : 0;
}

// I-Am as a global broadcast with normal priority, so routers pass it on;
// it goes out with the next token.
static void i_am(BACNET_t *bacnet)
{
  static const uint8_t npdu_broadcast[] = { 0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF };
  uint8_t buffer[32];
  BACNET_Tag_Out_t out = { .data = buffer, .size = sizeof(buffer) };
  for(uint8_t i = 0; i < sizeof(npdu_broadcast); i++) BACNET_Tag_Byte(&out, npdu_broadcast[i]);
  BACNET_Tag_Byte(&out, BACNET_PDU_UnconfirmedRequest << 4);
  BACNET_Tag_Byte(&out, BACNET_Service_IAm);
  BACNET_Tag_Object(&out, BACNET_Type_Device, bacnet->instance);
  BACNET_Tag_Unsigned(&out, BACNET_APDU_MAX);
  BACNET_Tag_Enumerated(&out, BACNET_SEGMENTATION);
  BACNET_Tag_Unsigned(&out, BACNET_VENDOR_ID);
  MSTP_Send(bacnet->mstp, MSTP_BROADCAST, buffer, out.len, false);
}

// Limits come both or none, each `BACNET_WILDCARD` at most, else the Who-Is goes unanswered
static void who_is(BACNET_t *bacnet, BACNET_Tag_In_t *in)
{
  BACNET_Tag_t low, high;
  bool limits = unsigned_field(in, 0, &low);
  if(limits && !unsigned_field(in, 1, &high)) return;
  if(in->bad || in->pos < in->len) return;
  if(limits && (low.value > BACNET_WILDCARD || high.value > BACNET_WILDCARD)) return;
  if(limits && (bacnet->instance < low.value || bacnet->instance > high.value)) return;
  i_am(bacnet);
}

// Confirmed request answered: an ack, an error, a reject,
// or an abort when the answer does not fit the client or the request comes in segments.
static void confirmed(BACNET_t *bacnet, BACNET_Request_t *request, const BACNET_Tag_In_t *apdu)
{
  const uint8_t *data = apdu->data;
  request->invoke = data[2];
  request->service = data[3];
  request->max_apdu = max_apdu(data[1] & 0x0F);
  uint8_t *buffer = heap_alloc(MSTP_DATA_MAX);
  BACNET_Tag_Out_t out = { .data = buffer, .size = MSTP_DATA_MAX };
  npdu_reply(&out, request);
  uint16_t start = out.len;
  out.size = start + minv(BACNET_APDU_MAX, request->max_apdu);
  BACNET_Tag_In_t in = { .data = &data[4], .len = apdu->len - 4 };
  uint8_t service = request->service;
  // Segments are not taken, a segmented request gets the Abort of an answer too long
  if(data[0] & 0x08) out.full = true;
  else if(service == BACNET_Service_ReadProperty) read_property(bacnet, request, &in, &out);
  else if(service == BACNET_Service_WriteProperty) write_property(bacnet, request, &in, &out);
  else reject(&out, request, BACNET_Reject_UnrecognizedService);
  if(out.full) {
    out.len = start;
    out.size = MSTP_DATA_MAX;
    out.full = false;
    BACNET_Tag_Byte(&out, (BACNET_PDU_Abort << 4) | 0x01); // sent by the server
    BACNET_Tag_Byte(&out, request->invoke);
    BACNET_Tag_Byte(&out, BACNET_ABORT_SEGMENTATION_NOT_SUPPORTED);
  }
  MSTP_Reply(bacnet->mstp, buffer, out.len);
  heap_free(buffer);
}

// One frame from the link: a request to serve or an answer to a request of this device.
// A confirmed request is served in a Data Expecting Reply to this node,
// in any other frame it is dropped, and so is one by global broadcast, as BTL tests it.
static void serve(BACNET_t *bacnet, const MSTP_Frame_t *frame)
{
  BACNET_Request_t request = { 0 };
  BACNET_Tag_In_t apdu;
  if(!npdu(frame, &request, &apdu) || !apdu.len) return;
  uint8_t pdu = apdu.data[0] >> 4;
  if(pdu == BACNET_PDU_UnconfirmedRequest) {
    if(apdu.len < 2 || apdu.data[1] != BACNET_Service_WhoIs) return;
    BACNET_Tag_In_t in = { .data = &apdu.data[2], .len = apdu.len - 2 };
    who_is(bacnet, &in);
    return;
  }
  if(pdu != BACNET_PDU_ConfirmedRequest) {
    answer(bacnet, frame->source, &apdu);
    return;
  }
  if(!frame->expecting_reply || request.global) return;
  if(apdu.len >= 4) confirmed(bacnet, &request, &apdu);
}

// `Database_Revision`: CRC-32 of the device and its table, other objects in a firmware change it
static uint32_t database_revision(const BACNET_t *bacnet)
{
  CRC_Start(&crc32_iso);
  CRC_Next(&crc32_iso, &bacnet->instance, sizeof(bacnet->instance));
  if(bacnet->name) CRC_Next(&crc32_iso, bacnet->name, strlen(bacnet->name));
  for(uint16_t i = 0; i < bacnet->object_count; i++) {
    const BACNET_Object_t *object = &bacnet->objects[i];
    CRC_Next(&crc32_iso, &object->type, sizeof(object->type));
    CRC_Next(&crc32_iso, &object->instance, sizeof(object->instance));
    if(object->name) CRC_Next(&crc32_iso, object->name, strlen(object->name));
  }
  return CRC_End(&crc32_iso);
}

//--------------------------------------------------------------------------------------------- API

void BACNET_Loop(BACNET_t *bacnet)
{
  if(!bacnet->_init) {
    bacnet->_revision = database_revision(bacnet);
    bacnet->_init = true;
  }
  const MSTP_Frame_t *frame;
  while((frame = MSTP_Loop(bacnet->mstp))) serve(bacnet, frame);
}

status_t BACNET_Read(BACNET_t *bacnet, uint8_t mac, BACNET_Type_t type, uint32_t instance,
  float *value, uint32_t timeout_ms)
{
  if(bacnet->_asking) return BUSY;
  uint8_t buffer[16];
  BACNET_Tag_Out_t out = { .data = buffer, .size = sizeof(buffer) };
  request(bacnet, &out, BACNET_Service_ReadProperty, type, instance);
  status_t status = ask(bacnet, mac, &out, timeout_ms);
  if(!status) *value = bacnet->_value;
  return status;
}

status_t BACNET_Write(BACNET_t *bacnet, uint8_t mac, BACNET_Type_t type, uint32_t instance,
  float value, uint32_t timeout_ms)
{
  if(bacnet->_asking) return BUSY;
  uint8_t buffer[24];
  BACNET_Tag_Out_t out = { .data = buffer, .size = sizeof(buffer) };
  request(bacnet, &out, BACNET_Service_WriteProperty, type, instance);
  bool real = type == BACNET_Type_AnalogInput || type == BACNET_Type_AnalogValue;
  BACNET_Tag_Open(&out, 3);
  if(real) BACNET_Tag_Real(&out, value);
  else BACNET_Tag_Enumerated(&out, value != 0);
  BACNET_Tag_Close(&out, 3);
  return ask(bacnet, mac, &out, timeout_ms);
}

//-------------------------------------------------------------------------------------------------

// .tests/test_bacnet.c

// BACnet device over its MS/TP link, exchanges byte for byte with the boiler room of the plan:
// the read of the plan and the ReadProperty and WriteProperty of Annex F,
// Who-Is with and without a range answered by I-Am at the token,
// every property of every object type, `Object_List` whole and by index,
// writes of each type and each refusal, each Reject reason,
// Abort for an answer the client cannot take whole or a request in segments,
// a request through a router answered with its network priority, Device 4194303 in a read,
// `Database_Revision` following the table, confirmed requests by broadcast left unanswered,
// and the device as a client: `Present_Value` read and written in a master on MAC 6,
// its answer postponed or not, every other answer and no answer in time an `ERR`.

#include <string.h>

#include "task.h"
#include "test.h"
#include "plc/com/mstp.c"
#include "plc/com/bacnet_tag.c"
#include "plc/com/bacnet.c"

// Bytes of a string literal and their count, its terminator left out
#define bytes_len(literal) (literal), (sizeof(literal) - 1)

static uint8_t rx_memory[1024];
static BUFF_t rx = { .memory = rx_memory, .size = sizeof(rx_memory) };
static UART_t port = { .UART_38400, .buff = &rx };

// Objects of the boiler room in the plan
enum { OBJ_Supply, OBJ_PumpRun, OBJ_Setpoint, OBJ_Manual, OBJ_Pump };
static const BACNET_Object_t boiler_room[] = {
  [OBJ_Supply] = {
    .type = BACNET_Type_AnalogInput,
    .instance = 1,
    .name = "Supply temp",
    .units = BACNET_Unit_DegreesCelsius
  },
  [OBJ_PumpRun] = {
    .type = BACNET_Type_BinaryInput,
    .instance = 1,
    .name = "Pump running"
  },
  [OBJ_Setpoint] = {
    .type = BACNET_Type_AnalogValue,
    .instance = 1,
    .name = "Setpoint",
    .units = BACNET_Unit_DegreesCelsius,
    .writable = true,
    .value = 55
  },
  [OBJ_Manual] = {
    .type = BACNET_Type_BinaryValue,
    .instance = 1,
    .name = "Manual",
    .writable = true
  },
  [OBJ_Pump] = {
    .type = BACNET_Type_BinaryValue,
    .instance = 2,
    .name = "Pump",
    .writable = true
  }
};

// Their identifiers as frames carry them
static const char device_id[] = "\x02\x00\x03\xED";
static const char supply_id[] = "\x00\x00\x00\x01";
static const char pump_run_id[] = "\x00\xC0\x00\x01";
static const char setpoint_id[] = "\x00\x80\x00\x01";
static const char manual_id[] = "\x01\x40\x00\x01";

static BACNET_Object_t objects[array_len(boiler_room)];
static BACNET_Object_t many[93];
static MSTP_t node;
static BACNET_t device;

//-------------------------------------------------------------------------------------------- Line

// Frame laid out as the standard has it, its CRCs by the link, tested against Annex G
static uint16_t frame(uint8_t *out, uint8_t type, uint8_t destination, uint8_t source,
  const void *data, uint16_t len)
{
  out[0] = 0x55;
  out[1] = 0xFF;
  out[2] = type;
  out[3] = destination;
  out[4] = source;
  out[5] = (uint8_t)(len >> 8);
  out[6] = (uint8_t)len;
  CRC_Append(&crc_header, &out[2], 5);
  if(!len) return 8;
  memcpy(&out[8], data, len);
  return 8 + CRC_Append(&crc_data, &out[8], len);
}

// Bytes from the line, one message as the receiver timeout closes it
static void hear(const uint8_t *bytes, uint16_t len)
{
  for(uint16_t i = 0; i < len; i++) BUFF_Push(&rx, bytes[i]);
  BUFF_Break(&rx);
}

// Frame of another node
static void hear_frame(uint8_t type, uint8_t destination, uint8_t source,
  const void *data, uint16_t len)
{
  uint8_t bytes[520];
  hear(bytes, frame(bytes, type, destination, source, data, len));
}

// The node put exactly this frame on the line since the last look
static void sent(uint8_t type, uint8_t destination, const void *data, uint16_t len,
  const char *msg)
{
  uint8_t expected[300], line[256] = { 0 };
  uint16_t size = frame(expected, type, destination, 5, data, len);
  uint16_t count = UART_HostSent(&port, line);
  TEST_Bytes(line, expected, size, msg);
  TEST_Check(count == size, "%s: %u bytes, %u expected", msg, count, size);
}

// The node answered MAC 1 with this NPDU
static void answered(const void *npdu, uint16_t len, const char *msg) {
  sent(MSTP_Type_DataNotExpectingReply, 1, npdu, len, msg);
}

// The node sent nothing since the last look
static bool quiet(void) { return !UART_HostSent(&port, NULL); }

//------------------------------------------------------------------------------------------ Device

// One pass of the device's thread, one millisecond later
static void pass(void)
{
  BACNET_Loop(&device);
  TEST_Run(1);
}

// Boiler room of the plan on MAC 5 at 38400 baud, a quiet line, the first pass still to come
static void boiler(void)
{
  heap_free(node._rx);
  heap_free(node._tx);
  heap_free(node._pending);
  memset(&node, 0, sizeof(node));
  node.uart = &port;
  node.address = 5;
  memcpy(objects, boiler_room, sizeof(objects));
  device = (BACNET_t){
    .mstp = &node,
    .instance = 1005,
    .name = "Boiler room",
    .model = "Boiler controller",
    .firmware = "1.0",
    .version = "1.2.0",
    .objects = objects,
    .object_count = array_len(objects)
  };
  TASK_ClearAll();
  UART_HostAttach(&port, NULL, NULL);
  UART_Init(&port);
  UART_HostSent(&port, NULL);
}

static void start(void)
{
  boiler();
  pass();
}

// Confirmed request from MAC 1 in a Data Expecting Reply, and one pass
static void hear_request(const void *npdu, uint16_t len)
{
  hear_frame(MSTP_Type_DataExpectingReply, 5, 1, npdu, len);
  pass();
}

// ReadProperty from MAC 1 with invoke ID 7, of the object with identifier `object`
static void read_request(const char *object, uint8_t property)
{
  uint8_t request[] = { 0x01, 0x04, 0x00, 0x05, 0x07, 0x0C, 0x0C, 0, 0, 0, 0, 0x19, property };
  memcpy(&request[7], object, 4);
  hear_request(request, sizeof(request));
}

// ReadProperty answered with `value` inside context tag 3
static void reads(const char *object, uint8_t property, const char *value, uint16_t len,
  const char *msg)
{
  read_request(object, property);
  uint8_t ack[64] = { 0x01, 0x00, 0x30, 0x07, 0x0C, 0x0C, 0, 0, 0, 0, 0x19, property, 0x3E };
  memcpy(&ack[6], object, 4);
  memcpy(&ack[13], value, len);
  ack[13 + len] = 0x3F;
  answered(ack, 14 + len, msg);
}

// Token to the node from MAC 1, and one pass
static void token(void)
{
  hear_frame(MSTP_Type_Token, 5, 1, NULL, 0);
  pass();
}

//---------------------------------------------------------------------------------------- Examples

// ReadProperty of the plan: the frame of the client in, the reply on the line byte for byte
static void example_of_the_plan(void)
{
  start();
  objects[OBJ_Supply].value = 21.5f;
  static const uint8_t request[] = {
    0x55, 0xFF, 0x05, 0x05, 0x01, 0x00, 0x0D, 0xAB,
    0x01, 0x04, 0x00, 0x03, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55,
    0x21, 0x61
  };
  hear(request, sizeof(request));
  pass();
  static const uint8_t reply[] = {
    0x55, 0xFF, 0x06, 0x01, 0x05, 0x00, 0x13, 0x6C,
    0x01, 0x00, 0x30, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55,
    0x3E, 0x44, 0x41, 0xAC, 0x00, 0x00, 0x3F,
    0x38, 0x24
  };
  uint8_t line[64];
  TEST_Equal(UART_HostSent(&port, line), sizeof(reply), "reply length");
  TEST_Bytes(line, reply, sizeof(reply), "reply of the plan");
}

// ReadProperty of Analog Input 5 by a client taking 50 bytes, Annex F.3.5,
// and WriteProperty of Analog Value 1, Annex F.3.9.
static void examples_of_annex_f(void)
{
  start();
  objects[OBJ_Supply].instance = 5;
  objects[OBJ_Supply].value = 72.3f;
  hear_request(bytes_len("\x01\x04\x00\x00\x01\x0C\x0C\x00\x00\x00\x05\x19\x55"));
  answered(bytes_len("\x01\x00\x30\x01\x0C\x0C\x00\x00\x00\x05\x19\x55"
    "\x3E\x44\x42\x90\x99\x9A\x3F"), "Present_Value 72.3");
  hear_request(bytes_len("\x01\x04\x00\x04\x59\x0F\x0C\x00\x80\x00\x01\x19\x55"
    "\x3E\x44\x43\x34\x00\x00\x3F"));
  answered(bytes_len("\x01\x00\x20\x59\x0F"), "SimpleACK");
  TEST_Check(objects[OBJ_Setpoint].value == 180.0f, "Setpoint 180");
  TEST_Check(objects[OBJ_Setpoint].update, "update raised");
}

//------------------------------------------------------------------------------------------ Who-Is

// Who-Is broadcast from MAC 1 as YABE sends it, normal priority to every network
static void who_is_request(const char *apdu, uint16_t len)
{
  uint8_t npdu[32] = { 0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF };
  memcpy(&npdu[6], apdu, len);
  hear_frame(MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, 1, npdu, 6 + len);
  pass();
}

// In a ring of MAC 1, 5 and 6: I-Am goes out with the next token, ahead of passing it on
static void who_is_answered_at_the_token(void)
{
  start();
  token();
  sent(MSTP_Type_PollForMaster, 6, NULL, 0, "successor looked for");
  hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 6, NULL, 0);
  pass();
  sent(MSTP_Type_Token, 6, NULL, 0, "token to 6");
  static const char i_am_1005[] =
    "\x01\x20\xFF\xFF\x00\xFF\x10\x00\xC4\x02\x00\x03\xED\x22\x01\xE0\x91\x03\x21\x00";
  static const struct {
    const char *apdu;
    uint8_t len;
    bool answered;
    const char *what;
  } cases[] = {
    { bytes_len("\x10\x08"), true, "no range, Annex F.4.9" },
    { bytes_len("\x10\x08\x0A\x03\xED\x1A\x03\xED"), true, "range 1005 to 1005" },
    { bytes_len("\x10\x08\x09\x00\x1B\x3F\xFF\xFF"), true, "range 0 to 4194303" },
    { bytes_len("\x10\x08\x09\x03\x19\x03"), false, "range 3 to 3, Annex F.4.9" },
    { bytes_len("\x10\x08\x0A\x03\xED"), false, "one limit" },
    { bytes_len("\x10\x08\x09\x00\x1B\x40\x00\x00"), false, "limit past 4194303" },
    { bytes_len("\x10\x00\xC4\x02\x00\x00\x03\x22\x04\x00\x91\x03\x21\x63"), false, "I-Am of another device, Annex F.4.9" }
  };
  for(uint8_t i = 0; i < array_len(cases); i++) {
    who_is_request(cases[i].apdu, cases[i].len);
    TEST_Check(quiet(), "%s: nothing before the token", cases[i].what);
    token();
    if(cases[i].answered) {
      sent(MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, bytes_len(i_am_1005), cases[i].what);
      pass();
    }
    sent(MSTP_Type_Token, 6, NULL, 0, cases[i].what);
  }
}

//-------------------------------------------------------------------------------------------- Read

static void reads_device_properties(void)
{
  start();
  reads(device_id, 75, bytes_len("\xC4\x02\x00\x03\xED"), "Object_Identifier");
  reads(device_id, 77, bytes_len("\x75\x0C\x00" "Boiler room"), "Object_Name");
  reads(device_id, 79, bytes_len("\x91\x08"), "Object_Type");
  reads(device_id, 112, bytes_len("\x91\x00"), "System_Status operational");
  reads(device_id, 121, bytes_len("\x75\x09\x00" "OpenCPLC"), "Vendor_Name");
  reads(device_id, 120, bytes_len("\x21\x00"), "Vendor_Identifier");
  reads(device_id, 70, bytes_len("\x75\x12\x00" "Boiler controller"), "Model_Name");
  reads(device_id, 44, bytes_len("\x74\x00" "1.0"), "Firmware_Revision");
  reads(device_id, 12, bytes_len("\x75\x06\x00" "1.2.0"), "Application_Software_Version");
  reads(device_id, 98, bytes_len("\x21\x01"), "Protocol_Version");
  reads(device_id, 139, bytes_len("\x21\x0C"), "Protocol_Revision 12");
  reads(device_id, 97, bytes_len("\x85\x06\x00\x00\x09\x00\x00\x20"),
    "services: ReadProperty, WriteProperty, Who-Is");
  reads(device_id, 96, bytes_len("\x85\x08\x05\xB4\x80\x00\x00\x00\x00\x00"),
    "object types: AI, AV, BI, BV, Device");
  reads(device_id, 76, bytes_len(
    "\xC4\x02\x00\x03\xED\xC4\x00\x00\x00\x01\xC4\x00\xC0\x00\x01"
    "\xC4\x00\x80\x00\x01\xC4\x01\x40\x00\x01\xC4\x01\x40\x00\x02"),
    "Object_List, the Device first");
  reads(device_id, 62, bytes_len("\x22\x01\xE0"), "Max_APDU_Length_Accepted 480");
  reads(device_id, 107, bytes_len("\x91\x03"), "Segmentation_Supported none");
  reads(device_id, 11, bytes_len("\x22\x0B\xB8"), "APDU_Timeout 3000");
  reads(device_id, 73, bytes_len("\x21\x00"), "Number_Of_APDU_Retries 0");
  reads(device_id, 30, bytes_len(""), "Device_Address_Binding empty");
  reads(device_id, 64, bytes_len("\x21\x7F"), "Max_Master 127");
  reads(device_id, 63, bytes_len("\x21\x01"), "Max_Info_Frames 1");
}

static void reads_object_properties(void)
{
  start();
  objects[OBJ_Supply].value = 21.5f;
  objects[OBJ_PumpRun].value = 1;
  const char *ids[] = { supply_id, pump_run_id, setpoint_id, manual_id };
  for(uint8_t i = 0; i < array_len(ids); i++) {
    reads(ids[i], 111, bytes_len("\x82\x04\x00"), "Status_Flags all clear");
    reads(ids[i], 36, bytes_len("\x91\x00"), "Event_State normal");
    reads(ids[i], 81, bytes_len("\x10"), "Out_Of_Service false");
  }
  reads(supply_id, 75, bytes_len("\xC4\x00\x00\x00\x01"), "Analog Input 1");
  reads(supply_id, 77, bytes_len("\x75\x0C\x00" "Supply temp"), "its name");
  reads(supply_id, 79, bytes_len("\x91\x00"), "its type");
  reads(supply_id, 85, bytes_len("\x44\x41\xAC\x00\x00"), "21.5");
  reads(supply_id, 117, bytes_len("\x91\x3E"), "degrees Celsius");
  reads(pump_run_id, 75, bytes_len("\xC4\x00\xC0\x00\x01"), "Binary Input 1");
  reads(pump_run_id, 77, bytes_len("\x75\x0D\x00" "Pump running"), "its name");
  reads(pump_run_id, 79, bytes_len("\x91\x03"), "its type");
  reads(pump_run_id, 85, bytes_len("\x91\x01"), "active");
  reads(pump_run_id, 84, bytes_len("\x91\x00"), "Polarity normal");
  reads(setpoint_id, 75, bytes_len("\xC4\x00\x80\x00\x01"), "Analog Value 1");
  reads(setpoint_id, 77, bytes_len("\x75\x09\x00" "Setpoint"), "its name");
  reads(setpoint_id, 79, bytes_len("\x91\x02"), "its type");
  reads(setpoint_id, 85, bytes_len("\x44\x42\x5C\x00\x00"), "55.0");
  reads(setpoint_id, 117, bytes_len("\x91\x3E"), "degrees Celsius");
  reads(manual_id, 75, bytes_len("\xC4\x01\x40\x00\x01"), "Binary Value 1");
  reads(manual_id, 77, bytes_len("\x75\x07\x00" "Manual"), "its name");
  reads(manual_id, 79, bytes_len("\x91\x05"), "its type");
  reads(manual_id, 85, bytes_len("\x91\x00"), "inactive");
}

static void unknown_objects_and_properties(void)
{
  start();
  static const char unknown_property[] = "\x01\x00\x50\x07\x0C\x91\x02\x91\x20";
  static const char unknown_object[] = "\x01\x00\x50\x07\x0C\x91\x01\x91\x1F";
  read_request(supply_id, 84);
  answered(bytes_len(unknown_property), "no Polarity in an Analog Input");
  read_request(pump_run_id, 117);
  answered(bytes_len(unknown_property), "no Units in a Binary Input");
  read_request(setpoint_id, 84);
  answered(bytes_len(unknown_property), "no Polarity in an Analog Value");
  read_request(manual_id, 117);
  answered(bytes_len(unknown_property), "no Units in a Binary Value");
  read_request(device_id, 85);
  answered(bytes_len(unknown_property), "no Present_Value in the Device");
  read_request(supply_id, 76);
  answered(bytes_len(unknown_property), "no Object_List in an Analog Input");
  read_request("\x00\x00\x00\x09", 85);
  answered(bytes_len(unknown_object), "no Analog Input 9");
  read_request("\x02\x00\x03\xEE", 75);
  answered(bytes_len(unknown_object), "no Device 1006");
}

static void object_list_by_index(void)
{
  start();
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x00"));
  answered(bytes_len("\x01\x00\x30\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x00"
    "\x3E\x21\x06\x3F"), "index 0: six elements");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x01"));
  answered(bytes_len("\x01\x00\x30\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x01"
    "\x3E\xC4\x02\x00\x03\xED\x3F"), "index 1: the Device");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x06"));
  answered(bytes_len("\x01\x00\x30\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x06"
    "\x3E\xC4\x01\x40\x00\x02\x3F"), "index 6: Binary Value 2");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x02\x00\x03\xED\x19\x4C\x29\x07"));
  answered(bytes_len("\x01\x00\x50\x07\x0C\x91\x02\x91\x2A"), "index 7: invalid-array-index");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x19\x55\x29\x01"));
  answered(bytes_len("\x01\x00\x50\x07\x0C\x91\x02\x91\x32"),
    "index of Present_Value: property-is-not-an-array");
}

// Device 4194303 reads as this device, the ack carrying its real instance; a write takes no such
static void device_4194303_in_a_read(void)
{
  start();
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x02\x3F\xFF\xFF\x19\x4B"));
  answered(bytes_len("\x01\x00\x30\x07\x0C\x0C\x02\x00\x03\xED\x19\x4B"
    "\x3E\xC4\x02\x00\x03\xED\x3F"), "instance 1005 learned");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x02\x3F\xFF\xFF\x19\x4D"
    "\x3E\x72\x00" "X" "\x3F"));
  answered(bytes_len("\x01\x00\x50\x07\x0F\x91\x01\x91\x1F"), "write: unknown-object");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x00\x3F\xFF\xFF\x19\x55"));
  answered(bytes_len("\x01\x00\x50\x07\x0C\x91\x01\x91\x1F"),
    "Analog Input 4194303: unknown-object");
}

// `Database_Revision` as the device reports it, read through Device 4194303
static uint32_t revision(void)
{
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x02\x3F\xFF\xFF\x19\x9B"));
  uint8_t line[64];
  UART_HostSent(&port, line);
  // Header 8 bytes, NPDU 2, the ack up to the opening tag 11, then an Unsigned
  const uint8_t *value = &line[21];
  uint8_t len = value[0] & 0x07;
  TEST_Check((value[0] & 0xF8) == 0x20 && len >= 1 && len <= 4 && value[1 + len] == 0x3F,
    "Database_Revision an Unsigned");
  uint32_t number = 0;
  for(uint8_t i = 1; i <= len; i++) number = (number << 8) | value[i];
  return number;
}

static void database_revision_follows_the_table(void)
{
  start();
  uint32_t first = revision();
  start();
  TEST_Equal(revision(), first, "same table, same revision");
  objects[OBJ_Supply].value = 80;
  TEST_Equal(revision(), first, "values leave it alone");
  boiler();
  objects[OBJ_Pump].name = "Pump 2";
  pass();
  TEST_Check(revision() != first, "object name");
  boiler();
  objects[OBJ_Pump].instance = 3;
  pass();
  TEST_Check(revision() != first, "object instance");
  boiler();
  device.object_count--;
  pass();
  TEST_Check(revision() != first, "one object less");
  boiler();
  device.name = "Boiler house";
  pass();
  TEST_Check(revision() != first, "device name");
  boiler();
  device.instance = 1006;
  pass();
  TEST_Check(revision() != first, "device instance");
}

//------------------------------------------------------------------------------------------- Write

static void writes_of_each_type(void)
{
  start();
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55"
    "\x3E\x44\x42\x70\x00\x00\x3F\x49\x08"));
  answered(bytes_len("\x01\x00\x20\x07\x0F"), "Setpoint, its priority 8 ignored");
  TEST_Check(objects[OBJ_Setpoint].value == 60.0f && objects[OBJ_Setpoint].update, "60 written");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x01\x40\x00\x01\x19\x55\x3E\x91\x01\x3F"));
  answered(bytes_len("\x01\x00\x20\x07\x0F"), "Manual");
  TEST_Check(objects[OBJ_Manual].value == 1 && objects[OBJ_Manual].update, "Manual on");
  hear_request(bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x01\x40\x00\x01\x19\x55\x3E\x91\x00\x3F"));
  answered(bytes_len("\x01\x00\x20\x07\x0F"), "Manual again");
  TEST_Check(objects[OBJ_Manual].value == 0, "Manual off");
}

static void write_refusals(void)
{
  start();
  objects[OBJ_Pump].writable = false;
  static const struct {
    const char *npdu;
    uint8_t len;
    uint8_t error_class;
    uint8_t error_code;
    const char *what;
  } cases[] = {
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x01\x40\x00\x01\x19\x55\x3E\x91\x02\x3F"), 2, 37, "Binary Value 2: value-out-of-range" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x91\x01\x3F"), 2, 9, "Analog Value as Enumerated: invalid-data-type" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x01\x40\x00\x01\x19\x55\x3E\x44\x3F\x80\x00\x00\x3F"), 2, 9, "Binary Value as REAL: invalid-data-type" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x44\x42\x70\x00\x00\x3F"), 2, 9, "two values: invalid-data-type" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x00\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x3F"), 2, 40, "Analog Input: write-access-denied" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x01\x40\x00\x02\x19\x55\x3E\x91\x01\x3F"), 2, 40, "Value object not writable: write-access-denied" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x4D\x3E\x72\x00" "X" "\x3F"), 2, 40, "Object_Name: write-access-denied" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x02\x00\x03\xED\x19\x4D\x3E\x72\x00" "X" "\x3F"), 2, 40, "Device: write-access-denied" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x54\x3E\x91\x00\x3F"), 2, 32, "Polarity of an Analog Value: unknown-property" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x09\x19\x55\x3E\x44\x42\x70\x00\x00\x3F"), 1, 31, "Analog Value 9: unknown-object" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x29\x02\x3E\x44\x42\x70\x00\x00\x3F"), 2, 50, "index: property-is-not-an-array" }
  };
  for(uint8_t i = 0; i < array_len(cases); i++) {
    hear_request(cases[i].npdu, cases[i].len);
    const uint8_t expected[] = {
      0x01, 0x00, 0x50, 0x07, 0x0F, 0x91, cases[i].error_class, 0x91, cases[i].error_code
    };
    answered(expected, sizeof(expected), cases[i].what);
  }
  for(uint8_t i = 0; i < array_len(objects); i++) {
    TEST_Check(objects[i].value == boiler_room[i].value && !objects[i].update,
      "object %u untouched", i);
  }
}

//---------------------------------------------------------------------------------------- Refusals

static void rejects_by_reason(void)
{
  start();
  static const struct {
    const char *npdu;
    uint8_t len;
    uint8_t reason;
    const char *what;
  } cases[] = {
    { bytes_len("\x01\x04\x00\x05\x07\x0E\x0C\x00\x00\x00\x01\x1E\x09\x55\x1F"), 9, "ReadPropertyMultiple: unrecognized-service" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C"), 5, "nothing: missing-required-parameter" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x00\x00\x00\x01"), 5, "no property: missing-required-parameter" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55"), 5, "write without a value: missing-required-parameter" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C\xC4\x00\x00\x00\x01\x19\x55"), 4, "application tag for [0]: invalid-tag" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x00\x00"), 4, "object cut short: invalid-tag" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C\x0B\x00\x00\x01\x19\x55"), 4, "object in three bytes: invalid-tag" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x18"), 4, "property of no bytes: invalid-tag" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00"), 4, "no closing tag [3]: invalid-tag" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x4F"), 4, "closing tag [4] for [3]: invalid-tag" },
    { bytes_len("\x01\x04\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x19\x55\x49\x01"), 7, "field [4] in a read: too-many-arguments" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x3F\x49\x00"), 6, "priority 0: parameter-out-of-range" },
    { bytes_len("\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x3F\x49\x11"), 6, "priority 17: parameter-out-of-range" }
  };
  for(uint8_t i = 0; i < array_len(cases); i++) {
    hear_request(cases[i].npdu, cases[i].len);
    const uint8_t expected[] = { 0x01, 0x00, 0x60, 0x07, cases[i].reason };
    answered(expected, sizeof(expected), cases[i].what);
  }
  TEST_Check(!objects[OBJ_Setpoint].update, "nothing written");
}

// `Object_List` of `count` objects and the Device, asked by a client taking max-APDU `code`
static void list_request(uint8_t count, uint8_t code)
{
  boiler();
  device.objects = many;
  device.object_count = count;
  pass();
  const uint8_t request[] = {
    0x01, 0x04, 0x00, code, 0x07, 0x0C, 0x0C, 0x02, 0x00, 0x03, 0xED, 0x19, 0x4C
  };
  hear_request(request, sizeof(request));
}

// Each object takes 5 bytes of the ack, the rest of it 12
static void abort_when_the_answer_does_not_fit(void)
{
  for(uint8_t i = 0; i < array_len(many); i++) {
    many[i] = (BACNET_Object_t){ .type = BACNET_Type_AnalogInput, .instance = i };
  }
  static const char aborted[] = "\x01\x00\x71\x07\x04";
  uint8_t line[256];
  list_request(37, 2);
  TEST_Equal(UART_HostSent(&port, line), 10 + 2 + 202, "37 objects and the Device: 202 bytes");
  TEST_Equal(line[10], 0x30, "a ComplexACK for a client taking 206");
  list_request(38, 2);
  answered(bytes_len(aborted), "one more: Abort segmentation-not-supported from the server");
  list_request(92, 5);
  UART_HostSent(&port, line);
  TEST_Equal((line[5] << 8) | line[6], 2 + 477, "92 objects: 477 bytes");
  TEST_Equal(line[10], 0x30, "a ComplexACK within 480");
  list_request(93, 5);
  answered(bytes_len(aborted), "93: past 480, though the client takes 1476");
  list_request(1, 6);
  answered(bytes_len(aborted), "reserved max-APDU code 6");
  list_request(1, 15);
  answered(bytes_len(aborted), "reserved max-APDU code 15");
  start();
  hear_request(bytes_len("\x01\x04\x0C\x05\x07\x00\x04\x0C\x0C\x00\x00\x00\x01\x19\x55"));
  answered(bytes_len(aborted), "first segment of a request");
}

//----------------------------------------------------------------------------------------- Network

// Client 192.168.1.10:47808 on network 2001 through the router on MAC 1
static void routed_request_and_priority(void)
{
  start();
  objects[OBJ_Supply].value = 21.5f;
  hear_request(bytes_len("\x01\x0E\x07\xD1\x06\xC0\xA8\x01\x0A\xBA\xC0"
    "\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x19\x55"));
  answered(bytes_len("\x01\x22\x07\xD1\x06\xC0\xA8\x01\x0A\xBA\xC0\xFF"
    "\x30\x07\x0C\x0C\x00\x00\x00\x01\x19\x55\x3E\x44\x41\xAC\x00\x00\x3F"),
    "back to the client by the router, priority critical equipment kept");
  hear_request(bytes_len("\x01\x07\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x19\x55"));
  answered(bytes_len("\x01\x03\x30\x07\x0C\x0C\x00\x00\x00\x01\x19\x55"
    "\x3E\x44\x41\xAC\x00\x00\x3F"), "priority life safety kept");
  hear_request(bytes_len("\x01\x24\x00\x05\x01\x07\xFF"
    "\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x19\x55"));
  sent(MSTP_Type_ReplyPostponed, 1, NULL, 0, "network 5: not this device, the link postpones");
  hear_request(bytes_len("\x01\x0C\x07\xD1\x00\x00\x05\x07\x0C\x0C\x00\x00\x00\x01\x19\x55"));
  sent(MSTP_Type_ReplyPostponed, 1, NULL, 0, "SLEN 0: no client to answer");
  // Request-Master-Key, its bytes read as an APDU would be a Who-Is
  hear_frame(MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, 1, bytes_len("\x01\x80\x10\x08"));
  pass();
  // Who-Is to every device of network 5, for a router to pass on
  hear_frame(MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, 1,
    bytes_len("\x01\x20\x00\x05\x00\xFF\x10\x08"));
  pass();
  token();
  sent(MSTP_Type_PollForMaster, 6, NULL, 0, "no I-Am for either");
}

// A write of the Setpoint by broadcast or outside a Data Expecting Reply, left undone
static void confirmed_by_broadcast_unanswered(void)
{
  start();
  static const char write_60[] =
    "\x01\x04\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x3F";
  hear_frame(MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, 1, bytes_len(write_60));
  pass();
  TEST_Check(quiet(), "to every node");
  hear_frame(MSTP_Type_DataNotExpectingReply, 5, 1, bytes_len(write_60));
  pass();
  TEST_Check(quiet(), "not in a Data Expecting Reply");
  hear_request(bytes_len("\x01\x24\xFF\xFF\x00\xFF"
    "\x00\x05\x07\x0F\x0C\x00\x80\x00\x01\x19\x55\x3E\x44\x42\x70\x00\x00\x3F"));
  sent(MSTP_Type_ReplyPostponed, 1, NULL, 0, "to every network: the link postpones");
  TEST_Check(objects[OBJ_Setpoint].value == 55 && !objects[OBJ_Setpoint].update,
    "Setpoint untouched");
}

//------------------------------------------------------------------------------------------ Client

// Master on MAC 6, the device the node asks: it answers a poll, gives the token back,
// and answers a request with `answer`, the invoke ID taken from the request.
static struct {
  const char *answer;
  uint16_t answer_len;
  uint8_t source;  // MAC the answer comes from, `6` when left `0`
  uint8_t shift;   // added to the invoke ID of the answer
  bool postpone;   // Reply Postponed first, the answer with the next token
  bool silent;     // no answer at all
  bool owed;       // answer postponed, not sent yet
  bool busy;       // a second call of the client refused while the first waits
  uint8_t request[32];
  uint16_t request_len;
} peer;

static void peer_answers(const char *npdu, uint16_t len)
{
  peer.answer = npdu;
  peer.answer_len = len;
}

// Answer frame to the request the peer holds
static uint16_t peer_answer(uint8_t *bytes)
{
  uint8_t npdu[64];
  memcpy(npdu, peer.answer, peer.answer_len);
  npdu[3] = (uint8_t)(peer.request[4] + peer.shift);
  uint8_t source = peer.source ? peer.source : 6;
  return frame(bytes, MSTP_Type_DataNotExpectingReply, 5, source, npdu, peer.answer_len);
}

// Frames the node puts on the line, as the peer hears them
static void peer_line(void *ctx, const uint8_t *data, uint16_t len)
{
  unused(ctx);
  if(len < 8 || data[3] != 6) return;
  uint8_t bytes[160];
  uint16_t size = 0;
  float value;
  switch(data[2]) {
    case MSTP_Type_PollForMaster:
      hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 6, NULL, 0);
      return;
    case MSTP_Type_Token:
      if(peer.owed) size = peer_answer(bytes);
      peer.owed = false;
      size += frame(&bytes[size], MSTP_Type_Token, 5, 6, NULL, 0);
      hear(bytes, size);
      return;
    case MSTP_Type_DataExpectingReply:
      peer.request_len = len - 10;
      memcpy(peer.request, &data[8], peer.request_len);
      peer.busy = BACNET_Read(&device, 6, BACNET_Type_AnalogInput, 1, &value, 100) == BUSY;
      if(peer.silent) return;
      if(peer.postpone) {
        peer.owed = true;
        hear_frame(MSTP_Type_ReplyPostponed, 5, 6, NULL, 0);
        return;
      }
      hear(bytes, peer_answer(bytes));
      return;
  }
}

// The device's thread, a task each millisecond, so a call of the client advances
static void loop_task(void *arg)
{
  unused(arg);
  BACNET_Loop(&device);
  TASK_Add(&loop_task, NULL, 1);
}

// Node 5 in a ring with master 6, the token on its way to the node
static void ring(void)
{
  start();
  memset(&peer, 0, sizeof(peer));
  UART_HostAttach(&port, &peer_line, NULL);
  TASK_Add(&loop_task, NULL, 1);
  hear_frame(MSTP_Type_Token, 5, 6, NULL, 0);
}

static void client_reads_and_writes(void)
{
  ring();
  float value = 0;
  peer_answers(bytes_len("\x01\x00\x30\x00\x0C\x0C\x00\x00\x00\x01\x19\x55"
    "\x3E\x44\x41\xAC\x00\x00\x3F"));
  TEST_Check(!BACNET_Read(&device, 6, BACNET_Type_AnalogInput, 1, &value, 1000), "AI read");
  TEST_Equal(value, 21.5f, "21.5");
  TEST_Equal(peer.request_len, 13, "request length");
  TEST_Bytes(peer.request, "\x01\x04\x00\x03\x01\x0C\x0C\x00\x00\x00\x01\x19\x55", 13,
    "ReadProperty, invoke ID 1");
  TEST_Check(peer.busy, "a second call refused while the first waits");
  peer_answers(bytes_len("\x01\x00\x30\x00\x0C\x0C\x01\x40\x00\x01\x19\x55"
    "\x3E\x91\x01\x3F"));
  TEST_Check(!BACNET_Read(&device, 6, BACNET_Type_BinaryValue, 1, &value, 1000), "BV read");
  TEST_Equal(value, 1.0f, "active");
  peer_answers(bytes_len("\x01\x00\x20\x00\x0F"));
  TEST_Check(!BACNET_Write(&device, 6, BACNET_Type_AnalogValue, 1, 60, 1000), "AV written");
  TEST_Equal(peer.request_len, 20, "request length");
  TEST_Bytes(peer.request, "\x01\x04\x00\x03\x03\x0F\x0C\x00\x80\x00\x01\x19\x55"
    "\x3E\x44\x42\x70\x00\x00\x3F", 20, "WriteProperty of a REAL, invoke ID 3");
  TEST_Check(!BACNET_Write(&device, 6, BACNET_Type_BinaryValue, 1, 1, 1000), "BV written");
  TEST_Equal(peer.request_len, 17, "request length");
  TEST_Bytes(peer.request, "\x01\x04\x00\x03\x04\x0F\x0C\x01\x40\x00\x01\x19\x55"
    "\x3E\x91\x01\x3F", 17, "WriteProperty of an Enumerated");
}

// Reply Postponed, then the answer with the next token of the peer
static void client_waits_for_a_postponed_answer(void)
{
  ring();
  peer.postpone = true;
  peer_answers(bytes_len("\x01\x00\x30\x00\x0C\x0C\x00\x00\x00\x01\x19\x55"
    "\x3E\x44\x41\xAC\x00\x00\x3F"));
  float value = 0;
  TEST_Check(!BACNET_Read(&device, 6, BACNET_Type_AnalogInput, 1, &value, 1000), "answered");
  TEST_Equal(value, 21.5f, "21.5");
}

// Every answer but an ack is an `ERR`, and so is no answer in time
static void client_refused(void)
{
  ring();
  static const struct {
    const char *npdu;
    uint8_t len;
    const char *what;
  } answers[] = {
    { bytes_len("\x01\x00\x50\x00\x0C\x91\x01\x91\x1F"), "Error unknown-object" },
    { bytes_len("\x01\x00\x60\x00\x05"), "Reject missing-required-parameter" },
    { bytes_len("\x01\x00\x71\x00\x04"), "Abort segmentation-not-supported" },
    { bytes_len("\x01\x00\x20\x00\x0C"), "SimpleACK to a read" },
    { bytes_len("\x01\x00\x20\x00\x0F"), "SimpleACK of a write to a read" },
    { bytes_len("\x01\x00\x30\x00\x0F\x0C\x00\x00\x00\x01\x19\x55\x3E\x44\x41\xAC\x00\x00\x3F"), "ComplexACK of another service" },
    { bytes_len("\x01\x00\x30\x00\x0C\x0C\x00\x00\x00\x01\x19\x55\x3E\x44\x41\xAC\x00\x00"), "ack without its closing tag" },
    { bytes_len("\x01\x00\x30\x00\x0C\x0C\x00\x00\x00\x01\x19\x55\x3E\x73\x00\x41\x42\x3F"), "a string read" }
  };
  float value = 7;
  for(uint8_t i = 0; i < array_len(answers); i++) {
    peer_answers(answers[i].npdu, answers[i].len);
    status_t status = BACNET_Read(&device, 6, BACNET_Type_AnalogInput, 1, &value, 1000);
    TEST_Check(status == ERR, "%s: ERR", answers[i].what);
  }
  TEST_Equal(value, 7.0f, "value left alone");
  peer_answers(bytes_len("\x01\x00\x20\x00\x0F"));
  peer.silent = true;
  uint64_t from = tick_now();
  TEST_Check(BACNET_Write(&device, 6, BACNET_Type_BinaryValue, 1, 1, 300) == ERR, "no answer");
  TEST_Check(tick_now() - from >= 300, "the whole 300ms waited");
  peer.silent = false;
  peer.shift = 1;
  TEST_Check(BACNET_Write(&device, 6, BACNET_Type_BinaryValue, 1, 1, 300) == ERR,
    "answer to another invoke ID");
  peer.shift = 0;
  peer.source = 7;
  TEST_Check(BACNET_Write(&device, 6, BACNET_Type_BinaryValue, 1, 1, 300) == ERR,
    "answer from another device");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(example_of_the_plan);
  TEST(examples_of_annex_f);
  TEST(who_is_answered_at_the_token);
  TEST(reads_device_properties);
  TEST(reads_object_properties);
  TEST(unknown_objects_and_properties);
  TEST(object_list_by_index);
  TEST(device_4194303_in_a_read);
  TEST(database_revision_follows_the_table);
  TEST(writes_of_each_type);
  TEST(write_refusals);
  TEST(rejects_by_reason);
  TEST(abort_when_the_answer_does_not_fit);
  TEST(routed_request_and_priority);
  TEST(confirmed_by_broadcast_unanswered);
  TEST(client_reads_and_writes);
  TEST(client_waits_for_a_postponed_answer);
  TEST(client_refused);
}

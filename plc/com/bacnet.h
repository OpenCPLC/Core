// plc/com/bacnet.h

#ifndef BACNET_H_
#define BACNET_H_

#include <stdbool.h>
#include <stdint.h>

#include "mstp.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef BACNET_VENDOR_ID
  // `Vendor_Identifier` of the device, the number ASHRAE assigns to its maker
  #define BACNET_VENDOR_ID 0
#endif

#ifndef BACNET_VENDOR_NAME
  // `Vendor_Name` of the device
  #define BACNET_VENDOR_NAME "OpenCPLC"
#endif

//------------------------------------------------------------------------------------------- Types

// Object types, numbers of the standard
typedef enum {
  BACNET_Type_AnalogInput = 0,
  BACNET_Type_AnalogValue = 2,
  BACNET_Type_BinaryInput = 3,
  BACNET_Type_BinaryValue = 5,
  BACNET_Type_Device = 8,
} BACNET_Type_t;

// Engineering units, numbers of the standard; any other of its numbers works cast to the type
typedef enum {
  BACNET_Unit_Milliamperes = 2,
  BACNET_Unit_Amperes = 3,
  BACNET_Unit_Ohms = 4,
  BACNET_Unit_Volts = 5,
  BACNET_Unit_KilowattHours = 19,
  BACNET_Unit_Hertz = 27,
  BACNET_Unit_PercentRelativeHumidity = 29,
  BACNET_Unit_Watts = 47,
  BACNET_Unit_Kilowatts = 48,
  BACNET_Unit_Pascals = 53,
  BACNET_Unit_Kilopascals = 54,
  BACNET_Unit_Bars = 55,
  BACNET_Unit_DegreesCelsius = 62,
  BACNET_Unit_DegreesFahrenheit = 64,
  BACNET_Unit_Hours = 71,
  BACNET_Unit_Minutes = 72,
  BACNET_Unit_Seconds = 73,
  BACNET_Unit_MetersPerSecond = 74,
  BACNET_Unit_Liters = 82,
  BACNET_Unit_LitersPerMinute = 88,
  BACNET_Unit_NoUnits = 95,
  BACNET_Unit_Percent = 98,
  BACNET_Unit_RevolutionsPerMinute = 104,
  BACNET_Unit_Kilohms = 122,
  BACNET_Unit_Millivolts = 124,
  BACNET_Unit_CubicMetersPerHour = 135,
} BACNET_Unit_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief One object of the device, like a Modbus register.
 * @param[in] type Analog or Binary, Input or Value
 * @param[in] instance `0`–`4194302`, unique within its type
 * @param[in] name `Object_Name`, unique in the device
 * @param[in] units Units of an analog object
 * @param[in] writable Value object a client may write
 * @param[in,out] value `Present_Value`, `0` or `1` for a binary object
 * @param[in,out] update Set by a client write, the program clears it
 */
typedef struct {
  BACNET_Type_t type;
  uint32_t instance;
  const char *name;
  BACNET_Unit_t units;
  bool writable;
  float value;
  bool update;
} BACNET_Object_t;

/**
 * @brief BACnet device on an MS/TP link: a server of its objects and a client of other devices.
 * @param[in] mstp Link of the device
 * @param[in] instance Device instance `0`–`4194302`, unique in the whole BACnet network
 * @param[in] name `Object_Name` of the device, unique in the whole BACnet network
 * @param[in] model `Model_Name`
 * @param[in] firmware `Firmware_Revision`
 * @param[in] version `Application_Software_Version`
 * @param[in] objects Object table
 * @param[in] object_count Objects in the table
 * Internal:
 * @param _revision `Database_Revision`, a CRC of the table
 * @param _asking A request of this device waits for its answer
 * @param _answered The answer came, in `_status` and `_value`
 * @param _status `OK` for an ack, `ERR` for any other answer
 * @param _invoke Invoke ID of the request
 * @param _service Service of the request
 * @param _peer MAC asked
 * @param _value `Present_Value` read
 * @param _init Initialization completed flag
 */
typedef struct {
  MSTP_t *mstp;
  uint32_t instance;
  const char *name;
  const char *model;
  const char *firmware;
  const char *version;
  BACNET_Object_t *objects;
  uint16_t object_count;
  // internal
  uint32_t _revision;
  bool _asking;
  bool _answered;
  status_t _status;
  uint8_t _invoke;
  uint8_t _service;
  uint8_t _peer;
  float _value;
  bool _init;
} BACNET_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Run the device: serve Who-Is, ReadProperty and WriteProperty,
 *   and take the answers to its own requests.
 *   Call it every pass of its own thread, at most 5ms apart.
 * @param[in,out] bacnet Device
 */
void BACNET_Loop(BACNET_t *bacnet);

/**
 * @brief Read `Present_Value` of an object in another device on the line, as a BMS does.
 *   The wait yields, so the call comes from a thread other than the one of `BACNET_Loop`.
 * @param[in,out] bacnet Device that asks, its link carries the request
 * @param[in] mac MAC of the device read
 * @param[in] type Object type
 * @param[in] instance Object instance
 * @param[out] value `Present_Value`, `0` or `1` of a binary object
 * @param[in] timeout_ms Wait for the answer, the wait for the token included
 * @return `OK` on a value, `BUSY` while another request waits, `ERR` on any other answer or none
 */
status_t BACNET_Read(BACNET_t *bacnet, uint8_t mac, BACNET_Type_t type, uint32_t instance,
  float *value, uint32_t timeout_ms);

/**
 * @brief Write `Present_Value` of an object in another device on the line, as a BMS does:
 *   a REAL to an analog object, an Enumerated `0` or `1` to a binary one.
 *   The wait yields, so the call comes from a thread other than the one of `BACNET_Loop`.
 * @param[in,out] bacnet Device that asks, its link carries the request
 * @param[in] mac MAC of the device written
 * @param[in] type Object type
 * @param[in] instance Object instance
 * @param[in] value Value written
 * @param[in] timeout_ms Wait for the answer, the wait for the token included
 * @return `OK` on an ack, `BUSY` while another request waits, `ERR` on any other answer or none
 */
status_t BACNET_Write(BACNET_t *bacnet, uint8_t mac, BACNET_Type_t type, uint32_t instance,
  float value, uint32_t timeout_ms);

//-------------------------------------------------------------------------------------------------
#endif

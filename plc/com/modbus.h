// plc/com/modbus.h

#ifndef MODBUS_H_
#define MODBUS_H_

// Quantity limits from the Modbus application protocol, section 6.
// The field on the wire is `uint16_t` and carries values no legal frame holds.
#define MODBUS_READ_BITS_MAX 2000
#define MODBUS_READ_REGISTERS_MAX 125
#define MODBUS_WRITE_BITS_MAX 1968
#define MODBUS_WRITE_REGISTERS_MAX 123

typedef enum {
  MODBUS_Fnc_Unknown = 0x00,
  MODBUS_Fnc_ReadBits = 0x01,
  MODBUS_Fnc_ReadOuts = 0x02,
  MODBUS_Fnc_ReadHoldingRegisters = 0x03,
  MODBUS_Fnc_ReadInputRegisters = 0x04,
  MODBUS_Fnc_PresetBit = 0x05,
  MODBUS_Fnc_PresetRegister = 0x06,
  MODBUS_Fnc_WriteBits = 0x0F,
  MODBUS_Fnc_WriteRegisters = 0x10
} MODBUS_Fnc_t;

#endif

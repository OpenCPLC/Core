#ifndef MODBUS_H_
#define MODBUS_H_

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
} MODBUS_Fnc_e;

#endif
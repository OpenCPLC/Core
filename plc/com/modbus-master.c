#include "modbus-master.h"

//------------------------------------------------------------------------------------------------- COMMON

static MODBUS_Error_e MODBUS_SendRead(UART_t *uart, uint8_t addr, MODBUS_Fnc_e fnc, uint8_t *buffer, uint16_t tx_length, uint16_t rx_length, uint32_t timeout_ms)
{
  CRC_Append(&crc16_modbus, buffer, tx_length - 2);
  UART_Clear(uart);
  UART_Send(uart, buffer, tx_length);
  uint32_t wait_ms;
  wait_ms = 2 * UART_CalcTime_ms(uart, tx_length) + 10;
  if(timeout(wait_ms, WAIT_&UART_SendCompleted, uart)) return MODBUS_Error_Sending;
  wait_ms = 2 * UART_CalcTime_ms(uart, rx_length) + 10 + timeout_ms;
  if(timeout(wait_ms, WAIT_&UART_Size, uart)) return MODBUS_Error_Timeout;
  uint16_t size = UART_Size(uart);
  if(size != rx_length) {
    UART_Clear(uart);
    return MODBUS_Error_Length;
  }
  UART_Read(uart, buffer);
  if(buffer[0] != addr) return MODBUS_Error_Adrress;
  if(buffer[1] != fnc) return MODBUS_Error_Function;
  if(CRC_Error(&crc16_modbus, buffer, size)) return MODBUS_Error_Crc;
  return MODBUS_Ok;
}

//------------------------------------------------------------------------------------------------- BIRS

#define MODBUS_READBITS_TXLEN 9

static MODBUS_Error_e MODBUS_ReadBin(UART_t *uart, uint8_t addr, MODBUS_Fnc_e fnc, uint16_t start, uint16_t count, bool *memory, uint32_t timeout_ms)
{
  if(UART_IsBusy(uart)) return MODBUS_Error_Uart;
  uint8_t databyte_count = (count + 7) / 8;
  uint16_t rx_lenght = databyte_count + 5;
  uint16_t len = rx_lenght > MODBUS_READBITS_TXLEN ? rx_lenght : MODBUS_READBITS_TXLEN;
  uint8_t *buffer = (uint8_t *)heap_new(len);
  buffer[0] = addr;
  buffer[1] = fnc;
  buffer[2] = (uint8_t)(start >> 8);
  buffer[3] = (uint8_t)start;
  buffer[4] = (uint8_t)(count >> 8);
  buffer[5] = (uint8_t)count;
  buffer[6] = databyte_count;
  MODBUS_Error_e error = MODBUS_SendRead(uart, addr, fnc, buffer, MODBUS_READBITS_TXLEN, rx_lenght, timeout_ms);
  if(error) return error;
  if(buffer[2] != databyte_count) return MODBUS_Error_Count;
  uint8_t *byte = &buffer[3];
  uint8_t bit = 0;
  while(count) {
    *memory = (*byte >> bit) & 0x01;
    memory++;
    bit++;
    if(bit >= 8) {
      bit = 0;
      byte++;
    }
    count--;
  }
  return MODBUS_Ok;
}

MODBUS_Error_e MODBUS_ReadBits(UART_t *uart, uint8_t addr, uint16_t start, uint16_t count, bool *memory, uint32_t timeout_ms)
{
  MODBUS_Error_e error = MODBUS_ReadBin(uart, addr, MODBUS_Fnc_ReadBits, start, count, memory, timeout_ms);
  heap_clear();
  return error;
}

MODBUS_Error_e MODBUS_ReadOuts(UART_t *uart, uint8_t addr, uint16_t start, uint16_t count, bool *memory, uint32_t timeout_ms)
{
  MODBUS_Error_e error = MODBUS_ReadBin(uart, addr, MODBUS_Fnc_ReadOuts, start, count, memory, timeout_ms);
  heap_clear();
  return error;
}

#define MODBUS_PRESETBIT_TXLEN 7
#define MODBUS_PRESETBIT_RXLEN 6

static MODBUS_Error_e _MODBUS_PresetBit(UART_t *uart, uint8_t addr, uint16_t index, bool value, uint32_t timeout_ms)
{
  if(UART_IsBusy(uart)) return MODBUS_Error_Uart;
  uint8_t *buffer = (uint8_t *)heap_new(MODBUS_PRESETBIT_TXLEN);
  buffer[0] = addr;
  buffer[1] = MODBUS_Fnc_PresetBit;
  buffer[2] = (uint8_t)(index >> 8);
  buffer[3] = (uint8_t)index;
  buffer[4] = value ? 0xFF : 0x00;
  buffer[5] = 0x00;
  MODBUS_Error_e error = MODBUS_SendRead(uart, addr, MODBUS_Fnc_PresetBit, buffer, MODBUS_PRESETBIT_TXLEN, MODBUS_PRESETBIT_RXLEN, timeout_ms);
  if(error) return error;
  if(((uint16_t)buffer[2] << 8 | buffer[3]) != index) return MODBUS_Error_Index;
  if((buffer[4] ? true : false) != value) return MODBUS_Error_Value;
  return MODBUS_Ok;
}

MODBUS_Error_e MODBUS_PresetBit(UART_t *uart, uint8_t addr, uint16_t index, bool value, uint32_t timeout_ms)
{
  MODBUS_Error_e error = _MODBUS_PresetBit(uart, addr, index, value, timeout_ms);
  heap_clear();
  return error;
}

#define MODBUS_WRITEBITS_RXLEN 8

static MODBUS_Error_e _MODBUS_WriteBits(UART_t *uart, uint8_t addr, uint16_t count, uint16_t start, bool *memory, uint32_t timeout_ms)
{
  if(UART_IsBusy(uart)) return MODBUS_Error_Uart;
  uint16_t databyte_count = (count + 7) / 8;
  uint16_t tx_lenght = databyte_count + 6;
  uint16_t len = tx_lenght > MODBUS_WRITEBITS_RXLEN ? tx_lenght : MODBUS_WRITEBITS_RXLEN;
  uint8_t *buffer = (uint8_t *)heap_new(len);
  buffer[0] = addr;
  buffer[1] = MODBUS_Fnc_WriteBits;
  buffer[2] = (uint8_t)(start >> 8);
  buffer[3] = (uint8_t)start;
  buffer[4] = (uint8_t)(count >> 8);
  buffer[5] = (uint8_t)count;
  buffer[6] = (uint8_t)databyte_count;
  uint8_t *buff = &buffer[7];
  uint8_t value = 0;
  uint8_t bit = 0;
  while(count) {
    if(*memory) value |= (1 << bit);
    memory++;
    bit++;
    if(bit >= 8) {
      bit = 0;
      *buff++ = value;
      value = 0;
    }
    count--;
  }
  if(bit != 0) {
    *buff++ = value;
  }
  MODBUS_Error_e error = MODBUS_SendRead(uart, addr, MODBUS_Fnc_WriteBits, buffer, tx_lenght, MODBUS_WRITEBITS_RXLEN, timeout_ms);
  if(error) return error;
  if(((uint16_t)buffer[2] << 8 | buffer[3]) != start) return MODBUS_Error_Start;
  if(((uint16_t)buffer[4] << 8 | buffer[5]) != count) return MODBUS_Error_Count;
  return MODBUS_Ok;
}

MODBUS_Error_e MODBUS_WriteBits(UART_t *uart, uint8_t addr, uint16_t count, uint16_t start, bool *memory, uint32_t timeout_ms)
{
  MODBUS_Error_e error = _MODBUS_WriteBits(uart, addr, count, start, memory, timeout_ms);
  heap_clear();
  return error;
}

//------------------------------------------------------------------------------------------------- READ-REGS

#define MODBUS_READREGS_TXLEN 8

static MODBUS_Error_e MODBUS_ReadRegs(UART_t *uart, uint8_t addr, MODBUS_Fnc_e fnc, uint16_t start, uint16_t count, uint16_t *memory, uint32_t timeout_ms)
{
  if(UART_IsBusy(uart)) return MODBUS_Error_Uart;
  uint16_t databyte_count = 2 * count;
  uint16_t rx_lenght = databyte_count + 5;
  uint16_t len = rx_lenght > MODBUS_READREGS_TXLEN ? rx_lenght : MODBUS_READREGS_TXLEN;
  uint8_t *buffer = (uint8_t *)heap_new(len);
  buffer[0] = addr;
  buffer[1] = fnc;
  buffer[2] = (uint8_t)(start >> 8);
  buffer[3] = (uint8_t)start;
  buffer[4] = (uint8_t)(count >> 8);
  buffer[5] = (uint8_t)count;
  MODBUS_Error_e error = MODBUS_SendRead(uart, addr, fnc, buffer, MODBUS_READREGS_TXLEN, rx_lenght, timeout_ms);
  if(error) return error;
  if(buffer[2] != databyte_count) return MODBUS_Error_Count;
  uint8_t *buff = &buffer[3];
  while(count) {
    *memory = ((uint16_t)*buff << 8) | *(buff + 1);
    buff += 2;
    memory++;
    count--;
  }
  return MODBUS_Ok;
}

MODBUS_Error_e MODBUS_ReadInputRegisters(UART_t *uart, uint8_t addr, uint16_t start, uint16_t count, uint16_t *memory, uint32_t timeout_ms)
{
  MODBUS_Error_e error = MODBUS_ReadRegs(uart, addr, MODBUS_Fnc_ReadInputRegisters, start, count, memory, timeout_ms);
  heap_clear();
  return error;
}

MODBUS_Error_e MODBUS_ReadHoldingRegisters(UART_t *uart, uint8_t addr, uint16_t start, uint16_t count, uint16_t *memory, uint32_t timeout_ms)
{
  MODBUS_Error_e error = MODBUS_ReadRegs(uart, addr, MODBUS_Fnc_ReadHoldingRegisters, start, count, memory, timeout_ms);
  heap_clear();
  return error;
}

//------------------------------------------------------------------------------------------------- WRITE-REGS

#define MODBUS_PRESSREG_TXLEN 8
#define MODBUS_PRESSREG_RXLEN 8

static MODBUS_Error_e _MODBUS_PresetRegister(UART_t *uart, uint8_t addr, uint16_t index, uint16_t value, uint32_t timeout_ms)
{
  if(UART_IsBusy(uart)) return MODBUS_Error_Uart;
  uint8_t *buffer = (uint8_t *)heap_new(MODBUS_PRESSREG_TXLEN);
  buffer[0] = addr;
  buffer[1] = MODBUS_Fnc_PresetRegister;
  buffer[2] = (uint8_t)(index >> 8);
  buffer[3] = (uint8_t)index;
  buffer[4] = (uint8_t)(value >> 8);
  buffer[5] = (uint8_t)value;
  MODBUS_Error_e error = MODBUS_SendRead(uart, addr, MODBUS_Fnc_PresetRegister, buffer, MODBUS_PRESSREG_TXLEN, MODBUS_PRESSREG_RXLEN, timeout_ms);
  if(error) return error;
  if(((uint16_t)buffer[2] << 8 | buffer[3]) != index) return MODBUS_Error_Index;
  if(((uint16_t)buffer[4] << 8 | buffer[5]) != value) return MODBUS_Error_Value;
  return MODBUS_Ok;
}

MODBUS_Error_e MODBUS_PresetRegister(UART_t *uart, uint8_t addr, uint16_t index, uint16_t value, uint32_t timeout_ms)
{
  MODBUS_Error_e error = _MODBUS_PresetRegister(uart, addr, index, value, timeout_ms);
  heap_clear();
  return error;
}

#define MODBUS_WRITEREGS_RXLEN 7

static MODBUS_Error_e _MODBUS_WriteRegisters(UART_t *uart, uint8_t addr, uint16_t start, uint16_t count, uint16_t *memory, uint32_t timeout_ms)
{
  if(UART_IsBusy(uart)) return MODBUS_Error_Uart;
  uint16_t databyte_count = 2 * count;
  uint16_t tx_lenght = databyte_count + 9;
  uint8_t *buffer = (uint8_t *)heap_new(tx_lenght);
  buffer[0] = addr;
  buffer[1] = MODBUS_Fnc_WriteRegisters;
  buffer[2] = (uint8_t)(start >> 8);
  buffer[3] = (uint8_t)start;
  buffer[4] = (uint8_t)(count >> 8);
  buffer[5] = (uint8_t)count;
  buffer[6] = (uint8_t)databyte_count;
  uint8_t *buff = &buffer[7];
  while(count) {
    *buff++ = (uint8_t)(*memory >> 8);
    *buff++ = (uint8_t)*memory;
    memory++;
    count--;
  }
  MODBUS_Error_e error = MODBUS_SendRead(uart, addr, MODBUS_Fnc_WriteRegisters, buffer, tx_lenght, MODBUS_WRITEREGS_RXLEN, timeout_ms);
  if(error) return error; 
  if(((uint16_t)buffer[2] << 8 | buffer[3]) != start) return MODBUS_Error_Start;
  if(((uint16_t)buffer[4] << 8 | buffer[5]) != count) return MODBUS_Error_Count;
  return MODBUS_Ok;
}

MODBUS_Error_e MODBUS_WriteRegisters(UART_t *uart, uint8_t addr, uint16_t start, uint16_t count, uint16_t *memory, uint32_t timeout_ms)
{
  MODBUS_Error_e error = _MODBUS_WriteRegisters(uart, addr, start, count, memory, timeout_ms);
  heap_clear();
  return error;
}

//-------------------------------------------------------------------------------------------------

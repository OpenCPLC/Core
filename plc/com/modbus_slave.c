// plc/com/modbus_slave.c

#include "modbus_slave.h"

#include "heap.h"

MODBUS_Status_t MODBUS_Loop(MODBUS_Slave_t *modbus)
{
  if(UART_SendActive(modbus->uart)) return MODBUS_Status_UartBusy;
  while(UART_MessageCount(modbus->uart) > 1) UART_Skip(modbus->uart);
  uint16_t size_rx = UART_Size(modbus->uart);
  if(!size_rx) return MODBUS_Status_None;
  heap_free((void *)modbus->buffer_rx);
  modbus->buffer_rx = (uint8_t *)heap_alloc(size_rx);
  size_rx = UART_Read(modbus->uart, modbus->buffer_rx);
  if(size_rx <= 5) return MODBUS_Status_TooShort;
  if(CRC_Error(&crc16_modbus, modbus->buffer_rx, size_rx)) return MODBUS_Status_InvalidCRC;
  if(modbus->buffer_rx[0] != modbus->address) return MODBUS_Status_Ignored;
  uint16_t reg, start, count, value;
  uint8_t bit;
  heap_free((void *)modbus->buffer_tx);
  modbus->buffer_tx = NULL; // Holds a live block or `NULL`, never a freed pointer
  uint16_t size_tx = 0;
  MODBUS_Fnc_t function_code = (MODBUS_Fnc_t)modbus->buffer_rx[1];
  switch(function_code) {
    case MODBUS_Fnc_ReadBits:
    case MODBUS_Fnc_ReadOuts:
      if(size_rx != 8) return MODBUS_Status_InvalidSize;
      start = (modbus->buffer_rx[2] << 8) | modbus->buffer_rx[3];
      count = (modbus->buffer_rx[4] << 8) | modbus->buffer_rx[5];
      if(!count || count > MODBUS_READ_BITS_MAX) return MODBUS_Status_InvalidSize;
      size_tx = ((count + 7) / 8) + 3;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      modbus->buffer_tx[0] = modbus->buffer_rx[0];
      modbus->buffer_tx[1] = modbus->buffer_rx[1];
      modbus->buffer_tx[2] = size_tx - 3;
      reg = start / 16;
      bit = start % 16;
      uint16_t rcount = count / 8;
      if(count % 8) rcount++;
      uint16_t high, low;
      for(uint16_t i = 0; i < rcount; i++) {
        if(reg < modbus->reg_count) {
          high = modbus->reg_read[reg] >> bit;
          if(reg + 1 < modbus->reg_count) low = modbus->reg_read[reg + 1] << (16 - bit);
          else low = 0;
        }
        else {
          high = 0;
          low = 0;
        }
        modbus->buffer_tx[i + 3] = high | low;
        bit += 8;
        if(bit > 15) {
          bit -= 16;
          reg++;
        }
      }
      break;
    case MODBUS_Fnc_ReadHoldingRegisters:
    case MODBUS_Fnc_ReadInputRegisters:
      if(size_rx != 8) return MODBUS_Status_InvalidSize;
      start = (modbus->buffer_rx[2] << 8) | modbus->buffer_rx[3];
      count = (modbus->buffer_rx[4] << 8) | modbus->buffer_rx[5];
      if(!count || count > MODBUS_READ_REGISTERS_MAX) return MODBUS_Status_InvalidSize;
      size_tx = 2 * count + 3;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      modbus->buffer_tx[0] = modbus->buffer_rx[0];
      modbus->buffer_tx[1] = modbus->buffer_rx[1];
      modbus->buffer_tx[2] = size_tx - 3;
      for(uint16_t i = 0; i < count; i++) {
        if(start + i < modbus->reg_count) value = modbus->reg_read[start + i];
        else value = 0;
        modbus->buffer_tx[3 + (i * 2)] = (uint8_t)(value >> 8);
        modbus->buffer_tx[3 + (i * 2) + 1] = (uint8_t)value;
      }
      break;
    case MODBUS_Fnc_PresetBit:
      if(size_rx != 8) return MODBUS_Status_InvalidSize;
      size_tx = 6;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      for(uint16_t i = 0; i < size_tx; i++) modbus->buffer_tx[i] = modbus->buffer_rx[i];
      start = (modbus->buffer_rx[2] << 8) | (modbus->buffer_rx[3]);
      reg = start / 16;
      bit = start % 16;
      // Both reads below index `reg_read` with a frame-derived `reg`
      if(reg >= modbus->reg_count) break;
      value = modbus->buffer_rx[4] ?
        modbus->reg_read[reg] | (1 << bit) : modbus->reg_read[reg] & ~(1 << bit);
      if((!modbus->write_mask || modbus->write_mask[reg]) && modbus->reg_read[reg] != value) {
        modbus->reg_write[reg] = value;
        modbus->update_flag[reg] = true;
        modbus->update_any = true;
      }
      break;
    case MODBUS_Fnc_PresetRegister:
      if(size_rx != 8) return MODBUS_Status_InvalidSize;
      size_tx = 6;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      for(uint16_t i = 0; i < size_tx; i++) modbus->buffer_tx[i] = modbus->buffer_rx[i];
      reg = (modbus->buffer_rx[2] << 8) | (modbus->buffer_rx[3]);
      value = (modbus->buffer_rx[4] << 8) | (modbus->buffer_rx[5]);
      if(reg < modbus->reg_count && (!modbus->write_mask || modbus->write_mask[reg]) &&
        modbus->reg_read[reg] != value) {
        modbus->reg_write[reg] = value;
        modbus->update_flag[reg] = true;
        modbus->update_any = true;
      }
      break;
    case MODBUS_Fnc_WriteBits:
      if(size_rx < 10 || (size_rx != modbus->buffer_rx[6] + 9))
        return MODBUS_Status_InvalidSize;
      size_tx = 6;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      for(uint16_t i = 0; i < size_tx; i++) modbus->buffer_tx[i] = modbus->buffer_rx[i];
      start = (modbus->buffer_rx[2] << 8) | (modbus->buffer_rx[3]);
      count = (modbus->buffer_rx[4] << 8) | (modbus->buffer_rx[5]);
      // Byte count and coil count describe the same payload, they agree on a legal frame
      if(!count || count > MODBUS_WRITE_BITS_MAX) return MODBUS_Status_InvalidSize;
      if(modbus->buffer_rx[6] != (count + 7) / 8) return MODBUS_Status_InvalidSize;
      const uint8_t *coils = &modbus->buffer_rx[7];
      reg = start / 16;
      bit = start % 16;
      // One register per pass, the stored value carries the bits outside the coil range
      uint16_t idx = 0;
      while(idx < count && reg < modbus->reg_count) {
        value = modbus->reg_read[reg];
        while(bit < 16 && idx < count) {
          if((coils[idx / 8] >> (idx % 8)) & 1) value |= (1 << bit);
          else value &= ~(1 << bit);
          bit++;
          idx++;
        }
        bit = 0;
        if((!modbus->write_mask || modbus->write_mask[reg]) &&
          modbus->reg_read[reg] != value) {
          modbus->reg_write[reg] = value;
          modbus->update_flag[reg] = true;
          modbus->update_any = true;
        }
        reg++;
      }
      break;
    case MODBUS_Fnc_WriteRegisters:
      count = (modbus->buffer_rx[4] << 8) | modbus->buffer_rx[5];
      if(!count || count > MODBUS_WRITE_REGISTERS_MAX) return MODBUS_Status_InvalidSize;
      if(size_rx < 11 || !(size_rx % 2) || (count != (size_rx - 9) / 2) ||
        count != modbus->buffer_rx[6] / 2) return MODBUS_Status_InvalidSize;
      size_tx = 6;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      for(uint16_t i = 0; i < size_tx; i++) modbus->buffer_tx[i] = modbus->buffer_rx[i];
      start = (modbus->buffer_rx[2] << 8) | modbus->buffer_rx[3];
      for(uint16_t i = 0; i < count; i++) {
        value = (modbus->buffer_rx[7 + (2 * i)] << 8) | modbus->buffer_rx[8 + (2 * i)];
        if(start + i < modbus->reg_count &&
          (!modbus->write_mask || modbus->write_mask[start + i]) &&
          modbus->reg_read[start + i] != value) {
          modbus->reg_write[start + i] = value;
          modbus->update_flag[start + i] = true;
          modbus->update_any = true;
        }
      }
      break;
    default:
      size_tx = size_rx;
      modbus->buffer_tx = (uint8_t *)heap_alloc(size_tx + 2);
      for(uint16_t i = 0; i < size_tx; i++) modbus->buffer_tx[i] = modbus->buffer_rx[i];
      break;
  }
  if(size_tx) {
    size_tx = CRC_Append(&crc16_modbus, modbus->buffer_tx, size_tx);
    if(UART_Send(modbus->uart, modbus->buffer_tx, size_tx)) return MODBUS_Status_SendError;
  }
  return MODBUS_Status_Handled;
}

//-------------------------------------------------------------------------------------------------

bool MODBUS_HasUpdate(MODBUS_Slave_t *modbus)
{
  if(modbus->update_any) {
    modbus->update_any = false;
    return true;
  }
  return false;
}

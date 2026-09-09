// plc/com/one_wire.c

#include "one_wire.h"

//---------------------------------------------------------------------------------------- Internal

static void write_bit(WIRE_t *onewire, bool value)
{
  GPIO_Rst(onewire->gpio);
  GPIO_Mode(onewire->gpio, GPIO_Mode_Output);
  if(value) {
    sleep_us(10);
    GPIO_Set(onewire->gpio);
    sleep_us(55);
  }
  else {
    sleep_us(65);
    GPIO_Set(onewire->gpio);
    sleep_us(5);
  }
}

static bool read_bit(WIRE_t *onewire)
{
  GPIO_Rst(onewire->gpio);
  GPIO_Mode(onewire->gpio, GPIO_Mode_Output);
  sleep_us(3);
  GPIO_Mode(onewire->gpio, GPIO_Mode_Input);
  sleep_us(10);
  bool value = GPIO_In(onewire->gpio);
  sleep_us(53);
  return value;
}

//--------------------------------------------------------------------------------------------- API

bool WIRE_Init(WIRE_t *onewire)
{
  onewire->gpio->out_type = GPIO_OutType_OpenDrain;
  onewire->gpio->speed = GPIO_Speed_VeryHigh;
  onewire->gpio->mode = GPIO_Mode_Input;
  onewire->gpio->set = true;
  GPIO_Init(onewire->gpio);
  return !timeout(250, WAIT_&GPIO_In, onewire->gpio);
}

bool WIRE_Reset(WIRE_t *onewire)
{
  GPIO_Mode(onewire->gpio, GPIO_Mode_Input);
  GPIO_Init(onewire->gpio);
  if(timeout(250, WAIT_&GPIO_In, onewire->gpio)) return false;
  GPIO_Rst(onewire->gpio);
  GPIO_Mode(onewire->gpio, GPIO_Mode_Output);
  sleep_us(480);
  GPIO_Set(onewire->gpio);
  GPIO_Mode(onewire->gpio, GPIO_Mode_Input);
  sleep_us(70);
  bool value = !GPIO_In(onewire->gpio);
  sleep_us(470);
  return value;
}

void WIRE_Write(WIRE_t *onewire, uint8_t value)
{
  for(uint8_t mask = 0x01; mask; mask <<= 1) {
    write_bit(onewire, (mask & value) != 0);
  }
  GPIO_Mode(onewire->gpio, GPIO_Mode_Input);
  GPIO_Rst(onewire->gpio);
}

void WIRE_WriteParasitePower(WIRE_t *onewire, uint8_t value)
{
  for(uint8_t mask = 0x01; mask; mask <<= 1) {
    write_bit(onewire, (mask & value) != 0);
  }
}

uint8_t WIRE_Read(WIRE_t *onewire)
{
  uint8_t value = 0;
  for(uint8_t mask = 0x01; mask; mask <<= 1) {
    if(read_bit(onewire)) value |= mask;
  }
  return value;
}

void WIRE_Select(WIRE_t *onewire, uint8_t *addr)
{
  WIRE_Write(onewire, ONEWIRE_CMD_MATCH_ROM);
  for(uint8_t i = 0; i < 8; i++) WIRE_Write(onewire, addr[i]);
}

void WIRE_Skip(WIRE_t *onewire)
{
  WIRE_Write(onewire, ONEWIRE_CMD_SKIP_ROM);
}

bool WIRE_Search(WIRE_t *onewire, uint8_t *addr)
{
  uint8_t id_bit_i = 1, id_bit, id_bit_comp;
  uint8_t rom_i = 0, last_zero = 0;
  bool res = false; // Search result
  uint8_t mask = 1; // ROM byte mask
  uint8_t dir; // Search direction
  if(!onewire->_last_dev_flag) {
    if(!WIRE_Reset(onewire)) {
      onewire->_last = 0;
      onewire->_last_dev_flag = false;
      onewire->_last_family = 0;
      return false;
    }
    WIRE_Write(onewire, ONEWIRE_CMD_SEARCH_ROM); // Normal search
    do {
      id_bit = read_bit(onewire);
      id_bit_comp = read_bit(onewire);
      if(id_bit == 1 && id_bit_comp == 1) break;
      else {
        if(id_bit != id_bit_comp) dir = id_bit;
        else {
          if(id_bit_i < onewire->_last) {
            dir = ((onewire->_rom[rom_i] & mask) > 0);
          }
          else {
            dir = (id_bit_i == onewire->_last);
          }
          if(dir == 0) {
            last_zero = id_bit_i;
            if(last_zero < 9) onewire->_last_family = last_zero;
          }
        }
        if(dir == 1) onewire->_rom[rom_i] |= mask;
        else onewire->_rom[rom_i] &= ~mask;
        // Search ROM spends three slots per bit: value, complement, then the direction
        write_bit(onewire, dir);
        id_bit_i++;
        mask <<= 1;
        if(mask == 0) {
          rom_i++;
          mask = 1;
        }
      }
    }
    while(rom_i < 8);

    if(id_bit_i >= 65) {
      onewire->_last = last_zero;
      if(onewire->_last == 0) {
        onewire->_last_dev_flag = true;
      }
      res = true;
    }
  }
  if(!res || !onewire->_rom[0]) {
    onewire->_last = 0;
    onewire->_last_dev_flag = false;
    onewire->_last_family = 0;
    res = false;
  }
  else {
    for(uint8_t i = 0; i < 8; i++) addr[i] = onewire->_rom[i];
  }
  return res;
}

//-------------------------------------------------------------------------------------------------

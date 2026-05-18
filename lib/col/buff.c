// lib/col/buff.c

#include "buff.h"

//------------------------------------------------------------------------------------------------- State

uint16_t BUFF_Size(BUFF_t *buff)
{
  if(buff->_msg_head != buff->_msg_tail) return buff->_msg_size[buff->_msg_tail];
  return 0;
}

uint16_t BUFF_MessageCount(BUFF_t *buff)
{
  if(buff->_msg_head >= buff->_msg_tail) return buff->_msg_head - buff->_msg_tail;
  return BUFF_MSG_LIMIT - buff->_msg_tail + buff->_msg_head;
}

//------------------------------------------------------------------------------------------------- Byte ops

bool BUFF_Append(BUFF_t *buff, uint8_t value)
{
  volatile uint8_t *next = buff->_head + 1;
  if(next >= buff->_end_memory) next = buff->memory;
  // Drop-newest: byte lost, pending data intact
  if(next == buff->_tail) {
    if(buff->Overflow) buff->Overflow();
    return false;
  }
  *buff->_head = value;
  buff->_msg_counter++;
  buff->_head = next;
  if(!buff->console_mode) buff->_echo = buff->_head;
  buff->_break_allow = true;
  return true;
}

bool BUFF_Pop(BUFF_t *buff, uint8_t *value)
{
  if(buff->_msg_counter) {
    buff->_msg_counter--;
    if(!buff->_msg_counter) buff->_break_allow = false;
    bool move_echo = buff->_head == buff->_echo;
    if(buff->_head == buff->memory) buff->_head = buff->_end_memory;
    buff->_head--;
    if(move_echo) buff->_echo = buff->_head;
    if(value) *value = *buff->_head;
    return true;
  }
  return false;
}

bool BUFF_Echo(BUFF_t *buff, char *value)
{
  if(buff->_echo == buff->_head) return false;
  if(value) *value = *buff->_echo;
  buff->_echo++;
  if(buff->_echo >= buff->_end_memory) buff->_echo = buff->memory;
  return true;
}

//------------------------------------------------------------------------------------------------- Message break

bool BUFF_Break(BUFF_t *buff)
{
  if(!buff->_break_allow) return false;
  uint16_t next = buff->_msg_head + 1;
  if(next >= BUFF_MSG_LIMIT) next = 0;
  // Drop-newest: message lost, pending queue intact
  if(next == buff->_msg_tail) {
    if(buff->Overflow) buff->Overflow();
    return false;
  }
  buff->_msg_size[buff->_msg_head] = buff->_msg_counter;
  buff->_msg_counter = 0;
  buff->_msg_head = next;
  buff->_break_allow = false;
  return true;
}

//------------------------------------------------------------------------------------------------- Push (console-aware)

bool BUFF_Push(BUFF_t *buff, uint8_t value)
{
  if(buff->console_mode) {
    // ESC sequence: second byte
    if(buff->_esc == 1) {
      if(value == '[' || value == 'O') buff->_esc = 2;
      else buff->_esc = 0;
      return false;
    }
    // ESC sequence: terminator
    if(buff->_esc == 2) {
      if(value >= 0x40 && value <= 0x7E) buff->_esc = 0;
      return false;
    }
    // ESC start
    if(value == 0x1B) {
      buff->_esc = 1;
      return false;
    }
    // Enter: close current message
    if(value == '\r' || value == '\n') {
      if(!buff->_break_allow) return false;
      BUFF_Append(buff, '\n');
      BUFF_Break(buff);
      return true;
    }
    // FF / Ctrl+C: close + skip message
    if(value == '\f' || value == 0x03) {
      BUFF_Append(buff, '\f');
      BUFF_Break(buff);
      BUFF_Skip(buff);
      return true;
    }
    // Ctrl+S (XOFF): ignore
    if(value == 0x13) {
      return false;
    }
  }
  return BUFF_Append(buff, value);
}

//------------------------------------------------------------------------------------------------- Read

uint16_t BUFF_Read(BUFF_t *buff, uint8_t *dst)
{
  uint16_t size = BUFF_Size(buff);
  if(!size) return 0;
  uint16_t n = size;
  while(n) {
    if(dst) *dst++ = *buff->_tail;
    buff->_tail++;
    if(buff->_tail >= buff->_end_memory) buff->_tail = buff->memory;
    n--;
  }
  buff->_msg_tail++;
  if(buff->_msg_tail >= BUFF_MSG_LIMIT) buff->_msg_tail = 0;
  return size;
}

uint16_t BUFF_Peek(BUFF_t *buff, uint8_t *dst)
{
  uint16_t size = BUFF_Size(buff);
  if(!size) return 0;
  uint16_t n = size;
  uint8_t *ptr = (uint8_t*)buff->_tail;
  while(n) {
    if(dst) *dst++ = *ptr;
    ptr++;
    if(ptr >= buff->_end_memory) ptr = buff->memory;
    n--;
  }
  return size;
}

bool BUFF_Skip(BUFF_t *buff)
{
  return BUFF_Read(buff, NULL) ? true : false;
}

char *BUFF_ReadString(BUFF_t *buff)
{
  uint16_t size = BUFF_Size(buff);
  if(!size) return NULL;
  char *str = heap_new(size + 1);
  if(!str) return NULL;
  BUFF_Read(buff, (uint8_t*)str);
  str[size] = '\0';
  return str;
}

//------------------------------------------------------------------------------------------------- Clear

void BUFF_Clear(BUFF_t *buff)
{
  while(BUFF_Skip(buff));
  buff->_msg_counter = 0;
  buff->_head = buff->_tail;
  buff->_echo = buff->_tail;
  buff->_esc = 0;
  buff->_break_allow = false;
}

//-------------------------------------------------------------------------------------------------
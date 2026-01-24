#include "buff.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initializes the circular buffer.
 * @param buff Pointer to the buffer structure.
 */
inline void BUFF_Init(BUFF_t *buff)
{
  buff->head = buff->memory;
  buff->tail = buff->memory;
  buff->end_memory = buff->memory + buff->size;
  buff->echo = buff->memory;
  buff->msg_head = 0;
  buff->msg_tail = 0;
  buff->break_allow = false;
}

/**
 * @brief Marks the end of the current message if break is allowed.
 * @param buff Pointer to the buffer structure.
 * @return `true` if the break was performed, false otherwise.
 */
bool BUFF_Break(BUFF_t *buff)
{
  if(!buff->break_allow) return false;
  buff->msg_size[buff->msg_head] = buff->msg_counter;
  buff->msg_counter = 0;
  buff->msg_head++;
  if(buff->msg_head >= BUFF_MESSAGE_LIMIT) buff->msg_head = 0;
  if(buff->msg_head == buff->msg_tail) {
    if(buff->Overflow) buff->Overflow();
  }
  buff->break_allow = false;
  return true;
}

/**
 * @brief Returns the size of the current message in the buffer.
 * @param buff Pointer to the buffer structure.
 * @return Size of the current message.
 */
uint16_t BUFF_Size(BUFF_t *buff)
{
  if(buff->msg_head != buff->msg_tail) return buff->msg_size[buff->msg_tail];
  return 0;
}

/**
 * @brief Appends a byte `value` to the circular buffer.
 * @param buff Pointer to the buffer structure.
 * @param value Value to append.
 * @return `true` if the value was appended, `false` if the buffer is full.
 */
bool BUFF_Append(BUFF_t *buff, uint8_t value)
{
  *buff->head = value;
  buff->msg_counter++;
  buff->head++;
  if(buff->head >= buff->end_memory) buff->head = buff->memory;
  if(!buff->console_mode) buff->echo = buff->head;
  buff->break_allow = true;
  if(buff->head == buff->tail) {
    if(buff->Overflow) buff->Overflow();
    return false;
  }
  return true;
}

/**
 * @brief Reads the next byte from the buffer without removing it.
 * @param buff Pointer to the buffer structure.
 * @param value Pointer to variable to store the peeked byte.
 * @return true if a byte was read, false if at the end.
 */
bool BUFF_Echo(BUFF_t *buff, char *value)
{
  if(buff->echo == buff->head) return false;
  if(value) *value = *buff->echo;
  buff->echo++;
  if(buff->echo >= buff->end_memory) buff->echo = buff->memory;
  return true;
}

/**
 * @brief Removes the last byte from the current message in the buffer.
 * @param buff Pointer to the buffer structure.
 * @param value Pointer to variable to store the removed byte.
 * @return `true` if a byte was removed, `false` if the message is empty.
 */
bool BUFF_Pop(BUFF_t *buff, uint8_t *value)
{
  if(buff->msg_counter) {
    buff->msg_counter--;
    if(!buff->msg_counter) buff->break_allow = false;
    bool move_echo = buff->head == buff->echo;
    if(buff->head == buff->memory) buff->head = buff->end_memory;
    buff->head--;
    if(move_echo) buff->echo = buff->head;
    if(value) *value = *buff->head;
    return true;
  }
  return false;
}

/**
 * @brief Pushes a byte to the circular buffer (console mode aware).
 *   Handles escape sequences, Enter, Ctrl+C, and line breaks for console input.
 * @param buff Pointer to the buffer structure.
 * @param value Value to push.
 * @return `true` if the value was appended or message ended, `false` if ignored or buffer is full.
 */
bool BUFF_Push(BUFF_t *buff, uint8_t value)
{
  if(buff->console_mode) {
    if(buff->esc == 1) {
      if(value == '[' || value == 'O') buff->esc = 2;
      else buff->esc = 0;
      return false;
    }
    if(buff->esc == 2) {
      if(value >= 0x40 && value <= 0x7E) buff->esc = 0;
      return false;
    }
    if(value == 0x1B) { 
      buff->esc = 1;
      return false;
    }
    if(value == '\r' || value == '\n') {
      if(!buff->break_allow) return false;
      BUFF_Append(buff, '\n');
      BUFF_Break(buff);
      return true;
    }
    if(value == '\f' || value == 0x03) {
      BUFF_Append(buff, '\f');
      BUFF_Break(buff);
      BUFF_Skip(buff);
      return true;
    }
    if(value == 0x13) {
      return false;
    }
  }
  return BUFF_Append(buff, value);
}

/**
 * @brief Read current message and copy to `dst`.
 * Advances buffer to next message.
 * @param buff Pointer to buffer control.
 * @param dst Destination buffer or NULL.
 * @return Size of copied message.
 */
uint16_t BUFF_Read(BUFF_t *buff, uint8_t *dst)
{
  uint16_t size = BUFF_Size(buff);
  if(!size) return 0;
  uint16_t n = size;
  while(n) {
    if(dst) *dst++ = *buff->tail;
    buff->tail++;
    if(buff->tail >= buff->end_memory) buff->tail = buff->memory;
    n--;
  }
  buff->msg_tail++;
  if(buff->msg_tail >= BUFF_MESSAGE_LIMIT) buff->msg_tail = 0;
  buff->msg_size[buff->msg_tail] = 0;
  return size;
}

/**
 * @brief Peek current message without advancing buffer.
 * @param buff Pointer to buffer control.
 * @param dst Destination buffer or NULL.
 * @return Size of message.
 */
uint16_t BUFF_Peek(BUFF_t *buff, uint8_t *dst)
{
  uint16_t size = BUFF_Size(buff);
  if(!size) return 0;
  uint16_t n = size;
  volatile uint8_t *ptr = buff->tail;
  while(n) {
    if(dst) *dst++ = *ptr;
    ptr++;
    if(ptr >= buff->end_memory) ptr = buff->memory;
    n--;
  }
  return size;
}

/**
 * @brief Skip current message in buffer using BUFF_Read.
 * @param buff Pointer to buffer control structure.
 * @return `true` if message skipped, `false` if buffer empty.
 */
bool BUFF_Skip(BUFF_t *buff)
{
  return BUFF_Read(buff, NULL) ? true : false;
}

/**
 * @brief Clear buffer by skipping all messages.
 * @param buff Pointer to buffer control.
 */
void BUFF_Clear(BUFF_t *buff)
{
  while(BUFF_Skip(buff));
}

/**
 * @brief Read current message from buffer and return as allocated string.
 * Memory is allocated dynamically and must be freed by caller.
 * @param buff Pointer to buffer control structure.
 * @return Pointer to allocated null-terminated string, or NULL if buffer empty or alloc failed.
 */
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


//-------------------------------------------------------------------------------------------------

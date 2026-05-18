// lib/col/buff.h

#ifndef BUFF_H_
#define BUFF_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "heap.h"

//------------------------------------------------------------------------------------------------- Config

#ifndef BUFF_MSG_LIMIT
  // Max pending messages in queue
  #define BUFF_MSG_LIMIT 32
#endif

//------------------------------------------------------------------------------------------------- Structure

/**
 * @brief Message-oriented circular buffer.
 * @param[in] memory Pointer to buffer memory
 * @param[in] size Buffer size in bytes
 * @param[in] console_mode Enable console input parsing (ESC, Enter, Ctrl+C)
 * @param[in] Overflow Optional overflow handler (`NULL` = disabled)
 * Internal:
 * @param _esc ESC parser state
 * @param _end_memory `memory + size`
 * @param _head Write pointer
 * @param _tail Read pointer
 * @param _echo Echo pointer
 * @param _msg_counter Bytes in current message
 * @param _msg_size Size of each pending message
 * @param _msg_head Message write index
 * @param _msg_tail Message read index
 * @param _break_allow Allow message break flag
 */
typedef struct {
  uint8_t *memory;
  uint16_t size;
  bool console_mode;
  void (*Overflow)(void);
  // internal
  uint8_t _esc;
  uint8_t *_end_memory;
  volatile uint8_t *_tail;
  volatile uint8_t *_head;
  volatile uint8_t *_echo;
  volatile uint16_t _msg_counter;
  uint16_t _msg_size[BUFF_MSG_LIMIT];
  volatile uint16_t _msg_head;
  volatile uint16_t _msg_tail;
  bool _break_allow;
} BUFF_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize circular buffer state.
 * @param[in,out] buff Pointer to buffer structure
 */
static inline void BUFF_Init(BUFF_t *buff)
{
  buff->_head = buff->memory;
  buff->_tail = buff->memory;
  buff->_end_memory = buff->memory + buff->size;
  buff->_echo = buff->memory;
  buff->_msg_counter = 0;
  buff->_msg_head = 0;
  buff->_msg_tail = 0;
  buff->_esc = 0;
  buff->_break_allow = false;
}

/**
 * @brief Mark end of current message. Drop-newest on queue overflow.
 * @param[in,out] buff Pointer to buffer structure
 * @return `true` if break performed, `false` if ignored or queue full
 */
bool BUFF_Break(BUFF_t *buff);

/**
 * @brief Get size of current pending message.
 * @param[in] buff Pointer to buffer structure
 * @return Size in bytes, `0` if no pending message
 */
uint16_t BUFF_Size(BUFF_t *buff);

/**
 * @brief Get number of pending messages in the queue.
 * @param[in] buff Pointer to buffer structure
 * @return Number of messages waiting to be read
 */
uint16_t BUFF_MessageCount(BUFF_t *buff);

/**
 * @brief Append single byte. Drop-newest on buffer overflow.
 * @param[in,out] buff Pointer to buffer structure
 * @param[in] value Byte to append
 * @return `true` if appended, `false` if buffer full
 */
bool BUFF_Append(BUFF_t *buff, uint8_t value);

/**
 * @brief Read next unread byte via echo pointer, without removing.
 * @param[in,out] buff Pointer to buffer structure
 * @param[out] value Byte destination or `NULL`
 * @return `true` if byte read, `false` if nothing to echo
 */
bool BUFF_Echo(BUFF_t *buff, char *value);

/**
 * @brief Remove last byte from current in-progress message.
 * @param[in,out] buff Pointer to buffer structure
 * @param[out] value Removed byte destination or `NULL`
 * @return `true` if byte removed, `false` if message empty
 */
bool BUFF_Pop(BUFF_t *buff, uint8_t *value);

/**
 * @brief Push byte with optional console-mode filtering.
 *   Handles ESC sequences, Enter, Ctrl+C, XOFF.
 * @param[in,out] buff Pointer to buffer structure
 * @param[in] value Byte to push
 * @return `true` if appended or message ended, `false` if ignored or full
 */
bool BUFF_Push(BUFF_t *buff, uint8_t value);

/**
 * @brief Read current message to `dst` and advance queue.
 * @param[in,out] buff Pointer to buffer structure
 * @param[out] dst Destination buffer or `NULL` to discard
 * @return Bytes copied
 */
uint16_t BUFF_Read(BUFF_t *buff, uint8_t *dst);

/**
 * @brief Peek current message without advancing queue.
 * @param[in] buff Pointer to buffer structure
 * @param[out] dst Destination buffer or `NULL`
 * @return Message size
 */
uint16_t BUFF_Peek(BUFF_t *buff, uint8_t *dst);

/**
 * @brief Skip current message.
 * @param[in,out] buff Pointer to buffer structure
 * @return `true` if message skipped, `false` if buffer empty
 */
bool BUFF_Skip(BUFF_t *buff);

/**
 * @brief Clear buffer fully: pending messages, in-progress data, ESC parser.
 * @param[in,out] buff Pointer to buffer structure
 */
void BUFF_Clear(BUFF_t *buff);

/**
 * @brief Read current message as heap-allocated null-terminated string.
 *   Caller must free the returned pointer.
 * @param[in,out] buff Pointer to buffer structure
 * @return Heap pointer or `NULL` if empty or alloc failed
 */
char *BUFF_ReadString(BUFF_t *buff);

//-------------------------------------------------------------------------------------------------
#endif
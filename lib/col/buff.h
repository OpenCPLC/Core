#ifndef BUFF_H_
#define BUFF_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "heap.h"

// Max number of messages
#ifndef BUFF_MESSAGE_LIMIT
  #define BUFF_MESSAGE_LIMIT 32
#endif

//-------------------------------------------------------------------------------------------------

/**
 * @brief Buffer configuration and runtime state.
 * @param memory Pointer to buffer memory (user)
 * @param size Buffer size in bytes (user)
 * @param console_mode Enable console input mode (user)
 * @param Overflow Optional overflow handler (user)
 * @param esc ESC parser state (internal)
 * @param end_memory memory + size (internal)
 * @param head Write pointer (internal)
 * @param tail Read pointer (internal)
 * @param echo Echo pointer (internal)
 * @param msg_counter Bytes in current message (internal)
 * @param msg_size Size of each message (internal)
 * @param msg_head Message head index (internal)
 * @param msg_tail Message tail index (internal)
 * @param break_allow Allow message break (internal)
 */
typedef struct {
  uint8_t *memory;
  uint16_t size;
  bool console_mode;
  uint8_t esc;
  void (*Overflow)(void);
  uint8_t *end_memory;
  volatile uint8_t *tail;
  volatile uint8_t *head;
  volatile uint8_t *echo;
  volatile uint16_t msg_counter;
  uint16_t msg_size[BUFF_MESSAGE_LIMIT];
  volatile uint16_t msg_head;
  volatile uint16_t msg_tail;
  bool break_allow;
} BUFF_t;

void BUFF_Init(BUFF_t *buff);
bool BUFF_Break(BUFF_t *buff);
uint16_t BUFF_Size(BUFF_t *buff);
bool BUFF_Append(BUFF_t *buff, uint8_t value);
bool BUFF_Echo(BUFF_t *buff, char *value);
bool BUFF_Pop(BUFF_t *buff, uint8_t *value);
bool BUFF_Push(BUFF_t *buff, uint8_t value);
uint16_t BUFF_Read(BUFF_t *buff, uint8_t *dst);
uint16_t BUFF_Peek(BUFF_t *buff, uint8_t *dst);
bool BUFF_Skip(BUFF_t *buff);
void BUFF_Clear(BUFF_t *buff);
char *BUFF_ReadString(BUFF_t *buff);

//-------------------------------------------------------------------------------------------------
#endif

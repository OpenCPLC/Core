// .tests/test_buff.c
// Message buffer: past `BUFF_MSG_LIMIT` the newest message goes, never an older one,
// and every message kept reads back whole, also one that runs over the end of the ring.
// In console mode Enter closes a line once, escape sequences and XOFF leave no byte,
// and Ctrl+C drops the line being typed, never one entered before it,
// leaving `\f` in front of the next.

#include <stdio.h>
#include <string.h>

#include "test.h"
#include "buff.c"

static void overflow_drops_the_newest(void)
{
  uint8_t memory[512];
  BUFF_t buff = { .memory = memory, .size = sizeof(memory) };
  BUFF_Init(&buff);
  uint16_t accepted = 0;
  for(uint16_t i = 0; i < BUFF_MSG_LIMIT + 4; i++) {
    char message[8];
    int len = snprintf(message, sizeof(message), "M%u;", i);
    TEST_Check(len > 0 && len < (int)sizeof(message));
    for(int j = 0; j < len; j++) TEST_Check(BUFF_Push(&buff, (uint8_t)message[j]));
    if(BUFF_Break(&buff)) accepted++;
  }
  TEST_Equal(accepted, BUFF_MSG_LIMIT - 1, "one slot keeps the message being written");
  TEST_Equal(BUFF_MessageCount(&buff), accepted, "every accepted message counted");
  for(uint16_t i = 0; i < accepted; i++) {
    char expected[8], got[8] = { 0 };
    snprintf(expected, sizeof(expected), "M%u;", i);
    TEST_Check(BUFF_Read(&buff, (uint8_t *)got) == strlen(expected), "message %u", i);
    TEST_Check(!strcmp(got, expected), "message %u reads %s", i, got);
  }
  TEST_Check(!BUFF_Size(&buff), "nothing past the accepted messages");
  for(const char *c = "X;"; *c; c++) TEST_Check(BUFF_Push(&buff, (uint8_t)*c));
  TEST_Check(BUFF_Break(&buff), "a message once there is room again");
  char got[8] = { 0 };
  TEST_Check(BUFF_Read(&buff, (uint8_t *)got) == 2 && !strcmp(got, "X;"), "reads %s", got);
}

// Messages of five bytes through a ring of sixteen, two waiting at a time,
// so every few of them run over its end.
static void message_over_the_ring_end_reads_whole(void)
{
  uint8_t memory[16];
  BUFF_t buff = { .memory = memory, .size = sizeof(memory) };
  BUFF_Init(&buff);
  for(uint8_t i = 0; i < 24; i++) {
    for(uint8_t j = 0; j < 5; j++) TEST_Check(BUFF_Push(&buff, (uint8_t)(5 * i + j)));
    TEST_Check(BUFF_Break(&buff));
    if(!i) continue;
    uint8_t expected[5], peeked[5], read[5];
    for(uint8_t j = 0; j < 5; j++) expected[j] = (uint8_t)(5 * (i - 1) + j);
    TEST_Check(BUFF_Peek(&buff, peeked) == 5, "message %u peeked", i - 1);
    TEST_Bytes(peeked, expected, 5, "message peeked");
    TEST_Check(BUFF_Read(&buff, read) == 5, "message %u read", i - 1);
    TEST_Bytes(read, expected, 5, "message read");
  }
}

//----------------------------------------------------------------------------------------- Console

static void type(BUFF_t *buff, const char *keys)
{
  while(*keys) BUFF_Push(buff, (uint8_t)*keys++);
}

// Next line as text, empty when none waits
static const char *next_line(BUFF_t *buff)
{
  static char line[64];
  uint16_t size = BUFF_Read(buff, (uint8_t *)line);
  line[size] = 0;
  return line;
}

static void console_keys(void)
{
  uint8_t memory[64];
  BUFF_t buff = { .memory = memory, .size = sizeof(memory), .console_mode = true };
  BUFF_Init(&buff);
  type(&buff, "\r");
  TEST_Equal(BUFF_MessageCount(&buff), 0, "Enter on an empty line");
  type(&buff, "ab\x1B[Ac\x1B[1;5Cd\x1BOPe\x13\r\n");
  TEST_Equal(BUFF_MessageCount(&buff), 1, "CR LF closes one line");
  TEST_Equal(next_line(&buff), "abcde\n", "arrow keys, F1 and XOFF left out");
  type(&buff, "first\r");
  type(&buff, "typo\x03");
  TEST_Equal(BUFF_MessageCount(&buff), 1, "Ctrl+C drops one line");
  TEST_Equal(next_line(&buff), "first\n", "line entered before Ctrl+C kept");
  type(&buff, "\rnext\r");
  TEST_Equal(next_line(&buff), "\fnext\n", "Enter after Ctrl+C closes no line");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(overflow_drops_the_newest);
  TEST(message_over_the_ring_end_reads_whole);
  TEST(console_keys);
}

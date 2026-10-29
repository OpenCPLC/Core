// .tests/test_mstp.c

// MS/TP master node against frames of the standard and the nodes around it on the line:
// CRC as the algorithms of Annex G compute it, the frames of the plan byte for byte,
// several frames in one message, a repeated preamble and padding skipped,
// broken, cut, foreign, broadcast and extended frames left alone,
// the token taken, passed, retried and lost, a successor found by Poll For Master,
// the gap up to it polled past `MSTP_POLL` tokens, the token generated in this node's slot only,
// a sole master, `online` only beside another master,
// answers to Poll For Master, Test_Request and data,
// Reply Postponed for a request the server leaves unanswered,
// and a request of this node holding the token until its answer, Reply Postponed or silence.

#include <string.h>

#include "test.h"
#include "plc/com/mstp.c"

static uint8_t rx_memory[1024];
static BUFF_t rx = { .memory = rx_memory, .size = sizeof(rx_memory) };
static UART_t port = { .UART_38400, .buff = &rx };
static MSTP_t node;

// Data frame the last pass handed over
static const MSTP_Frame_t *handed;

//------------------------------------------------------------------------------------------ Frames

// Header CRC by the algorithm of Annex G, apart from the CRC unit behind `crc_header`
static uint8_t annex_header(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0xFF;
  for(uint16_t i = 0; i < len; i++) {
    uint16_t c = crc ^ data[i];
    c ^= (c << 1) ^ (c << 2) ^ (c << 3) ^ (c << 4) ^ (c << 5) ^ (c << 6) ^ (c << 7);
    crc = (uint8_t)((c & 0xFE) ^ ((c >> 8) & 1));
  }
  return (uint8_t)~crc;
}

// Data CRC by the algorithm of Annex G
static uint16_t annex_data(const uint8_t *data, uint16_t len)
{
  uint16_t crc = 0xFFFF;
  for(uint16_t i = 0; i < len; i++) {
    uint16_t low = (crc & 0xFF) ^ data[i];
    crc = (crc >> 8) ^ (low << 8) ^ (low << 3) ^ (low << 12) ^ (low >> 4) ^
      (low & 0x0F) ^ ((low & 0x0F) << 7);
  }
  return (uint16_t)~crc;
}

// Frame laid out as the standard has it, its length returned
static uint16_t frame(uint8_t *out, uint8_t type, uint8_t destination, uint8_t source,
  const void *data, uint16_t len)
{
  out[0] = 0x55;
  out[1] = 0xFF;
  out[2] = type;
  out[3] = destination;
  out[4] = source;
  out[5] = (uint8_t)(len >> 8);
  out[6] = (uint8_t)len;
  out[7] = annex_header(&out[2], 5);
  if(!len) return 8;
  memcpy(&out[8], data, len);
  uint16_t crc = annex_data(&out[8], len);
  out[8 + len] = (uint8_t)crc;
  out[9 + len] = (uint8_t)(crc >> 8);
  return len + 10;
}

// Bytes from the line, one message as the receiver timeout closes it
static void hear(const uint8_t *bytes, uint16_t len)
{
  for(uint16_t i = 0; i < len; i++) BUFF_Push(&rx, bytes[i]);
  BUFF_Break(&rx);
}

// Frame of another node
static void hear_frame(uint8_t type, uint8_t destination, uint8_t source,
  const void *data, uint16_t len)
{
  uint8_t bytes[520];
  hear(bytes, frame(bytes, type, destination, source, data, len));
}

// The node put exactly this frame of its own on the line since the last look
static bool sent(uint8_t type, uint8_t destination, const void *data, uint16_t len)
{
  uint8_t expected[520], line[256];
  uint16_t size = frame(expected, type, destination, node.address, data, len);
  return UART_HostSent(&port, line) == size && !memcmp(line, expected, size);
}

// The node sent nothing since the last look
static bool quiet(void) { return !UART_HostSent(&port, NULL); }

//------------------------------------------------------------------------------------------ Passes

// One pass of the node's thread, one millisecond later
static void pass(void)
{
  handed = MSTP_Loop(&node);
  TEST_Run(1);
}

static void run(uint32_t ms)
{
  while(ms--) pass();
}

// Fresh node at `address` on a quiet line, after its first pass
static void start(uint8_t address, uint32_t baud)
{
  heap_free(node._rx);
  heap_free(node._tx);
  heap_free(node._pending);
  memset(&node, 0, sizeof(node));
  node.uart = &port;
  node.address = address;
  port.baud = baud;
  UART_Init(&port);
  UART_HostSent(&port, NULL);
  pass();
}

//---------------------------------------------------------------------------------- Frames and CRC

static void crc_by_annex_g(void)
{
  static const uint8_t header[] = { 0x00, 0x10, 0x05, 0x00, 0x00 };
  TEST_Equal(CRC_Run(&crc_header, header, 5), 0x8Cu, "token from 5 to 16");
  TEST_Equal(annex_header(header, 5), 0x8C, "Annex G agrees");
  uint8_t data[5] = { 0x01, 0x22, 0x30 };
  CRC_Append(&crc_data, data, 3);
  static const uint8_t crc[] = { 0x10, 0xBD };
  TEST_Bytes(&data[3], crc, 2, "data CRC, low byte first");
  TEST_Equal(CRC_Run(&crc_header, "123456789", 9), 0x89u, "header check value");
  TEST_Equal(CRC_Run(&crc_data, "123456789", 9), 0x6E90u, "data check value, low byte first");
}

// ReadProperty of the plan: the request handed over, the reply on the line byte for byte
static void example_read_both_ways(void)
{
  start(5, 38400);
  static const uint8_t request[] = {
    0x55, 0xFF, 0x05, 0x05, 0x01, 0x00, 0x0D, 0xAB,
    0x01, 0x04, 0x00, 0x03, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55,
    0x21, 0x61
  };
  hear(request, sizeof(request));
  pass();
  TEST_Check(handed && handed->expecting_reply, "request expecting a reply");
  TEST_Check(handed->len == 13, "13 bytes of NPDU");
  TEST_Bytes(handed->data, &request[8], 13, "NPDU");
  static const uint8_t npdu[] = {
    0x01, 0x00, 0x30, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55,
    0x3E, 0x44, 0x41, 0xAC, 0x00, 0x00, 0x3F
  };
  TEST_Check(!MSTP_Reply(&node, npdu, sizeof(npdu)), "reply");
  static const uint8_t reply[] = {
    0x55, 0xFF, 0x06, 0x01, 0x05, 0x00, 0x13, 0x6C,
    0x01, 0x00, 0x30, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55,
    0x3E, 0x44, 0x41, 0xAC, 0x00, 0x00, 0x3F,
    0x38, 0x24
  };
  uint8_t line[64];
  TEST_Equal(UART_HostSent(&port, line), sizeof(reply), "reply length");
  TEST_Bytes(line, reply, sizeof(reply), "reply of the plan");
  run(2);
  TEST_Check(quiet(), "no Reply Postponed after a reply");
}

// A broadcast and a token in one message, behind a repeated preamble, padding and a stray `55`
static void frames_share_a_message(void)
{
  start(5, 38400);
  static const uint8_t who_is[] = { 0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF, 0x10, 0x08 };
  uint8_t bytes[64];
  uint16_t len = 0;
  bytes[len++] = 0x55;
  len += frame(&bytes[len], MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, 3,
    who_is, sizeof(who_is));
  bytes[len++] = 0xFF;
  bytes[len++] = 0x55;
  bytes[len++] = 0x11;
  len += frame(&bytes[len], MSTP_Type_Token, 5, 3, NULL, 0);
  hear(bytes, len);
  pass();
  TEST_Check(handed && !handed->expecting_reply, "broadcast first");
  TEST_Bytes(handed->data, who_is, sizeof(who_is), "Who-Is");
  pass();
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "token taken, successor looked for");
}

static void receiver_timeout_per_baud(void)
{
  start(5, 9600);
  TEST_Equal(port.timeout, 60, "60 bit times at 9600");
  start(5, 115200);
  TEST_Equal(port.timeout, 230, "2ms at 115200");
}

// Cut, broken, foreign, extended and broadcast frames no node may answer
static void bad_frames_left_alone(void)
{
  start(5, 38400);
  static const uint8_t npdu[] = { 0x01, 0x00, 0x10, 0x08 };
  uint8_t bytes[64];
  uint16_t len = frame(bytes, MSTP_Type_DataNotExpectingReply, 5, 3, npdu, sizeof(npdu));
  hear(bytes, len - 3);
  bytes[7] ^= 0x01;
  hear(bytes, len);
  bytes[7] ^= 0x01;
  bytes[len - 1] ^= 0x01;
  hear(bytes, len);
  len = frame(bytes, 32, 5, 3, npdu, sizeof(npdu));
  bytes[len - 1] ^= 0x01;
  hear(bytes, len);
  hear_frame(MSTP_Type_DataNotExpectingReply, 7, 3, npdu, sizeof(npdu));
  hear_frame(MSTP_Type_Token, MSTP_BROADCAST, 3, NULL, 0);
  hear_frame(MSTP_Type_PollForMaster, MSTP_BROADCAST, 3, NULL, 0);
  hear_frame(MSTP_Type_TestRequest, MSTP_BROADCAST, 3, NULL, 0);
  hear_frame(MSTP_Type_DataExpectingReply, MSTP_BROADCAST, 3, npdu, sizeof(npdu));
  for(uint8_t i = 0; i < 12; i++) {
    pass();
    TEST_Check(!handed, "nothing handed over, pass %u", i);
  }
  TEST_Check(quiet(), "nothing sent");
  TEST_Equal(node.crc_errors, 2, "header and data CRC, the extended frame unchecked");
}

//------------------------------------------------------------------------------------------- Token

static void token_passed_retried_and_lost(void)
{
  start(5, 38400);
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "successor looked for from 6");
  hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 6, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_Token, 6, NULL, 0), "token to 6, which replied");
  hear((const uint8_t *)"\x55\xFF\x00", 3);
  run(29);
  TEST_Check(quiet(), "three bytes do not use the token, no retry before 30ms");
  run(3);
  TEST_Check(sent(MSTP_Type_Token, 6, NULL, 0), "token again after 30ms of silence");
  TEST_Equal(node.retries, 1, "one retry");
  run(32);
  TEST_Check(sent(MSTP_Type_PollForMaster, 7, NULL, 0), "6 gone, the next node looked for");
}

static void successor_uses_the_token(void)
{
  start(5, 38400);
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "6 polled");
  hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 6, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_Token, 6, NULL, 0), "token to 6");
  hear_frame(MSTP_Type_Token, 7, 6, NULL, 0);
  run(40);
  TEST_Check(quiet() && !node.retries, "6 passed it on, no retry");
}

// Ring 4 → 5 → 8: past 50 tokens the gap 6, 7 is polled, one address per token
static void gap_polled_after_50_tokens(void)
{
  start(5, 38400);
  hear_frame(MSTP_Type_Token, 5, 4, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "6 polled");
  run(32);
  TEST_Check(sent(MSTP_Type_PollForMaster, 7, NULL, 0), "7 polled");
  run(32);
  TEST_Check(sent(MSTP_Type_PollForMaster, 8, NULL, 0), "8 polled");
  hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 8, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_Token, 8, NULL, 0), "8 the successor");
  hear_frame(MSTP_Type_Token, 4, 8, NULL, 0);
  pass();
  for(uint8_t i = 0; i < 49; i++) {
    hear_frame(MSTP_Type_Token, 5, 4, NULL, 0);
    pass();
    TEST_Check(sent(MSTP_Type_Token, 8, NULL, 0), "token %u straight to 8", i);
    hear_frame(MSTP_Type_Token, 4, 8, NULL, 0);
    pass();
  }
  for(uint8_t station = 6; station < 8; station++) {
    hear_frame(MSTP_Type_Token, 5, 4, NULL, 0);
    pass();
    TEST_Check(sent(MSTP_Type_PollForMaster, station, NULL, 0), "%u of the gap polled", station);
    run(32);
    TEST_Check(sent(MSTP_Type_Token, 8, NULL, 0), "then the token to 8");
    hear_frame(MSTP_Type_Token, 4, 8, NULL, 0);
    pass();
  }
  hear_frame(MSTP_Type_Token, 5, 4, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_Token, 8, NULL, 0), "gap polled through, token straight to 8");
}

static void token_generated_in_own_slot(void)
{
  start(5, 38400);
  run(548);
  TEST_Check(quiet(), "no token before 500ms and five slots");
  run(12);
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "token generated in the slot of MAC 5");
}

// A thread held up past its slot waits until every slot is gone
static void missed_slot_waits_for_all(void)
{
  start(5, 38400);
  TEST_Run(600);
  run(1179);
  TEST_Check(quiet(), "slot missed, no token within the slots of others");
  run(3);
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "every slot gone, token generated");
}

// Line holds `bytes` among what the node sent since the last look
static bool line_holds(const uint8_t *bytes, uint16_t len)
{
  uint8_t line[256];
  uint16_t size = UART_HostSent(&port, line);
  for(uint16_t i = 0; i + len <= size; i++) {
    if(!memcmp(&line[i], bytes, len)) return true;
  }
  return false;
}

static void sole_master_keeps_the_token(void)
{
  start(5, 38400);
  run(560 + 127 * 32);
  TEST_Check(node._sole && !node.online, "nobody answered: sole master, alone on the line");
  static const uint8_t npdu[] = { 0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF, 0x10, 0x00 };
  TEST_Check(!MSTP_Send(&node, MSTP_BROADCAST, npdu, sizeof(npdu), false), "frame left");
  uint8_t expected[32];
  uint16_t len = frame(expected, MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, 5,
    npdu, sizeof(npdu));
  UART_HostSent(&port, NULL);
  bool out = false;
  for(uint8_t i = 0; i < 40 && !out; i++) {
    pass();
    out = line_holds(expected, len);
  }
  TEST_Check(out, "frame sent by the sole master");
  // A token while a Poll For Master waits is dropped with the one held,
  // the retry of its sender finds the node idle and ends the sole master.
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  run(5);
  TEST_Check(node._sole && node._state == MSTP_State_Idle, "token dropped, node idle");
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  TEST_Check(!node._sole && node.online, "the retried token ends the sole master, online");
}

//----------------------------------------------------------------------------------------- Answers

static void answers_poll_test_and_data(void)
{
  start(5, 38400);
  hear_frame(MSTP_Type_PollForMaster, 5, 3, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_ReplyToPollForMaster, 3, NULL, 0), "Reply To Poll For Master");
  hear_frame(MSTP_Type_TestRequest, 5, 3, "ABC", 3);
  pass();
  TEST_Check(sent(MSTP_Type_TestResponse, 3, "ABC", 3), "Test_Response with the same data");
  static const uint8_t npdu[] = { 0x01, 0x04, 0x00, 0x03, 0x07, 0x0C };
  hear_frame(MSTP_Type_DataExpectingReply, 5, 3, npdu, sizeof(npdu));
  pass();
  TEST_Check(handed && handed->expecting_reply, "request handed over");
  pass();
  TEST_Check(sent(MSTP_Type_ReplyPostponed, 3, NULL, 0), "left unanswered: Reply Postponed");
  TEST_Check(MSTP_Reply(&node, npdu, sizeof(npdu)) == ERR, "nothing left to answer");
}

static void online_until_the_token_is_lost(void)
{
  start(5, 38400);
  TEST_Check(!node.online, "offline at start");
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  TEST_Check(node.online, "online with the token");
  hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 6, NULL, 0);
  pass();
  hear_frame(MSTP_Type_Token, 3, 6, NULL, 0);
  run(400);
  TEST_Check(node.online, "online while the line was in use");
  run(200);
  TEST_Check(!node.online, "500ms of silence lose the token");
}

// The master that passed the token is gone: the whole ring polled, the node is left alone
static void offline_when_left_alone(void)
{
  start(5, 38400);
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  TEST_Check(node.online, "token from 3");
  run(127 * 32);
  TEST_Check(node._sole && !node.online, "nobody answered a poll: alone, not online");
}

static void send_waits_for_the_token(void)
{
  start(5, 38400);
  static const uint8_t npdu[] = { 0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF, 0x10, 0x00 };
  TEST_Check(!MSTP_Send(&node, MSTP_BROADCAST, npdu, sizeof(npdu), false), "queued");
  TEST_Check(MSTP_Send(&node, MSTP_BROADCAST, npdu, sizeof(npdu), false) == BUSY, "one at a time");
  run(5);
  TEST_Check(quiet(), "nothing without the token");
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_DataNotExpectingReply, MSTP_BROADCAST, npdu, sizeof(npdu)),
    "sent with the token");
  pass();
  TEST_Check(sent(MSTP_Type_PollForMaster, 6, NULL, 0), "then the token goes on");
  TEST_Check(MSTP_Send(&node, 3, npdu, 0, false) == ERR, "nothing to send");
}

//---------------------------------------------------------------------------------------- Requests

// ReadProperty of the plan, sent by this node to MAC 3
static const uint8_t request[] = {
  0x01, 0x04, 0x00, 0x03, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55
};

// Node 5 with successor 6, its request to MAC 3 sent with the next token
static void request_sent(void)
{
  start(5, 38400);
  hear_frame(MSTP_Type_Token, 5, 3, NULL, 0);
  pass();
  hear_frame(MSTP_Type_ReplyToPollForMaster, 5, 6, NULL, 0);
  pass();
  UART_HostSent(&port, NULL);
  TEST_Check(!MSTP_Send(&node, 3, request, sizeof(request), true), "request queued");
  hear_frame(MSTP_Type_Token, 5, 4, NULL, 0);
  pass();
  TEST_Check(sent(MSTP_Type_DataExpectingReply, 3, request, sizeof(request)),
    "Data Expecting Reply with the token");
}

static void request_holds_the_token_for_its_answer(void)
{
  request_sent();
  run(200);
  TEST_Check(quiet() && !handed, "token held while the answer is awaited");
  static const uint8_t ack[] = {
    0x01, 0x00, 0x30, 0x01, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x19, 0x55,
    0x3E, 0x44, 0x41, 0xAC, 0x00, 0x00, 0x3F
  };
  hear_frame(MSTP_Type_DataNotExpectingReply, 5, 3, ack, sizeof(ack));
  pass();
  TEST_Check(handed && !handed->expecting_reply && handed->source == 3, "answer from 3");
  TEST_Bytes(handed->data, ack, sizeof(ack), "handed over whole");
  pass();
  TEST_Check(sent(MSTP_Type_Token, 6, NULL, 0), "then the token goes on");
}

// Reply Postponed passes the token at once, the answer comes later with the token of 3
static void postponed_answer_comes_later(void)
{
  request_sent();
  hear_frame(MSTP_Type_ReplyPostponed, 5, 3, NULL, 0);
  pass();
  TEST_Check(!handed && sent(MSTP_Type_Token, 6, NULL, 0), "Reply Postponed: token to 6");
  static const uint8_t ack[] = { 0x01, 0x00, 0x20, 0x01, 0x0F };
  hear_frame(MSTP_Type_DataNotExpectingReply, 5, 3, ack, sizeof(ack));
  pass();
  TEST_Check(handed && handed->source == 3 && handed->len == sizeof(ack), "answer handed over");
}

static void wait_ends_without_an_answer(void)
{
  request_sent();
  run(299);
  TEST_Check(quiet(), "no answer yet within 300ms");
  run(2);
  TEST_Check(sent(MSTP_Type_Token, 6, NULL, 0), "300ms of silence: token to 6");
  request_sent();
  hear_frame(MSTP_Type_DataNotExpectingReply, 7, 3, request, sizeof(request));
  pass();
  TEST_Check(!handed && sent(MSTP_Type_Token, 6, NULL, 0), "another node talks: token to 6");
  request_sent();
  uint8_t bytes[32];
  uint16_t len = frame(bytes, MSTP_Type_DataNotExpectingReply, 5, 3, request, sizeof(request));
  bytes[len - 1] ^= 0x01;
  hear(bytes, len);
  pass();
  TEST_Check(!handed && sent(MSTP_Type_Token, 6, NULL, 0), "a broken frame: token to 6");
  request_sent();
  hear_frame(MSTP_Type_Token, 5, 4, NULL, 0);
  pass();
  TEST_Check(!handed && quiet() && node._state == MSTP_State_Idle,
    "a token while waiting: a second token on the line, this one dropped");
  request_sent();
  hear_frame(MSTP_Type_DataExpectingReply, 5, 3, request, sizeof(request));
  pass();
  TEST_Check(!handed && quiet() && node._state == MSTP_State_Idle,
    "a request while waiting: a second token on the line, this one dropped");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(crc_by_annex_g);
  TEST(example_read_both_ways);
  TEST(frames_share_a_message);
  TEST(receiver_timeout_per_baud);
  TEST(bad_frames_left_alone);
  TEST(token_passed_retried_and_lost);
  TEST(successor_uses_the_token);
  TEST(gap_polled_after_50_tokens);
  TEST(token_generated_in_own_slot);
  TEST(missed_slot_waits_for_all);
  TEST(sole_master_keeps_the_token);
  TEST(answers_poll_test_and_data);
  TEST(online_until_the_token_is_lost);
  TEST(offline_when_left_alone);
  TEST(send_waits_for_the_token);
  TEST(request_holds_the_token_for_its_answer);
  TEST(postponed_answer_comes_later);
  TEST(wait_ends_without_an_answer);
}

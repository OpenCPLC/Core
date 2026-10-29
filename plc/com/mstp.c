// plc/com/mstp.c

#include "mstp.h"

#include <string.h>

#include "crc.h"
#include "heap.h"
#include "vrts.h"

//------------------------------------------------------------------------------------------ Config

// Silence that loses the token, and the slot of one MAC when the token is generated anew
#define MSTP_NO_TOKEN_ms 500
#define MSTP_SLOT_ms 10

// Wait for the successor to use the token or for a polled node to reply.
// The standard allows 20–35ms, 30 leaves room for a master running on a PC.
#define MSTP_USAGE_TIMEOUT_ms 30

// Wait for the answer to a request of this node:
// a server may take 250ms to begin it, and bacnet-stack accepts at most 300.
#define MSTP_REPLY_TIMEOUT_ms 300

// Tokens between maintenance polls, token retries,
// and the bytes a node has to send before the line counts as in use.
#define MSTP_POLL 50
#define MSTP_RETRY_TOKEN 1
#define MSTP_MIN_OCTETS 4

#define MSTP_MAX_MASTER 127

// Receiver timeout: `T_frame_abort` of 60 bit times,
// but 2ms at least, a USB adapter can pause that long inside a frame.
#define MSTP_FRAME_ABORT_BITS 60
#define MSTP_TIMEOUT_MIN_ms 2

// Bytes ahead of the data: preamble `55 FF`, five header bytes and their CRC
#define MSTP_HEAD_LEN 8

//------------------------------------------------------------------------------------------- Types

typedef enum {
  MSTP_Type_Token = 0,
  MSTP_Type_PollForMaster = 1,
  MSTP_Type_ReplyToPollForMaster = 2,
  MSTP_Type_TestRequest = 3,
  MSTP_Type_TestResponse = 4,
  MSTP_Type_DataExpectingReply = 5,
  MSTP_Type_DataNotExpectingReply = 6,
  MSTP_Type_ReplyPostponed = 7,
} MSTP_Type_t;

// Frame read off the line, `data` points into the message
typedef struct {
  uint8_t type;
  uint8_t destination;
  uint8_t source;
  const uint8_t *data;
  uint16_t len;
} MSTP_RX_t;

//--------------------------------------------------------------------------------------------- CRC

// Header: CRC-8, polynomial x⁸ + x⁷ + 1, reflected, `0xFF` in and out
static const CRC_t crc_header = {
  .width = 8,
  .polynomial = 0x81,
  .initial = 0xFF,
  .reflect_data_in = 8,
  .reflect_data_out = true,
  .final_xor = 0xFF,
  .invert_out = false
};

// Data: CRC-16/IBM-SDLC, known as X-25, low byte first on the line
static const CRC_t crc_data = {
  .width = 16,
  .polynomial = 0x1021,
  .initial = 0xFFFF,
  .reflect_data_in = 16,
  .reflect_data_out = true,
  .final_xor = 0xFFFF,
  .invert_out = true
};

//-------------------------------------------------------------------------------------------- Line

static uint8_t next_station(uint8_t station) { return (station + 1) % (MSTP_MAX_MASTER + 1); }

static uint32_t silence_ms(const MSTP_t *mstp) { return (uint32_t)tick_diff(mstp->_silence_tick); }

// More than `MSTP_MIN_OCTETS` bytes since the state was entered, closed or still coming in
static bool line_used(MSTP_t *mstp) {
  return mstp->_event + UART_Size(mstp->uart) + UART_Receiving(mstp->uart) > MSTP_MIN_OCTETS;
}

// Frame onto the line, the next pass waits for its last bit
static status_t send(MSTP_t *mstp, MSTP_Type_t type, uint8_t destination,
  const uint8_t *data, uint16_t len)
{
  uint16_t size = MSTP_HEAD_LEN + (len ? len + 2 : 0);
  heap_free(mstp->_tx);
  mstp->_tx = heap_alloc(size);
  uint8_t *tx = mstp->_tx;
  tx[0] = 0x55;
  tx[1] = 0xFF;
  tx[2] = type;
  tx[3] = destination;
  tx[4] = mstp->address;
  tx[5] = (uint8_t)(len >> 8);
  tx[6] = (uint8_t)len;
  CRC_Append(&crc_header, &tx[2], 5);
  if(len) {
    memcpy(&tx[MSTP_HEAD_LEN], data, len);
    CRC_Append(&crc_data, &tx[MSTP_HEAD_LEN], len);
  }
  if(UART_Send(mstp->uart, tx, size)) return ERR;
  mstp->_sending = true;
  return OK;
}

// Next message off the line once the last one is parsed, its activity noted
static void receive(MSTP_t *mstp)
{
  if(UART_Receiving(mstp->uart)) mstp->_silence_tick = tick_now();
  uint16_t size = UART_Size(mstp->uart);
  if(!size || mstp->_rx_pos < mstp->_rx_len) return;
  mstp->_silence_tick = tick_now();
  heap_free(mstp->_rx);
  mstp->_rx = heap_alloc(size);
  mstp->_rx_pos = 0;
  mstp->_rx_len = UART_Read(mstp->uart, mstp->_rx);
  mstp->_event += mstp->_rx_len;
}

// Next valid frame of the message, `false` once it has none
static bool parse(MSTP_t *mstp, MSTP_RX_t *frame)
{
  const uint8_t *msg = mstp->_rx;
  while(mstp->_rx_len - mstp->_rx_pos >= MSTP_HEAD_LEN) {
    uint16_t pos = mstp->_rx_pos;
    // Any byte but the preamble is skipped, a repeated `0x55` included
    if(msg[pos] != 0x55 || msg[pos + 1] != 0xFF) {
      mstp->_rx_pos++;
      continue;
    }
    const uint8_t *header = &msg[pos + 2];
    if(CRC_Error(&crc_header, header, 6)) {
      mstp->crc_errors++;
      mstp->_rx_pos++;
      continue;
    }
    uint16_t len = ((uint16_t)header[3] << 8) | header[4];
    uint32_t size = MSTP_HEAD_LEN + (len ? (uint32_t)len + 2 : 0);
    // Cut short: the receiver timeout closed it, which stands for `T_frame_abort`
    if((uint32_t)(mstp->_rx_len - pos) < size) break;
    mstp->_rx_pos = pos + size;
    // Reserved, proprietary and extended frames, the last with CRC-32K, pass unchecked
    if(header[0] > MSTP_Type_ReplyPostponed || len > MSTP_DATA_MAX) continue;
    if(len && CRC_Error(&crc_data, &msg[pos + MSTP_HEAD_LEN], len + 2)) {
      mstp->crc_errors++;
      continue;
    }
    frame->type = header[0];
    frame->destination = header[1];
    frame->source = header[2];
    frame->data = &msg[pos + MSTP_HEAD_LEN];
    frame->len = len;
    return true;
  }
  mstp->_rx_pos = mstp->_rx_len;
  return false;
}

//------------------------------------------------------------------------------------------- Token

// Token to the successor, then wait for it to be used
static void pass_token(MSTP_t *mstp)
{
  mstp->_retry = 0;
  mstp->_event = 0;
  mstp->_state = MSTP_State_PassToken;
  send(mstp, MSTP_Type_Token, mstp->_ns, NULL, 0);
}

// Poll For Master to `station`, then wait for its reply
static void poll(MSTP_t *mstp, uint8_t station)
{
  mstp->_ps = station;
  mstp->_retry = 0;
  mstp->_state = MSTP_State_PollForMaster;
  send(mstp, MSTP_Type_PollForMaster, station, NULL, 0);
}

// Data frame for the server, with the reply it owes when the sender waits for one
static const MSTP_Frame_t *hand_over(MSTP_t *mstp, const MSTP_RX_t *frame, bool expecting_reply)
{
  mstp->_frame.data = frame->data;
  mstp->_frame.len = frame->len;
  mstp->_frame.source = frame->source;
  mstp->_frame.expecting_reply = expecting_reply;
  mstp->_answer = expecting_reply;
  mstp->_reply_to = frame->source;
  return &mstp->_frame;
}

//------------------------------------------------------------------------------------------ States

// Frames for this node, broadcasts that may be one, and silence that loses the token
static const MSTP_Frame_t *idle(MSTP_t *mstp)
{
  MSTP_RX_t frame;
  while(parse(mstp, &frame)) {
    bool for_us = frame.destination == mstp->address;
    switch(frame.type) {
      case MSTP_Type_Token:
        if(!for_us) break;
        mstp->online = true;
        mstp->_sole = false;
        mstp->_state = MSTP_State_UseToken;
        return NULL;
      case MSTP_Type_PollForMaster:
        if(!for_us) break;
        send(mstp, MSTP_Type_ReplyToPollForMaster, frame.source, NULL, 0);
        return NULL;
      case MSTP_Type_TestRequest:
        if(!for_us) break;
        send(mstp, MSTP_Type_TestResponse, frame.source, frame.data, frame.len);
        return NULL;
      case MSTP_Type_DataExpectingReply:
        if(!for_us) break;
        return hand_over(mstp, &frame, true);
      case MSTP_Type_DataNotExpectingReply:
        if(!for_us && frame.destination != MSTP_BROADCAST) break;
        return hand_over(mstp, &frame, false);
    }
  }
  if(silence_ms(mstp) >= MSTP_NO_TOKEN_ms) {
    mstp->online = false;
    mstp->_event = 0;
    mstp->_state = MSTP_State_NoToken;
  }
  return NULL;
}

// Token lost: generated anew in this node's slot, or after every slot when that one is missed
static void no_token(MSTP_t *mstp)
{
  uint32_t silence = silence_ms(mstp);
  uint32_t slot = MSTP_NO_TOKEN_ms + MSTP_SLOT_ms * mstp->address;
  bool missed = silence >= slot + MSTP_SLOT_ms &&
    silence <= MSTP_NO_TOKEN_ms + MSTP_SLOT_ms * (MSTP_MAX_MASTER + 1);
  if(silence < slot || missed) {
    if(line_used(mstp)) mstp->_state = MSTP_State_Idle;
    return;
  }
  mstp->_ns = mstp->address;
  mstp->_token_count = 0;
  poll(mstp, next_station(mstp->address));
}

// One frame left by `MSTP_Send`, then the token goes on, after the answer to a request
static void use_token(MSTP_t *mstp)
{
  mstp->_state = MSTP_State_DoneWithToken;
  if(!mstp->_pending_len) return;
  MSTP_Type_t type = MSTP_Type_DataNotExpectingReply;
  if(mstp->_pending_request) {
    type = MSTP_Type_DataExpectingReply;
    mstp->_state = MSTP_State_WaitForReply;
  }
  send(mstp, type, mstp->_pending_to, mstp->_pending, mstp->_pending_len);
  heap_free(mstp->_pending);
  mstp->_pending = NULL;
  mstp->_pending_len = 0;
}

// Token to the successor, or a Poll For Master past `MSTP_POLL` tokens:
// one address of the gap up to the successor per token, errata SSPC-135-2004.
static void done_with_token(MSTP_t *mstp)
{
  uint8_t ts = mstp->address;
  if(!mstp->_sole && mstp->_ns == ts) {
    poll(mstp, next_station(ts)); // successor unknown
    return;
  }
  if(mstp->_token_count < MSTP_POLL - 1) {
    mstp->_token_count++;
    if(mstp->_sole && mstp->_ns != next_station(ts)) {
      mstp->_state = MSTP_State_UseToken; // sole master keeps the token
      return;
    }
    pass_token(mstp);
    return;
  }
  if(next_station(mstp->_ps) != mstp->_ns) {
    poll(mstp, next_station(mstp->_ps));
    return;
  }
  // Gap polled through: the next maintenance poll comes after `MSTP_POLL` tokens again
  mstp->_token_count = 1;
  if(mstp->_sole) {
    uint8_t station = next_station(mstp->_ns);
    mstp->_ns = ts;
    poll(mstp, station);
    return;
  }
  mstp->_ps = ts;
  pass_token(mstp);
}

// Successor uses the token, or gets it once more, or is replaced by the next node polled
static void wait_token_use(MSTP_t *mstp)
{
  if(silence_ms(mstp) <= MSTP_USAGE_TIMEOUT_ms) {
    if(line_used(mstp)) mstp->_state = MSTP_State_Idle;
    return;
  }
  if(mstp->_retry < MSTP_RETRY_TOKEN) {
    mstp->_retry++;
    mstp->retries++;
    mstp->_event = 0;
    send(mstp, MSTP_Type_Token, mstp->_ns, NULL, 0);
    return;
  }
  uint8_t station = next_station(mstp->_ns);
  mstp->_ns = mstp->address;
  mstp->_token_count = 0;
  poll(mstp, station);
}

// Reply To Poll For Master makes the polled node the successor.
// Another frame for this node takes the token away, a stray or broken one counts as no reply.
static void wait_poll_reply(MSTP_t *mstp)
{
  uint32_t errors = mstp->crc_errors;
  MSTP_RX_t frame;
  bool stray = false;
  if(parse(mstp, &frame)) {
    if(frame.destination == mstp->address || frame.destination == MSTP_BROADCAST) {
      if(frame.type != MSTP_Type_ReplyToPollForMaster || frame.destination != mstp->address) {
        mstp->_state = MSTP_State_Idle;
        return;
      }
      mstp->_sole = false;
      mstp->_ns = frame.source;
      mstp->_ps = mstp->address;
      mstp->_token_count = 0;
      pass_token(mstp);
      return;
    }
    stray = true;
  }
  bool broken = mstp->crc_errors != errors;
  if(!stray && !broken && silence_ms(mstp) <= MSTP_USAGE_TIMEOUT_ms) return;
  if(mstp->_sole) {
    mstp->_state = MSTP_State_UseToken;
    return;
  }
  if(mstp->_ns != mstp->address) {
    pass_token(mstp);
    return;
  }
  uint8_t station = next_station(mstp->_ps);
  if(station != mstp->address) {
    poll(mstp, station);
    return;
  }
  mstp->online = false; // the whole ring polled, nobody answered
  mstp->_sole = true;
  mstp->_state = MSTP_State_UseToken;
}

// Data Not Expecting Reply for this node is the answer and goes to the server.
// Reply Postponed, Test_Response, a broken frame, another node talking or silence end the wait;
// a broadcast, a request or a token cycle frame for this node means two tokens on the line,
// and this one is dropped.
static const MSTP_Frame_t *wait_reply(MSTP_t *mstp)
{
  uint32_t errors = mstp->crc_errors;
  MSTP_RX_t frame;
  if(parse(mstp, &frame)) {
    mstp->_state = MSTP_State_DoneWithToken;
    if(frame.destination == MSTP_BROADCAST) mstp->_state = MSTP_State_Idle;
    if(frame.destination != mstp->address) return NULL;
    switch(frame.type) {
      case MSTP_Type_DataNotExpectingReply:
        return hand_over(mstp, &frame, false);
      case MSTP_Type_TestResponse:
      case MSTP_Type_ReplyPostponed:
        return NULL;
    }
    mstp->_state = MSTP_State_Idle;
    return NULL;
  }
  if(mstp->crc_errors != errors || silence_ms(mstp) >= MSTP_REPLY_TIMEOUT_ms) {
    mstp->_state = MSTP_State_DoneWithToken;
  }
  return NULL;
}

// Receiver timeout of the standard set on the port, the successor unknown until polled
static void init(MSTP_t *mstp)
{
  uint32_t bits = mstp->uart->baud * MSTP_TIMEOUT_MIN_ms / 1000;
  if(bits < MSTP_FRAME_ABORT_BITS) bits = MSTP_FRAME_ABORT_BITS;
  UART_SetTimeout(mstp->uart, (uint16_t)bits);
  UART_Clear(mstp->uart);
  mstp->_ns = mstp->address;
  mstp->_ps = mstp->address;
  mstp->_token_count = MSTP_POLL; // the first token looks for a successor
  mstp->_state = MSTP_State_Idle;
  mstp->_silence_tick = tick_now();
  mstp->_init = true;
}

//--------------------------------------------------------------------------------------------- API

const MSTP_Frame_t *MSTP_Loop(MSTP_t *mstp)
{
  if(!mstp->_init) init(mstp);
  if(mstp->_sending) {
    if(!UART_SendCompleted(mstp->uart)) return NULL;
    mstp->_sending = false;
    mstp->_silence_tick = tick_now();
  }
  if(mstp->_answer) {
    mstp->_answer = false; // the server left it unanswered
    send(mstp, MSTP_Type_ReplyPostponed, mstp->_reply_to, NULL, 0);
    return NULL;
  }
  receive(mstp);
  // States run one after another within a pass, until one sends, hands a frame over or stays
  while(1) {
    MSTP_State_t state = mstp->_state;
    const MSTP_Frame_t *frame = NULL;
    switch(state) {
      case MSTP_State_Idle: frame = idle(mstp); break;
      case MSTP_State_NoToken: no_token(mstp); break;
      case MSTP_State_UseToken: use_token(mstp); break;
      case MSTP_State_DoneWithToken: done_with_token(mstp); break;
      case MSTP_State_PassToken: wait_token_use(mstp); break;
      case MSTP_State_PollForMaster: wait_poll_reply(mstp); break;
      case MSTP_State_WaitForReply: frame = wait_reply(mstp); break;
    }
    if(frame || mstp->_sending || mstp->_state == state) return frame;
  }
}

status_t MSTP_Reply(MSTP_t *mstp, const uint8_t *data, uint16_t len)
{
  if(!mstp->_answer || len > MSTP_DATA_MAX) return ERR;
  mstp->_answer = false;
  return send(mstp, MSTP_Type_DataNotExpectingReply, mstp->_reply_to, data, len);
}

status_t MSTP_Send(MSTP_t *mstp, uint8_t destination, const uint8_t *data, uint16_t len,
  bool expecting_reply)
{
  if(mstp->_pending_len) return BUSY;
  if(!len || len > MSTP_DATA_MAX) return ERR;
  mstp->_pending = heap_alloc(len);
  memcpy(mstp->_pending, data, len);
  mstp->_pending_len = len;
  mstp->_pending_to = destination;
  mstp->_pending_request = expecting_reply;
  return OK;
}

//-------------------------------------------------------------------------------------------------

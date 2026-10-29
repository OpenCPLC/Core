// plc/com/mstp.h

#ifndef MSTP_H_
#define MSTP_H_

#include <stdbool.h>
#include <stdint.h>

#include "uart.h"
#include "main.h"

//--------------------------------------------------------------------------------------- Constants

// Destination of a frame for every node
#define MSTP_BROADCAST 255

// Longest NPDU in one frame [B]
#define MSTP_DATA_MAX 501

//------------------------------------------------------------------------------------------- Types

// State of the master node, after the standard
typedef enum {
  MSTP_State_Idle,
  MSTP_State_NoToken,
  MSTP_State_UseToken,
  MSTP_State_DoneWithToken,
  MSTP_State_PassToken,
  MSTP_State_PollForMaster,
  MSTP_State_WaitForReply,
} MSTP_State_t;

/**
 * @brief Data frame the link hands to the server.
 * @param[out] data NPDU, valid until the next `MSTP_Loop`
 * @param[out] len NPDU bytes
 * @param[out] source MAC of the sender
 * @param[out] expecting_reply `MSTP_Reply` before the next `MSTP_Loop`, Reply Postponed otherwise
 */
typedef struct {
  const uint8_t *data;
  uint16_t len;
  uint8_t source;
  bool expecting_reply;
} MSTP_Frame_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief MS/TP master node on an RS485 port.
 * @param[in] uart Port, 8N1, its receiver timeout set by the link
 * @param[in] address MAC `0`–`127`, unique on the line
 * @param[out] online In a ring with another master; a node alone on the line is never online
 * @param[out] retries Tokens passed to the successor a second time
 * @param[out] crc_errors Frames dropped on a bad CRC
 * Internal:
 * @param _state Master node state
 * @param _ns Next station, the successor
 * @param _ps Poll station, the last address polled
 * @param _token_count Tokens passed since the last maintenance poll
 * @param _retry Token sent to the successor again
 * @param _sole Sole master on the line
 * @param _event Bytes received since the state was entered
 * @param _silence_tick Tick of the last activity on the line
 * @param _rx Message being parsed
 * @param _rx_len Message bytes
 * @param _rx_pos Parse position
 * @param _tx Frame on the line
 * @param _sending Frame on the line, its last bit not out yet
 * @param _pending NPDU waiting for the token
 * @param _pending_len NPDU bytes, `0` = none
 * @param _pending_to Destination of the waiting NPDU
 * @param _pending_request The waiting NPDU is a request, the token held until its answer
 * @param _answer A frame expecting a reply waits for `MSTP_Reply`
 * @param _reply_to Sender of that frame
 * @param _frame Frame handed to the server
 * @param _init Initialization completed flag
 */
typedef struct {
  UART_t *uart;
  uint8_t address;
  bool online;
  uint32_t retries;
  uint32_t crc_errors;
  // internal
  MSTP_State_t _state;
  uint8_t _ns;
  uint8_t _ps;
  uint8_t _token_count;
  uint8_t _retry;
  bool _sole;
  uint16_t _event;
  uint64_t _silence_tick;
  uint8_t *_rx;
  uint16_t _rx_len;
  uint16_t _rx_pos;
  uint8_t *_tx;
  bool _sending;
  uint8_t *_pending;
  uint16_t _pending_len;
  uint8_t _pending_to;
  bool _pending_request;
  bool _answer;
  uint8_t _reply_to;
  MSTP_Frame_t _frame;
  bool _init;
} MSTP_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Run the link: receive, keep the token ring, send.
 *   Call it every pass of its own thread, at most 5ms apart.
 * @param[in,out] mstp Link
 * @return Data frame for the server, `NULL` when there is none
 */
const MSTP_Frame_t *MSTP_Loop(MSTP_t *mstp);

/**
 * @brief Answer the frame `MSTP_Loop` handed over with `expecting_reply`.
 * @param[in,out] mstp Link
 * @param[in] data NPDU of the answer
 * @param[in] len NPDU bytes, `MSTP_DATA_MAX` at most
 * @return `OK` when sent, `ERR` with no frame to answer or an NPDU too long
 */
status_t MSTP_Reply(MSTP_t *mstp, const uint8_t *data, uint16_t len);

/**
 * @brief Leave one frame to send when the node holds the token.
 *   The answer to a request comes from `MSTP_Loop` like any data frame.
 * @param[in,out] mstp Link
 * @param[in] destination MAC, `MSTP_BROADCAST` for every node
 * @param[in] data NPDU
 * @param[in] len NPDU bytes, `1` to `MSTP_DATA_MAX`
 * @param[in] expecting_reply Request, token held for its answer, a Reply Postponed or a timeout
 * @return `OK` when queued, `BUSY` while one waits, `ERR` on a bad length
 */
status_t MSTP_Send(MSTP_t *mstp, uint8_t destination, const uint8_t *data, uint16_t len,
  bool expecting_reply);

//-------------------------------------------------------------------------------------------------
#endif

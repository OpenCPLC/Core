// lib/sh/stream.h

#ifndef STREAM_H_
#define STREAM_H_

#include "log.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef STREAM_ADDRESS
  // First byte of every line is a device address, lines for others are dropped
  #define STREAM_ADDRESS OFF
#endif

#ifndef STREAM_CRC
  // Every line ends with a checksum verified against `crc`
  #define STREAM_CRC OFF
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  STREAM_Modify_None = 0,
  STREAM_Modify_Lowercase = 1,
  STREAM_Modify_Uppercase = 2
} STREAM_Modify_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Line source of the shell: a port wrapped in reader callbacks.
 * @param[in] name Stream identifier
 * @param[in] modify Case applied to the tokens
 * @param[in] Read Take the pending line as a heap string
 * @param[in] Size Bytes of the pending line, `0` = nothing waits
 * @param[in] Send Reply callback, `NULL` = none
 * @param[in] SwitchMode Called with `true` entering a binary transfer, `false` leaving it
 * @param[in] file Output buffer of the responses, `NULL` = the console one
 * @param[in] Readdress Called when the `addr` command changes the address
 * @param[in] address Device address matched against the line prefix
 * @param[in] crc Checksum of the lines
 * Internal:
 * @param _data_mode Binary transfer in progress
 * @param _packages Frames left in the transfer
 */
typedef struct {
  const char *name;
  STREAM_Modify_t modify;
  char *(*Read)(void);
  uint16_t (*Size)(void);
  void (*Send)(uint8_t *, uint16_t);
  void (*SwitchMode)(bool);
  MBB_t *file;
  #if(STREAM_ADDRESS)
  void (*Readdress)(uint8_t);
  uint8_t address;
  #endif
  #if(STREAM_CRC)
  CRC_t *crc;
  #endif
  // internal
  bool _data_mode;
  uint16_t _packages;
} STREAM_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Take the pending line, tokenized in argument mode, raw in data mode.
 * @param[in,out] stream Stream instance
 * @param[out] argv Tokens, or one entry holding the raw bytes
 * @return Token count, or the byte count in data mode; `0` when nothing waits
 */
uint16_t STREAM_Read(STREAM_t *stream, char ***argv);

// Enter or leave the binary transfer mode
void STREAM_DataMode(STREAM_t *stream);
void STREAM_ArgsMode(STREAM_t *stream);

//-------------------------------------------------------------------------------------------------
#endif

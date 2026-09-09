// lib/sh/stream.c

#include "stream.h"

#include <string.h>
#include "dbg.h"
#include "heap.h"
#include "xstring.h"

//--------------------------------------------------------------------------------------------- API

uint16_t STREAM_Read(STREAM_t *stream, char ***argv)
{
  if(stream->file) DBG_SetFile(stream->file);
  else DBG_DefaultFile();
  uint16_t length = stream->Size();
  if(!length) return 0;
  char *buffer = stream->Read();
  #if(STREAM_ADDRESS)
  if((uint8_t)*buffer != stream->address) return 0;
  buffer++;
  length--;
  #endif
  #if(STREAM_CRC)
  if(CRC_Error(stream->crc, (uint8_t *)buffer, length)) return 0;
  length -= stream->crc->width / 8;
  #endif
  if(stream->_data_mode) {
    // One entry pointing at the raw bytes, both in a single block
    char **raw = heap_new(sizeof(char *) + length);
    raw[0] = (char *)(raw + 1);
    memcpy(raw[0], buffer, length);
    *argv = raw;
    return length;
  }
  int argc = str_explode(argv, str_trim(buffer), ' ');
  for(int i = 0; i < argc; i++) {
    if(stream->modify == STREAM_Modify_Lowercase) str_lower_this((*argv)[i]);
    else if(stream->modify == STREAM_Modify_Uppercase) str_upper_this((*argv)[i]);
  }
  return (uint16_t)argc;
}

void STREAM_DataMode(STREAM_t *stream)
{
  stream->_data_mode = true;
  if(stream->SwitchMode) stream->SwitchMode(true);
}

void STREAM_ArgsMode(STREAM_t *stream)
{
  stream->_data_mode = false;
  if(stream->SwitchMode) stream->SwitchMode(false);
}

//-------------------------------------------------------------------------------------------------

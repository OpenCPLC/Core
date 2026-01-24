#include "stream.h"

//-------------------------------------------------------------------------------------------------

uint16_t STREAM_Read(STREAM_t *stream, char ***argv)
{
  if(stream->file) DBG_SetFile(stream->file);
  else DBG_DefaultFile();
  uint16_t length = stream->Size();
  if(length) {
    char *buffer = stream->Read();
    #if(STREAM_ADDRESS)
      uint8_t address = *buffer;
      if(address != stream->address) return 0;
      buffer++;
      length--;
    #endif
    #if(STREAM_CRC)
      if(CRC_Error(stream->crc, buffer, length)) return 0;
      length -= stream_crc->width / 8;
    #endif
    if(stream->data_mode) {
      char **file;
      char *loc;
      file = heap_new(sizeof(char*) + (length * sizeof(char)));
      loc = (char*)file + sizeof(char*);
      memcpy(loc, buffer, length);
      file[0] = loc;
      *argv = file;
      return length;
    }
    else {
      buffer = str_trim(buffer);
      int argc = str_explode(argv, buffer, ' ');
      if(stream->modify == STREAM_Modify_Lowercase || stream->modify == STREAM_Modify_Uppercase) {
        for(int i = 0; i < argc; i++) {
          if(stream->modify == STREAM_Modify_Lowercase) str_lower_this((*argv)[i]);
          else if(stream->modify == STREAM_Modify_Uppercase) str_upper_this((*argv)[i]);
        }
      }
      return (uint8_t)argc;
    }
  }
  return 0;
}

void STREAM_DataMode(STREAM_t *stream)
{
  stream->data_mode = true;
  if(stream->SwitchMode) stream->SwitchMode(stream->data_mode);
}

void STREAM_ArgsMode(STREAM_t *stream)
{
  stream->data_mode = false;
  if(stream->SwitchMode) stream->SwitchMode(stream->data_mode);
}

//-------------------------------------------------------------------------------------------------

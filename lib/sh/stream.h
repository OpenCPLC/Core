#ifndef STREAM_H_
#define STREAM_H_

#include "log.h"
#include "main.h"

#ifndef STREAM_ADDRESS
  #define STREAM_ADDRESS 0
#endif

#ifndef STREAM_CRC
  #define STREAM_CRC 0
#endif

//-------------------------------------------------------------------------------------------------

typedef enum {
  STREAM_Modify_Free = 0,
  STREAM_Modify_Lowercase = 1,
  STREAM_Modify_Uppercase = 2
} STREAM_Modify_e;

typedef struct {
  const char *name;
  bool data_mode;
  STREAM_Modify_e modify;
  char *(*Read)(void);
  uint16_t (*Size)(void);
  void (*Send)(uint8_t *, uint16_t);
  uint16_t packages;
  #if(STREAM_ADDRESS)
    void (*Readdress)(uint8_t);
    uint8_t address;
  #endif
  FILE_t *file; // Plik przekazywany buildera odpowiedzi (takiego jak bash)
  void (*SwitchMode)(bool);
  #if(STREAM_CRC)
    CRC_t *crc;
  #endif
} STREAM_t;

//-------------------------------------------------------------------------------------------------

uint16_t STREAM_Read(STREAM_t *stream, char ***argv);
void STREAM_DataMode(STREAM_t *stream);
void STREAM_ArgsMode(STREAM_t *stream);

//-------------------------------------------------------------------------------------------------
#endif

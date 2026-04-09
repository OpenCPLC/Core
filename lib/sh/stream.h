// lib/sh/stream.h

#ifndef STREAM_H_
#define STREAM_H_

#include "log.h"
#include "main.h"

#ifndef STREAM_ADDRESS
  #define STREAM_ADDRESS OFF
#endif

#ifndef STREAM_CRC
  #define STREAM_CRC OFF
#endif

//-------------------------------------------------------------------------------------------------

typedef enum {
  STREAM_Modify_None = 0,
  STREAM_Modify_Lowercase = 1,
  STREAM_Modify_Uppercase = 2
} STREAM_Modify_t;

/**
 * @brief Stream interface for command processing.
 * @param[in] name Stream identifier
 * @param[in] modify Case modification mode
 * @param[in] Read Read string from stream
 * @param[in] Size Get available data size
 * @param[in] Send Send data callback
 * @param[in] SwitchMode Switch between data/args mode
 * @param[in] file Output file for responses
 * @param[in] address Stream address (when `STREAM_ADDRESS` enabled)
 * @param[in] Readdress Address change callback
 * @param[in] crc CRC configuration (when `STREAM_CRC` enabled)
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

//-------------------------------------------------------------------------------------------------

/**
 * @brief Read and parse data from stream.
 * @param[in,out] stream Stream instance
 * @param[out] argv Pointer to argument array
 * @return Argument count or data length in data mode
 */
uint16_t STREAM_Read(STREAM_t *stream, char ***argv);

/**
 * @brief Switch stream to binary data mode.
 * @param[in,out] stream Stream instance
 */
void STREAM_DataMode(STREAM_t *stream);

/**
 * @brief Switch stream to argument parsing mode.
 * @param[in,out] stream Stream instance
 */
void STREAM_ArgsMode(STREAM_t *stream);

//-------------------------------------------------------------------------------------------------

#endif
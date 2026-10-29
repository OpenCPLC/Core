// plc/com/bacnet_tag.h

#ifndef BACNET_TAG_H_
#define BACNET_TAG_H_

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

//------------------------------------------------------------------------------------------- Types

// Application tag numbers: the type of a value
typedef enum {
  BACNET_App_Null = 0,
  BACNET_App_Boolean = 1,
  BACNET_App_Unsigned = 2,
  BACNET_App_Signed = 3,
  BACNET_App_Real = 4,
  BACNET_App_Double = 5,
  BACNET_App_OctetString = 6,
  BACNET_App_CharacterString = 7,
  BACNET_App_BitString = 8,
  BACNET_App_Enumerated = 9,
  BACNET_App_Date = 10,
  BACNET_App_Time = 11,
  BACNET_App_ObjectIdentifier = 12,
} BACNET_App_t;

/**
 * @brief Bytes being encoded.
 * @param[in] data Buffer
 * @param[in] size Buffer bytes
 * @param[out] len Bytes written
 * @param[out] full A value did not fit, the bytes after it are missing
 */
typedef struct {
  uint8_t *data;
  uint16_t size;
  uint16_t len;
  bool full;
} BACNET_Tag_Out_t;

/**
 * @brief Bytes being decoded.
 * @param[in] data Bytes
 * @param[in] len Byte count
 * @param[out] pos Position past the last tag read
 * @param[out] bad A tag was malformed or ran past the end
 */
typedef struct {
  const uint8_t *data;
  uint16_t len;
  uint16_t pos;
  bool bad;
} BACNET_Tag_In_t;

/**
 * @brief Tag read off the bytes.
 * @param[out] number Application type, or the field of a context tag
 * @param[out] context Context tag, an application one otherwise
 * @param[out] opening Opening tag
 * @param[out] closing Closing tag
 * @param[out] len Content bytes
 * @param[out] value Content of up to 4 bytes, big-endian; an application Boolean's value
 */
typedef struct {
  uint8_t number;
  bool context;
  bool opening;
  bool closing;
  uint32_t len;
  uint32_t value;
} BACNET_Tag_t;

//------------------------------------------------------------------------------------------ Encode

// Raw byte, as the headers of NPDU and APDU take them
void BACNET_Tag_Byte(BACNET_Tag_Out_t *out, uint8_t value);

// Opening and closing tag of context field `number`
void BACNET_Tag_Open(BACNET_Tag_Out_t *out, uint8_t number);
void BACNET_Tag_Close(BACNET_Tag_Out_t *out, uint8_t number);

// Application values
void BACNET_Tag_Boolean(BACNET_Tag_Out_t *out, bool value);
void BACNET_Tag_Unsigned(BACNET_Tag_Out_t *out, uint32_t value);
void BACNET_Tag_Enumerated(BACNET_Tag_Out_t *out, uint32_t value);
void BACNET_Tag_Real(BACNET_Tag_Out_t *out, float value);
void BACNET_Tag_Object(BACNET_Tag_Out_t *out, uint16_t type, uint32_t instance);

// Character string in UTF-8, `NULL` as an empty one
void BACNET_Tag_String(BACNET_Tag_Out_t *out, const char *value);

/**
 * @brief Bit string, bit `0` the most significant bit of the first byte.
 * @param[in,out] out Bytes being encoded
 * @param[in] bits Bits in that order
 * @param[in] count Bit count
 */
void BACNET_Tag_Bits(BACNET_Tag_Out_t *out, const uint8_t *bits, uint16_t count);

// Context values of field `number`
void BACNET_Tag_ContextUnsigned(BACNET_Tag_Out_t *out, uint8_t number, uint32_t value);
void BACNET_Tag_ContextObject(BACNET_Tag_Out_t *out, uint8_t number,
  uint16_t type, uint32_t instance);

//------------------------------------------------------------------------------------------ Decode

/**
 * @brief Next tag with its content.
 * @param[in,out] in Bytes being decoded, `pos` moved past the tag
 * @param[out] tag Tag read
 * @return `true` for a tag, `false` at the end or on a malformed one, which also sets `bad`
 */
bool BACNET_Tag_Read(BACNET_Tag_In_t *in, BACNET_Tag_t *tag);

//-------------------------------------------------------------------------------------------------
#endif

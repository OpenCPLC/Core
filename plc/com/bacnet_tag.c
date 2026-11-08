// plc/com/bacnet_tag.c

#include "bacnet_tag.h"

#include <string.h>

//------------------------------------------------------------------------------------------ Encode

void BACNET_Tag_Byte(BACNET_Tag_Out_t *out, uint8_t value)
{
  if(out->len >= out->size) {
    out->full = true;
    return;
  }
  out->data[out->len++] = value;
}

// Tag octet, then the number past `14` and the length past `4` in the octets after it
static void header(BACNET_Tag_Out_t *out, uint8_t number, bool context, uint8_t lvt)
{
  uint8_t octet = (number < 15 ? number << 4 : 0xF0) | (context ? 0x08 : 0x00) | lvt;
  BACNET_Tag_Byte(out, octet);
  if(number >= 15) BACNET_Tag_Byte(out, number);
}

static void tag(BACNET_Tag_Out_t *out, uint8_t number, bool context, uint32_t len)
{
  if(len < 5) {
    header(out, number, context, (uint8_t)len);
    return;
  }
  header(out, number, context, 5);
  if(len < 254) {
    BACNET_Tag_Byte(out, (uint8_t)len);
    return;
  }
  uint8_t octets = len < 0x10000 ? 2 : 4;
  BACNET_Tag_Byte(out, octets == 2 ? 254 : 255);
  while(octets--) BACNET_Tag_Byte(out, (uint8_t)(len >> (8 * octets)));
}

// Unsigned content in as few bytes as hold it
static uint8_t octets_of(uint32_t value)
{
  if(value < 0x100) return 1;
  if(value < 0x10000) return 2;
  if(value < 0x1000000) return 3;
  return 4;
}

static void content(BACNET_Tag_Out_t *out, uint32_t value, uint8_t len)
{
  while(len--) BACNET_Tag_Byte(out, (uint8_t)(value >> (8 * len)));
}

static uint32_t object_id(uint16_t type, uint32_t instance) {
  return ((uint32_t)type << 22) | (instance & 0x3FFFFF);
}

void BACNET_Tag_Open(BACNET_Tag_Out_t *out, uint8_t number)
{
  header(out, number, true, 6);
}

void BACNET_Tag_Close(BACNET_Tag_Out_t *out, uint8_t number)
{
  header(out, number, true, 7);
}

// An application Boolean carries its value in the length field
void BACNET_Tag_Boolean(BACNET_Tag_Out_t *out, bool value)
{
  header(out, BACNET_App_Boolean, false, value ? 1 : 0);
}

void BACNET_Tag_Unsigned(BACNET_Tag_Out_t *out, uint32_t value)
{
  uint8_t len = octets_of(value);
  tag(out, BACNET_App_Unsigned, false, len);
  content(out, value, len);
}

void BACNET_Tag_Enumerated(BACNET_Tag_Out_t *out, uint32_t value)
{
  uint8_t len = octets_of(value);
  tag(out, BACNET_App_Enumerated, false, len);
  content(out, value, len);
}

void BACNET_Tag_Real(BACNET_Tag_Out_t *out, float value)
{
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  tag(out, BACNET_App_Real, false, 4);
  content(out, bits, 4);
}

void BACNET_Tag_Object(BACNET_Tag_Out_t *out, uint16_t type, uint32_t instance)
{
  tag(out, BACNET_App_ObjectIdentifier, false, 4);
  content(out, object_id(type, instance), 4);
}

// Character set `0`, ANSI X3.4 in the oldest editions and UTF-8 since
void BACNET_Tag_String(BACNET_Tag_Out_t *out, const char *value)
{
  uint32_t len = value ? strlen(value) : 0;
  tag(out, BACNET_App_CharacterString, false, len + 1);
  BACNET_Tag_Byte(out, 0);
  for(uint32_t i = 0; i < len; i++) BACNET_Tag_Byte(out, (uint8_t)value[i]);
}

// Unused bits of the last byte counted in the byte ahead of the bits
void BACNET_Tag_Bits(BACNET_Tag_Out_t *out, const uint8_t *bits, uint16_t count)
{
  uint16_t bytes = (count + 7) / 8;
  tag(out, BACNET_App_BitString, false, bytes + 1u);
  BACNET_Tag_Byte(out, (uint8_t)(8 * bytes - count));
  for(uint16_t i = 0; i < bytes; i++) BACNET_Tag_Byte(out, bits[i]);
}

void BACNET_Tag_ContextUnsigned(BACNET_Tag_Out_t *out, uint8_t number, uint32_t value)
{
  uint8_t len = octets_of(value);
  tag(out, number, true, len);
  content(out, value, len);
}

void BACNET_Tag_ContextObject(BACNET_Tag_Out_t *out, uint8_t number,
  uint16_t type, uint32_t instance)
{
  tag(out, number, true, 4);
  content(out, object_id(type, instance), 4);
}

//------------------------------------------------------------------------------------------ Decode

static uint8_t take(BACNET_Tag_In_t *in)
{
  if(in->pos >= in->len) {
    in->bad = true;
    return 0;
  }
  return in->data[in->pos++];
}

// Length past `4`: one more byte, or `254` and two, or `255` and four
static uint32_t extended_len(BACNET_Tag_In_t *in)
{
  uint32_t len = take(in);
  uint8_t octets = len == 254 ? 2 : len == 255 ? 4 : 0;
  if(octets) len = 0;
  while(octets--) len = (len << 8) | take(in);
  return len;
}

bool BACNET_Tag_Read(BACNET_Tag_In_t *in, BACNET_Tag_t *tag)
{
  memset(tag, 0, sizeof(*tag));
  if(in->bad || in->pos >= in->len) return false;
  uint8_t octet = take(in);
  uint8_t lvt = octet & 0x07;
  tag->context = octet & 0x08;
  tag->number = octet >> 4;
  if(tag->number == 15) tag->number = take(in);
  if(lvt >= 6) {
    // Opening and closing tags exist in the context class only
    if(!tag->context) in->bad = true;
    tag->opening = lvt == 6;
    tag->closing = lvt == 7;
    return !in->bad;
  }
  if(!tag->context && tag->number == BACNET_App_Boolean) {
    tag->value = lvt;
    return !in->bad;
  }
  uint32_t len = lvt == 5 ? extended_len(in) : lvt;
  if(in->bad || len > (uint32_t)(in->len - in->pos)) {
    in->bad = true;
    return false;
  }
  tag->len = len;
  if(len <= 4) {
    for(uint32_t i = 0; i < len; i++) tag->value = (tag->value << 8) | in->data[in->pos + i];
  }
  in->pos += len;
  return true;
}

//-------------------------------------------------------------------------------------------------

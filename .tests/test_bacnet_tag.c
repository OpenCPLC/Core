// .tests/test_bacnet_tag.c

// BACnet tags against the encodings of the standard: values of the Annex F examples,
// unsigned values in the fewest bytes across each width, strings across the length bounds,
// bit strings from the most significant bit, extended tag numbers,
// opening and closing tags, a full buffer, and the same bytes decoded back,
// malformed ones marked bad.

#include <string.h>

#include "test.h"
#include "plc/com/bacnet_tag.c"

static uint8_t memory[300];
static BACNET_Tag_Out_t out;

// Empty buffer of `size` bytes
static void fresh(uint16_t size)
{
  memset(memory, 0, sizeof(memory));
  out = (BACNET_Tag_Out_t){ .data = memory, .size = size };
}

// Bytes to decode
static BACNET_Tag_In_t bytes(const uint8_t *data, uint16_t len) {
  return (BACNET_Tag_In_t){ .data = data, .len = len };
}

//------------------------------------------------------------------------------------------ Encode

// I-Am of Annex F.4.9, then each width boundary of an unsigned value
static void unsigned_in_fewest_bytes(void)
{
  fresh(sizeof(memory));
  BACNET_Tag_Unsigned(&out, 1024);
  BACNET_Tag_Enumerated(&out, 3);
  BACNET_Tag_Unsigned(&out, 99);
  static const uint8_t i_am[] = { 0x22, 0x04, 0x00, 0x91, 0x03, 0x21, 0x63 };
  TEST_Equal(out.len, sizeof(i_am), "I-Am values");
  TEST_Bytes(memory, i_am, sizeof(i_am), "Max_APDU 1024, no segmentation, vendor 99");
  fresh(sizeof(memory));
  static const uint32_t values[] = { 0, 255, 256, 65535, 65536, 0xFFFFFF, 0x1000000, 0xFFFFFFFF };
  for(uint8_t i = 0; i < array_len(values); i++) BACNET_Tag_Unsigned(&out, values[i]);
  static const uint8_t bounds[] = {
    0x21, 0x00, 0x21, 0xFF, 0x22, 0x01, 0x00, 0x22, 0xFF, 0xFF, 0x23, 0x01, 0x00, 0x00,
    0x23, 0xFF, 0xFF, 0xFF, 0x24, 0x01, 0x00, 0x00, 0x00, 0x24, 0xFF, 0xFF, 0xFF, 0xFF
  };
  TEST_Equal(out.len, sizeof(bounds), "one to four bytes");
  TEST_Bytes(memory, bounds, sizeof(bounds), "0 and 255 in one byte, 256 in two, up to four");
  TEST_Check(!out.full, "room left");
}

// Values of the ReadProperty and WriteProperty examples of Annex F.3
static void real_boolean_and_object(void)
{
  fresh(sizeof(memory));
  BACNET_Tag_Real(&out, 72.3f);
  BACNET_Tag_Real(&out, 180.0f);
  BACNET_Tag_Object(&out, 2, 1);
  BACNET_Tag_Object(&out, 8, 4194303);
  BACNET_Tag_Boolean(&out, false);
  BACNET_Tag_Boolean(&out, true);
  static const uint8_t expected[] = {
    0x44, 0x42, 0x90, 0x99, 0x9A, 0x44, 0x43, 0x34, 0x00, 0x00,
    0xC4, 0x00, 0x80, 0x00, 0x01, 0xC4, 0x02, 0x3F, 0xFF, 0xFF, 0x10, 0x11
  };
  TEST_Equal(out.len, sizeof(expected), "six values");
  TEST_Bytes(memory, expected, sizeof(expected), "72.3, 180.0, Analog Value 1, Device 4194303");
}

// Fields of the ReadProperty request of Annex F.3.5 and the Who-Is of F.4.9,
// a tag number past 14 in the octet after the tag.
static void context_and_constructed(void)
{
  fresh(sizeof(memory));
  BACNET_Tag_ContextObject(&out, 0, 0, 5);
  BACNET_Tag_ContextUnsigned(&out, 1, 85);
  BACNET_Tag_Open(&out, 3);
  BACNET_Tag_Close(&out, 3);
  BACNET_Tag_ContextUnsigned(&out, 0, 3);
  BACNET_Tag_ContextUnsigned(&out, 1, 3);
  BACNET_Tag_ContextUnsigned(&out, 15, 1);
  BACNET_Tag_Open(&out, 254);
  BACNET_Tag_Close(&out, 254);
  static const uint8_t expected[] = {
    0x0C, 0x00, 0x00, 0x00, 0x05, 0x19, 0x55, 0x3E, 0x3F, 0x09, 0x03, 0x19, 0x03,
    0xF9, 0x0F, 0x01, 0xFE, 0xFE, 0xFF, 0xFE
  };
  TEST_Equal(out.len, sizeof(expected), "nine tags");
  TEST_Bytes(memory, expected, sizeof(expected), "context values, opening and closing");
}

// Strings of Annex F, then content of 4 and 5 bytes, 253 and 254 bytes
static void strings_across_length_bounds(void)
{
  fresh(sizeof(memory));
  BACNET_Tag_String(&out, "OATemp");
  BACNET_Tag_String(&out, NULL);
  BACNET_Tag_String(&out, "abc");
  BACNET_Tag_String(&out, "abcd");
  static const uint8_t expected[] = {
    0x75, 0x07, 0x00, 0x4F, 0x41, 0x54, 0x65, 0x6D, 0x70, 0x71, 0x00,
    0x74, 0x00, 0x61, 0x62, 0x63, 0x75, 0x05, 0x00, 0x61, 0x62, 0x63, 0x64
  };
  TEST_Equal(out.len, sizeof(expected), "four strings");
  TEST_Bytes(memory, expected, sizeof(expected), "length in the tag up to 4, in a byte after it");
  char text[254];
  memset(text, 'x', sizeof(text) - 1);
  text[252] = 0;
  fresh(sizeof(memory));
  BACNET_Tag_String(&out, text);
  static const uint8_t short_head[] = { 0x75, 0xFD, 0x00, 'x' };
  TEST_Equal(out.len, 255, "253 bytes of content");
  TEST_Bytes(memory, short_head, sizeof(short_head), "253 still in one byte");
  text[252] = 'x';
  text[253] = 0;
  fresh(sizeof(memory));
  BACNET_Tag_String(&out, text);
  static const uint8_t long_head[] = { 0x75, 0xFE, 0x00, 0xFE, 0x00, 'x' };
  TEST_Equal(out.len, 258, "254 bytes of content");
  TEST_Bytes(memory, long_head, sizeof(long_head), "254 behind the mark 254, in two bytes");
}

// `Status_Flags`, the services and the object types of a device of revision 12
static void bit_strings_from_the_top(void)
{
  fresh(sizeof(memory));
  static const uint8_t flags[] = { 0x00 };
  static const uint8_t services[] = { 0x00, 0x09, 0x00, 0x00, 0x20 };
  static const uint8_t types[] = { 0xB4, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00 };
  static const uint8_t five[] = { 0xA8 };
  BACNET_Tag_Bits(&out, flags, 4);
  BACNET_Tag_Bits(&out, services, 40);
  BACNET_Tag_Bits(&out, types, 51);
  BACNET_Tag_Bits(&out, five, 5);
  static const uint8_t expected[] = {
    0x82, 0x04, 0x00,
    0x85, 0x06, 0x00, 0x00, 0x09, 0x00, 0x00, 0x20,
    0x85, 0x08, 0x05, 0xB4, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x82, 0x03, 0xA8
  };
  TEST_Equal(out.len, sizeof(expected), "four bit strings");
  TEST_Bytes(memory, expected, sizeof(expected), "unused bits counted ahead of the bits");
}

static void full_buffer_stops(void)
{
  fresh(4);
  BACNET_Tag_Real(&out, 21.5f);
  TEST_Check(out.full && out.len == 4, "a REAL needs five bytes");
  TEST_Equal(memory[4], 0, "nothing past the buffer");
  BACNET_Tag_Byte(&out, 0x3F);
  TEST_Check(out.full && out.len == 4, "still full");
}

//------------------------------------------------------------------------------------------ Decode

// ReadProperty ack of Annex F.3.5 back into its tags
static void reads_the_annex_f_ack(void)
{
  static const uint8_t ack[] = {
    0x0C, 0x00, 0x00, 0x00, 0x05, 0x19, 0x55, 0x3E, 0x44, 0x42, 0x90, 0x99, 0x9A, 0x3F
  };
  BACNET_Tag_In_t in = bytes(ack, sizeof(ack));
  BACNET_Tag_t tag;
  TEST_Check(BACNET_Tag_Read(&in, &tag), "object");
  TEST_Check(tag.context && tag.number == 0 && tag.len == 4 && tag.value == 5, "Analog Input 5");
  TEST_Check(BACNET_Tag_Read(&in, &tag), "property");
  TEST_Check(tag.context && tag.number == 1 && tag.len == 1 && tag.value == 85, "Present_Value");
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.opening && tag.number == 3, "opening 3");
  TEST_Check(BACNET_Tag_Read(&in, &tag), "value");
  TEST_Check(!tag.context && tag.number == BACNET_App_Real && tag.value == 0x4290999A, "REAL");
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.closing && tag.number == 3, "closing 3");
  TEST_Check(!BACNET_Tag_Read(&in, &tag) && !in.bad, "end, nothing wrong");
}

static void reads_lengths_and_numbers(void)
{
  static const uint8_t values[] = {
    0x11, 0x10, 0x75, 0x05, 0x00, 0x61, 0x62, 0x63, 0x64,
    0xF9, 0x0F, 0x01, 0xFE, 0xFE, 0x24, 0xFF, 0xFF, 0xFF, 0xFF
  };
  BACNET_Tag_In_t in = bytes(values, sizeof(values));
  BACNET_Tag_t tag;
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.value == 1 && !tag.len, "TRUE in the tag");
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.value == 0 && !tag.len, "FALSE");
  TEST_Check(BACNET_Tag_Read(&in, &tag), "string");
  TEST_Check(tag.number == BACNET_App_CharacterString && tag.len == 5 && in.pos == 9,
    "length 5 in the byte after the tag");
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.context && tag.number == 15 && tag.value == 1,
    "tag number 15 in the byte after the tag");
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.opening && tag.number == 254, "opening 254");
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.value == 0xFFFFFFFF, "four bytes");
  uint8_t text[258] = { 0x75, 0xFE, 0x00, 0xFE };
  in = bytes(text, sizeof(text));
  TEST_Check(BACNET_Tag_Read(&in, &tag) && tag.len == 254 && in.pos == 258, "254 in two bytes");
}

// A tag past the end, a cut length or number, and an opening tag of the application class
static void malformed_tags_are_bad(void)
{
  static const uint8_t cut_content[] = { 0x22, 0x01 };
  static const uint8_t cut_length[] = { 0x75 };
  static const uint8_t cut_long_length[] = { 0x75, 0xFE, 0x00 };
  static const uint8_t cut_number[] = { 0xF9 };
  static const uint8_t application_opening[] = { 0x06 };
  static const struct {
    const uint8_t *data;
    uint16_t len;
  } cases[] = {
    { cut_content, sizeof(cut_content) },
    { cut_length, sizeof(cut_length) },
    { cut_long_length, sizeof(cut_long_length) },
    { cut_number, sizeof(cut_number) },
    { application_opening, sizeof(application_opening) }
  };
  BACNET_Tag_t tag;
  for(uint8_t i = 0; i < array_len(cases); i++) {
    BACNET_Tag_In_t in = bytes(cases[i].data, cases[i].len);
    TEST_Check(!BACNET_Tag_Read(&in, &tag) && in.bad, "case %u bad", i);
    TEST_Check(!BACNET_Tag_Read(&in, &tag), "case %u stays bad", i);
  }
  BACNET_Tag_In_t in = bytes(cut_content, 0);
  TEST_Check(!BACNET_Tag_Read(&in, &tag) && !in.bad, "no bytes: the end, not bad");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(unsigned_in_fewest_bytes);
  TEST(real_boolean_and_object);
  TEST(context_and_constructed);
  TEST(strings_across_length_bounds);
  TEST(bit_strings_from_the_top);
  TEST(full_buffer_stops);
  TEST(reads_the_annex_f_ack);
  TEST(reads_lengths_and_numbers);
  TEST(malformed_tags_are_bad);
}

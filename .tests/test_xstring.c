// .tests/test_xstring.c
// Strings: case tables map the letters and invert each other, numbers with unit suffixes parse,
// integers split and print back in every base the library writes.

#include <math.h>
#include <string.h>

#include "test.h"
#include "xstring.c"

static void case_tables_map_letters(void)
{
  for(char upper = 'A'; upper <= 'Z'; upper++) {
    char lower = (char)(upper - 'A' + 'a');
    TEST_Check(LowerCase[(uint8_t)upper] == lower, "lower of %c", upper);
    TEST_Check(UpperCase[(uint8_t)lower] == upper, "upper of %c", lower);
  }
  for(uint16_t value = 0; value < 256; value++) {
    uint8_t lower = (uint8_t)LowerCase[value];
    uint8_t upper = (uint8_t)UpperCase[value];
    TEST_Check(lower == value || (uint8_t)UpperCase[lower] == value, "lower of %u", value);
    TEST_Check(upper == value || (uint8_t)LowerCase[upper] == value, "upper of %u", value);
  }
}

static void unit_suffixes_parse(void)
{
  static const struct {
    const char *text;
    float value;
  } vectors[] = {
    { "5meg", 5e6f }, { "2.5meg", 2.5e6f }, { "5m", 5e-3f },
    { "5e6", 5e6f }, { "1.5e-3", 1.5e-3f }, { "-2E3", -2e3f },
    { "5k", 5e3f }, { "5g", 5e9f }, { "5t", 5e12f },
    { "5u", 5e-6f }, { "50%", 0.5f }, { "7", 7.0f }
  };
  for(uint16_t i = 0; i < array_len(vectors); i++) {
    TEST_Check(str_is_uf32(vectors[i].text), "%s is a number", vectors[i].text);
    float value = vectors[i].value;
    TEST_Near(str_to_uf32(vectors[i].text), value, fabsf(value) * 1e-5f, vectors[i].text);
  }
  TEST_Check(!str_is_uf32(NULL));
  TEST_Check(!str_is_uf32(""));
}

static void heap_strings(void)
{
  char *wide = str_from_int(7, 10, true, 0, 200);
  TEST_Check(wide && strlen(wide) == 200, "number padded to 200");
  TEST_Equal(str_join(1, "abc"), "abc", "one part joined");
  TEST_Equal(str_join(3, "a", "b", "c"), "abc", "three parts joined");
  heap_clear();
}

static void integers_split(void)
{
  uint32_t part[4];
  TEST_Check(str_split_int("2.3.2", '.', 10, part, 4) == 3 && part[0] == 2 && part[2] == 2);
  TEST_Check(str_split_int("10-20", '-', 10, part, 2) == 2 && part[1] == 20);
  TEST_Check(str_split_int("7", '.', 10, part, 4) == 1 && part[0] == 7);
  TEST_Check(str_split_int("1..2", '.', 10, part, 4) == 0);
  TEST_Check(str_split_int("1.a", '.', 10, part, 4) == 0);
  TEST_Check(str_split_int("1.2.3", '.', 10, part, 2) == 0);
  TEST_Check(str_split_int("", '.', 10, part, 4) == 0);
  TEST_Check(str_split_int("4294967296", '.', 10, part, 1) == 0);
  TEST_Check(str_split_int("00A0-000f", '-', 16, part, 2) == 2 && part[0] == 0xA0 &&
    part[1] == 0x0F);
  TEST_Check(str_split_int("1-a", '-', 10, part, 2) == 0, "a digit the base does not have");
  // Padding is what the print adds, not what the input owes: any width parses
  TEST_Check(str_split_int("0-1", '-', 16, part, 2) == 2 && part[0] == 0 && part[1] == 1);
  TEST_Check(str_split_int("f-FFFF", '-', 16, part, 2) == 2 && part[0] == 0xF &&
    part[1] == 0xFFFF);
}

static void integers_print_back(void)
{
  char text[24];
  char *end = str_append_int(text, 3, 10, 0);
  *end++ = '.';
  end = str_append_int(end, 18, 10, 0);
  *end = 0;
  TEST_Equal(text, "3.18", "two numbers and a dot");
  end = str_append_int(text, 0, 10, 0);
  *end = 0;
  TEST_Equal(text, "0", "zero");
  end = str_append_int(text, -405, 10, 0);
  *end = 0;
  TEST_Equal(text, "-405", "negative");
  end = str_append_int(text, 0x2F, 16, 4);
  *end++ = '-';
  end = str_append_int(end, 1, 16, 4);
  *end = 0;
  TEST_Equal(text, "002F-0001", "hex padded to 4");
  // What the library prints, it parses back: the same bases on both sides
  uint32_t part[2];
  end = str_append_int(text, 5, 2, 8);
  *end = 0;
  TEST_Equal(text, "00000101", "binary padded to 8");
  TEST_Check(str_split_int(text, '-', 2, part, 2) == 1 && part[0] == 5);
  end = str_append_int(text, 35 * 36 + 35, 36, 0);
  *end = 0;
  TEST_Equal(text, "ZZ", "base 36");
  TEST_Check(str_split_int(text, '-', 36, part, 2) == 1 && part[0] == 35 * 36 + 35);
}

void TEST_Cases(void)
{
  TEST(case_tables_map_letters);
  TEST(unit_suffixes_parse);
  TEST(heap_strings);
  TEST(integers_split);
  TEST(integers_print_back);
}

// .tests/test_log.c
// Log gate: a level under `LOG_LEVEL` folds the call away, arguments included,
// a level at or above it prints the line once, critical and panic ones through the port.
// Format of `print`: flags, widths and precisions as in `printf`, the extensions it lists,
// arrays of every element type, and a specifier it lacks echoed without taking an argument.

#include <stdio.h>

#include "test.h"
#include "log.c"

static uint32_t evaluated;

static uint32_t evaluate(void) { return ++evaluated; }

static void silenced_level_skips_arguments(void)
{
  evaluated = 0;
  LOG_Debug("debug %u", evaluate());
  TEST_Equal(evaluated, 0, "arguments of a silenced level never run");
  LOG_Info("info %u", evaluate());
  TEST_Equal(evaluated, 1, "arguments of a printed level run once");
  TEST_Logged("info 1");
}

// Critical and panic lines skip the console thread and go straight to the port
static void critical_and_panic_reach_the_port(void)
{
  LOG_Critical("critical %u", 7);
  TEST_Check(!DbgFile->size, "critical sent before the call returns");
  TEST_Logged("critical 7");
  LOG_Panic("panic line");
  TEST_Check(!DbgFile->size, "panic sent before the call returns");
  TEST_Logged("panic line");
}

//------------------------------------------------------------------------------------------ Format

static uint8_t capture_memory[128];
static MBB_t capture = {
  .name = "capture", .buffer = capture_memory, .limit = sizeof(capture_memory) - 1
};

static void capture_begin(void)
{
  MBB_Clear(&capture);
  DBG_SetFile(&capture);
}

// Text written since `capture_begin`, the console file back in place
static const char *capture_end(void)
{
  DBG_DefaultFile();
  capture_memory[capture.size] = 0;
  return (const char *)capture_memory;
}

// Text one `print` call writes
#define printed(...) (capture_begin(), print(__VA_ARGS__), capture_end())

// Width pads with spaces, the `0` flag with zeros behind the sign, a precision sets the digits
static void integers_follow_printf(void)
{
  TEST_Equal(printed("%d|%i|%u", -7, 42, 4000000000u), "-7|42|4000000000", "plain");
  TEST_Equal(printed("%5d", -42), "  -42", "width");
  TEST_Equal(printed("%05d", -42), "-0042", "zero flag");
  TEST_Equal(printed("%.3d", 7), "007", "precision");
  TEST_Equal(printed("%6.3d", 7), "   007", "width and precision");
  TEST_Equal(printed("%X|%04X", 0xBEEFu, 0xAu), "BEEF|000A", "hexadecimal");
  TEST_Equal(printed("%b|%08b", 5, 5), "101|00000101", "binary");
  TEST_Equal(printed("%lld|%llu", INT64_MIN, UINT64_MAX),
    "-9223372036854775808|18446744073709551615", "64-bit");
  TEST_Equal(printed("%.3d", -7), "-007", "precision counts the digits, the sign comes on top");
}

// `%f` takes three places, `%F` two, a precision its own;
// width pads with spaces, the `0` flag with zeros, as for an integer.
static void floats_take_width_and_precision(void)
{
  TEST_Equal(printed("%f|%F|%.1f", 3.14159, 3.14159, 3.14159), "3.142|3.14|3.1", "places");
  TEST_Equal(printed("%7.2f", -3.14159), "  -3.14", "width");
  TEST_Equal(printed("%5.0f", 5.0), "    5", "width, no places");
  TEST_Equal(printed("%07.2f", -3.14159), "-003.14", "zero flag");
}

// Characters, strings, booleans, a table entry and a pointer
static void single_values(void)
{
  static const char *const words[] = { "on", "off" };
  TEST_Equal(printed("%c%s %B %S", 'x', "yz", true, 1, words), "xyz true off", "values");
  char pointer[24];
  snprintf(pointer, sizeof(pointer), "0x%0*X", (int)(2 * sizeof(void *)), 0x1234);
  TEST_Equal(printed("%p", (void *)0x1234), pointer, "pointer, every digit it holds");
}

typedef struct {
  int8_t x, y;
} point_t;

static int32_t point_print(void *object)
{
  const point_t *point = object;
  return DBG_Char('(') + DBG_Dec(point->x) + DBG_Char(',') + DBG_Dec(point->y) + DBG_Char(')');
}

// `%a` takes a count and the element width,
// the text up to the next conversion separates the elements, and that conversion reads them:
// integers of any width, floats, characters, strings, booleans, table entries and objects.
static void arrays_print_every_element(void)
{
  static const int16_t shorts[] = { -1, 300, 7 };
  TEST_Equal(printed("[%2a, %d]", 3, shorts), "[-1, 300, 7]", "16-bit integers");
  static const uint8_t bytes[] = { 0x0A, 0xFF };
  TEST_Equal(printed("%a %02X", 2, bytes), "0A FF", "bytes in hexadecimal");
  static const uint64_t longs[] = { 1, UINT64_MAX };
  TEST_Equal(printed("%8a;%u", 2, longs), "1;18446744073709551615", "64-bit integers");
  static const float floats[] = { 1.5f, -2.25f };
  TEST_Equal(printed("%a %.2f", 2, floats), "1.50 -2.25", "floats");
  TEST_Equal(printed("%a.%c", 3, "abc"), "a.b.c", "characters");
  static const char *const words[] = { "on", "off" };
  TEST_Equal(printed("%a/%s", 2, words), "on/off", "strings");
  static const bool flags[] = { true, false };
  TEST_Equal(printed("%a,%B", 2, flags), "true,false", "booleans");
  static const uint8_t picks[] = { 1, 0, 1 };
  TEST_Equal(printed("%a %S", 3, picks, words), "off on off", "table entries");
  static point_t points[] = { { 1, 2 }, { -3, 4 } };
  TEST_Equal(printed("%a %o", 2, points, point_print, sizeof(point_t)), "(1,2) (-3,4)",
    "objects");
}

// An array of no elements prints none of them, and no separator
static void empty_array_prints_nothing(void)
{
  static const int16_t shorts[] = { 1 };
  TEST_Equal(printed("[%2a, %d]", 0, shorts), "[]", "no elements");
}

// `%%`, the time and a specifier the formatter lacks take no argument,
// so the array waits for the conversion after them.
static void array_waits_for_a_conversion_with_argument(void)
{
  static const uint8_t pair[] = { 10, 20 };
  TEST_Equal(printed("%a%%, %u", 2, pair), "%10, 20", "`%%` before the conversion");
}

// A specifier the formatter lacks comes out as written and takes no argument
static void unknown_specifier_echoes(void)
{
  TEST_Equal(printed("%q%d", 5), "%q5", "unknown specifier");
  TEST_Equal(printed("100%%"), "100%", "escaped percent");
  TEST_Equal(printed("100%"), "100%", "percent at the end");
}

//-------------------------------------------------------------------------------------------------

void TEST_Cases(void)
{
  TEST(silenced_level_skips_arguments);
  TEST(critical_and_panic_reach_the_port);
  TEST(integers_follow_printf);
  TEST(floats_take_width_and_precision);
  TEST(single_values);
  TEST(arrays_print_every_element);
  TEST(empty_array_prints_nothing);
  TEST(array_waits_for_a_conversion_with_argument);
  TEST(unknown_specifier_echoes);
}

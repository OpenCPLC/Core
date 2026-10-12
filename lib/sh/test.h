// lib/sh/test.h

#ifndef TEST_H_
#define TEST_H_

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef TEST_LOG_SIZE
  // Firmware output kept for `TEST_Logged`, the newest bytes win [B]
  #define TEST_LOG_SIZE 4096
#endif

//--------------------------------------------------------------------------------------------- API

// Cases of a test program in run order, one `TEST(name)` each; the program defines it
void TEST_Cases(void);

/**
 * @brief Run cases and print the count line, `N OK` or `N OK, M ERR`.
 *   A test program runs `TEST_Cases` through it, a firmware any case list on the chip,
 *   where the lines of a case reach the console before the next case runs.
 * @param[in] Cases Case list, `TEST(name)` each
 * @return Misses, `0` when every check held
 */
uint32_t TEST_Suite(void (*Cases)(void));

// Run case `name`, a `void name(void)`; a miss ends it and the next case runs
#define TEST(name) _TEST_Case(name, #name)

/**
 * @brief Verdict: a miss prints the message with the condition, or the condition alone,
 *   and ends the case.
 * @param[in] cond Condition that has to hold
 * @param[in] ... Optional message: a format literal and its arguments, read by `printf`
 *   on the computer and by `print` on the chip, so `%d %u %x %s %c` mean the same on both
 */
#define TEST_Check(cond, ...) \
  _TEST_Check((cond), __FILE__, __LINE__, #cond, "" __VA_ARGS__)

/**
 * @brief Equality, a miss prints `actual` as written, its value and the expected one,
 *   in the format of its type.
 * @param[in] actual Value under test: integer, `bool`, `float`, `double` or string
 * @param[in] expected Value it has to equal
 * @param[in] msg What the check means
 */
#define TEST_Equal(actual, expected, msg) _Generic((actual), \
  bool: _TEST_EqualUint, \
  char: _TEST_EqualInt, signed char: _TEST_EqualInt, short: _TEST_EqualInt, \
  int: _TEST_EqualInt, long: _TEST_EqualInt, long long: _TEST_EqualInt, \
  unsigned char: _TEST_EqualUint, unsigned short: _TEST_EqualUint, \
  unsigned int: _TEST_EqualUint, unsigned long: _TEST_EqualUint, \
  unsigned long long: _TEST_EqualUint, \
  float: _TEST_EqualFloat, double: _TEST_EqualFloat, \
  char *: _TEST_EqualString, const char *: _TEST_EqualString \
)((actual), (expected), __FILE__, __LINE__, #actual, msg)

/**
 * @brief Value within `tolerance` of `expected`, a miss prints `actual` as written,
 *   its value, the expected one and the tolerance.
 * @param[in] actual Value under test
 * @param[in] expected Value it has to come close to
 * @param[in] tolerance Largest gap that still passes
 * @param[in] msg What the check means
 */
#define TEST_Near(actual, expected, tolerance, msg) \
  _TEST_Near((actual), (expected), (tolerance), __FILE__, __LINE__, #actual, msg)

/**
 * @brief Equal bytes, a miss prints the first byte that differs from both sides.
 * @param[in] actual Bytes under test
 * @param[in] expected Bytes they have to equal
 * @param[in] len Number of bytes
 * @param[in] msg What the check means
 */
#define TEST_Bytes(actual, expected, len, msg) \
  _TEST_Bytes((actual), (expected), (len), __FILE__, __LINE__, #actual, msg)

//-------------------------------------------------------------------------------------------- Host
#if defined(HOST_TEST)

// Firmware logged a line holding `text` since the case began or since the previous `TEST_Logged`
#define TEST_Logged(text) _TEST_Logged((text), __FILE__, __LINE__)

// `ms` steps of virtual time, each a tick forward and every task due by then
void TEST_Run(uint32_t ms);

// One step of virtual time; `let` of a test build takes it, inside a task the tick alone
void _TEST_Step(void);

// Bytes the UART `port` sends in a test build: the console port joins the firmware output
void _TEST_Output(const void *port, const uint8_t *data, uint32_t size);

void _TEST_Logged(const char *text, const char *file, uint32_t line);

#endif
//---------------------------------------------------------------------------------------- Internal

void _TEST_Case(void (*Case)(void), const char *name);
void _TEST_Check(bool cond, const char *file, uint32_t line, const char *text,
  const char *format, ...);
void _TEST_EqualInt(int64_t actual, int64_t expected,
  const char *file, uint32_t line, const char *text, const char *msg);
void _TEST_EqualUint(uint64_t actual, uint64_t expected,
  const char *file, uint32_t line, const char *text, const char *msg);
void _TEST_EqualFloat(double actual, double expected,
  const char *file, uint32_t line, const char *text, const char *msg);
void _TEST_EqualString(const char *actual, const char *expected,
  const char *file, uint32_t line, const char *text, const char *msg);
void _TEST_Near(double actual, double expected, double tolerance,
  const char *file, uint32_t line, const char *text, const char *msg);
void _TEST_Bytes(const void *actual, const void *expected, uint32_t len,
  const char *file, uint32_t line, const char *text, const char *msg);

//-------------------------------------------------------------------------------------------------
#endif

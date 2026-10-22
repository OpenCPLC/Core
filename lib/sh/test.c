// lib/sh/test.c

#include "test.h"

#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#if defined(HOST_TEST)
  #include <ctype.h>
  #include <stdio.h>
  #include <stdlib.h>
#endif

#include "dbg.h"
#include "log.h"
#if defined(HOST_TEST)
  #include "heap.h"
  #include "task.h"
  #include "vrts.h"
#endif

//------------------------------------------------------------------------------------------ Config

// Float in a verdict: the console formatter of the chip has no `%g`
#if defined(HOST_TEST)
  #define TEST_REAL "%.9g"
#else
  #define TEST_REAL "%.6f"
#endif

//----------------------------------------------------------------------------------------- Globals

static jmp_buf case_end;       // a miss jumps here, out of the case
static const char *case_name;  // `NULL` outside a case
static uint32_t ok_count;
static uint32_t err_count;
#if defined(HOST_TEST)
static bool finished;          // every case ran, so the exit belongs to the program
static bool in_task;           // a step inside a task moves the clock and runs nothing
static bool line_open;         // firmware output stopped mid-line, a verdict starts a new one
static bool in_code;           // inside a color code of the firmware output, which is not kept
static char logs[TEST_LOG_SIZE];
static uint32_t logs_size;
#endif

//------------------------------------------------------------------------------------------ Output
#if defined(HOST_TEST)

// Verdicts on standard output, apart from the firmware output they follow,
// each on a line of its own
static void vsay(const char *format, va_list args)
{
  if(line_open) putchar('\n');
  line_open = false;
  vprintf(format, args);
}
static void say_end(void) { putchar('\n'); }
static void flush(void) {} // standard output runs unbuffered, see `main`

#else

// Verdicts on the console, among the firmware lines of their cases
static void vsay(const char *format, va_list args) { vprint(format, args); }
static void say_end(void) { DBG_Enter(); }

// Cases may run in the console thread, which sends its buffer only between commands
static void flush(void) { DBG_Flush(); }

#endif

static void say(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vsay(format, args);
  va_end(args);
}

static void say_line(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vsay(format, args);
  va_end(args);
  say_end();
}

//-------------------------------------------------------------------------------------------- Logs
#if defined(HOST_TEST)

// Firmware output, indented so its own `ERR` lines never read as verdicts.
// Lines end in `\n` alone: a text stream adds its own `\r` where the system wants one.
static void logs_echo(const uint8_t *data, uint32_t size)
{
  for(uint32_t i = 0; i < size; i++) {
    if(data[i] == '\r') continue;
    if(!line_open) fputs("  ", stdout);
    putchar(data[i]);
    line_open = data[i] != '\n';
  }
}

// Firmware output: shown, and its newest text kept for `TEST_Logged` the way a console
// shows it, without color codes. A full store drops its older half.
static void logs_take(const uint8_t *data, uint32_t size)
{
  logs_echo(data, size);
  for(uint32_t i = 0; i < size; i++) {
    if(data[i] == 0x1B) in_code = true;
    if(in_code) {
      in_code = !isalpha(data[i]);
      continue;
    }
    if(logs_size == TEST_LOG_SIZE) {
      logs_size = TEST_LOG_SIZE / 2;
      memmove(logs, logs + logs_size, logs_size);
    }
    logs[logs_size++] = (char)data[i];
  }
}

void _TEST_Output(const void *port, const uint8_t *data, uint32_t size)
{
  if(port != DbgUart) return; // frames of another port stay in its module, for the test to read
  logs_take(data, size);
}

// Firmware output since the last pass; no `DBG_Loop` runs in a test to empty `DbgFile`
static void logs_drain(void)
{
  if(!DbgFile->size) return;
  logs_take(DbgFile->buffer, DbgFile->size);
  MBB_Clear(DbgFile);
}

static bool logs_hold(const char *text)
{
  uint32_t len = strlen(text);
  if(len > logs_size) return false;
  for(uint32_t i = 0; i + len <= logs_size; i++) {
    if(!memcmp(logs + i, text, len)) return true;
  }
  return false;
}

#endif
//-------------------------------------------------------------------------------------------- Miss

// `ERR` of a miss up to its message, after the firmware output that led to it
static void miss_start(const char *file, uint32_t line)
{
  #if defined(HOST_TEST)
  logs_drain();
  #endif
  err_count++;
  say(ANSI_RED "ERR" ANSI_END " %s:%u %s: ", file, (unsigned)line, case_name ? case_name : "-");
}

static void miss(const char *file, uint32_t line, const char *msg)
{
  miss_start(file, line);
  say_line("%s", msg);
}

// Out of the case, the next one runs; outside any case the program goes on
static void miss_end(void)
{
  if(case_name) longjmp(case_end, 1);
}

//-------------------------------------------------------------------------------------------- Case

void _TEST_Case(void (*Case)(void), const char *name)
{
  #if defined(HOST_TEST)
  logs_drain();
  logs_size = 0;
  #endif
  case_name = name;
  uint32_t err_before = err_count;
  if(!setjmp(case_end)) Case();
  #if defined(HOST_TEST)
  in_task = false; // a miss inside a task leaves the flag set
  TASK_ClearAll(); // tasks of this case never run in the next one
  logs_drain();
  #endif
  if(err_count == err_before) say_line(ANSI_GREEN "OK" ANSI_END " %s", name);
  case_name = NULL;
  flush();
}

//---------------------------------------------------------------------------------------- Verdicts

// A check that held counts, one that did not is left to its verdict
static bool held(bool cond)
{
  if(cond) ok_count++;
  return cond;
}

void _TEST_Check(bool cond, const char *file, uint32_t line, const char *text,
  const char *format, ...)
{
  if(held(cond)) return;
  if(*format) {
    miss_start(file, line);
    va_list args;
    va_start(args, format);
    vsay(format, args);
    va_end(args);
    say_end();
    say_line("  %s is false", text);
  }
  else miss(file, line, text);
  miss_end();
}

void _TEST_EqualInt(int64_t actual, int64_t expected,
  const char *file, uint32_t line, const char *text, const char *msg)
{
  if(held(actual == expected)) return;
  miss(file, line, msg);
  say_line("  %s is %" PRId64 ", expected %" PRId64, text, actual, expected);
  miss_end();
}

void _TEST_EqualUint(uint64_t actual, uint64_t expected,
  const char *file, uint32_t line, const char *text, const char *msg)
{
  if(held(actual == expected)) return;
  miss(file, line, msg);
  say_line("  %s is %" PRIu64 ", expected %" PRIu64, text, actual, expected);
  miss_end();
}

void _TEST_EqualFloat(double actual, double expected,
  const char *file, uint32_t line, const char *text, const char *msg)
{
  if(held(actual == expected)) return;
  miss(file, line, msg);
  say_line("  %s is " TEST_REAL ", expected " TEST_REAL, text, actual, expected);
  miss_end();
}

void _TEST_EqualString(const char *actual, const char *expected,
  const char *file, uint32_t line, const char *text, const char *msg)
{
  bool same = (actual && expected) ? !strcmp(actual, expected) : actual == expected;
  if(held(same)) return;
  miss(file, line, msg);
  say_line("  %s is \"%s\", expected \"%s\"",
    text, actual ? actual : "(null)", expected ? expected : "(null)");
  miss_end();
}

void _TEST_Near(double actual, double expected, double tolerance,
  const char *file, uint32_t line, const char *text, const char *msg)
{
  if(held(fabs(actual - expected) <= tolerance)) return;
  miss(file, line, msg);
  say_line("  %s is " TEST_REAL ", expected " TEST_REAL " within " TEST_REAL,
    text, actual, expected, tolerance);
  miss_end();
}

void _TEST_Bytes(const void *actual, const void *expected, uint32_t len,
  const char *file, uint32_t line, const char *text, const char *msg)
{
  const uint8_t *got = actual, *want = expected;
  uint32_t i = 0;
  while(i < len && got[i] == want[i]) i++;
  if(held(i == len)) return;
  miss(file, line, msg);
  say_line("  %s: byte %u of %u is 0x%02X, expected 0x%02X",
    text, (unsigned)i, (unsigned)len, got[i], want[i]);
  miss_end();
}

#if defined(HOST_TEST)

void _TEST_Logged(const char *text, const char *file, uint32_t line)
{
  logs_drain();
  bool found = logs_hold(text);
  logs_size = 0; // the next check of logs looks past this point
  if(held(found)) return;
  miss_start(file, line);
  say_line("no log line holds \"%s\"", text);
  miss_end();
}

#endif
//------------------------------------------------------------------------------------------- Suite

// Count line closing a suite, the one `make test` reads
static void report(void)
{
  if(err_count) say_line("%u OK, %u ERR", (unsigned)ok_count, (unsigned)err_count);
  else say_line("%u OK", (unsigned)ok_count);
}

uint32_t TEST_Suite(void (*Cases)(void))
{
  ok_count = 0;
  err_count = 0;
  Cases();
  report();
  flush();
  return err_count;
}

//-------------------------------------------------------------------------------------------- Time
#if defined(HOST_TEST)

void _TEST_Step(void)
{
  VrtsTicker++;
  if(!in_task) {
    // A task waiting inside its handler holds the task thread, as on the chip.
    // As many runs as tasks pending: one queued again at once cannot hold the tick for good.
    in_task = true;
    for(uint16_t n = TASK_Pending(); n && TASK_Step(); n--);
    in_task = false;
  }
  logs_drain();
}

void TEST_Run(uint32_t ms)
{
  for(uint32_t i = 0; i < ms; i++) _TEST_Step();
}

#endif
//-------------------------------------------------------------------------------------------- Main
#if defined(HOST_TEST)

// An exit before the last case is a failure, `exit(0)` of a firmware reset included
static void early_exit(void)
{
  if(finished) return;
  logs_drain();
  err_count++;
  say_line(ANSI_RED "ERR" ANSI_END " %s: program ended before its cases did",
    case_name ? case_name : "-");
  report();
  fflush(stdout);
  _Exit(1);
}

int main(void)
{
  setvbuf(stdout, NULL, _IONBF, 0); // a crash keeps everything printed so far
  atexit(early_exit);
  heap_init(); // as `sys_init` on the chip, before anything allocates
  // Critical and panic lines go straight to the port, whose bytes join the logs here
  static UART_t port = { .UART_115200 };
  DBG_Init(&port);
  uint32_t misses = TEST_Suite(TEST_Cases);
  finished = true;
  return misses ? 1 : 0;
}

#endif
//-------------------------------------------------------------------------------------------------

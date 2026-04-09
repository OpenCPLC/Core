// lib/sh/log.h

#ifndef LOG_H_
#define LOG_H_

#include "dbg.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

#ifndef LOG_ARYSPACE_MAXLEN
  // Max length of inline separator text between array elements (`%a` formatter)
  #define LOG_ARYSPACE_MAXLEN 32
#endif

#define LOG_LEVEL_DBG 0
#define LOG_LEVEL_INF 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_ERR 3
#define LOG_LEVEL_CRT 4
#define LOG_LEVEL_PNC 5
#define LOG_LEVEL_NIL 6

#ifndef LOG_LEVEL
  // Compile-time log threshold. Messages below this level are stripped at compile.
  #define LOG_LEVEL LOG_LEVEL_INF
#endif

typedef enum {
  LOG_Level_Debug = LOG_LEVEL_DBG,
  LOG_Level_Info = LOG_LEVEL_INF,
  LOG_Level_Warning = LOG_LEVEL_WRN,
  LOG_Level_Error = LOG_LEVEL_ERR,
  LOG_Level_Critical = LOG_LEVEL_CRT,
  LOG_Level_Panic = LOG_LEVEL_PNC,
  LOG_Level_None = LOG_LEVEL_NIL
} LOG_Level_t;

extern bool LogPrintFlag;

/**
 * @brief Print formatted string to debug output.
 * Format: `%[flags][width][.precision][length]conversion`
 * Flags:
 *   `0`: Zero-pad numeric output (default: space-pad)
 *   `-`: Left-align (parsed, not yet honored)
 * Standard conversions (printf-compatible):
 *   `%d` `%i` `%u` Integer (use `l`/`ll` for 64-bit)
 *   `%x` `%X`      Hexadecimal
 *   `%f` `%F`      Float (default precision 3 / 2)
 *   `%c` `%s`      Char / string
 *   `%p`           Pointer as `0x` + hex digits (8 on 32-bit, 16 on 64-bit)
 *   `%%`           Literal `%`
 * Extensions:
 *   `%b`           Binary (C23 standard)
 *   `%B`           Bool as `true`/`false`
 *   `%t`           Current RTC time `HH:MM:SS` (no arg consumed)
 *   `%lt`          Current RTC time with ms `HH:MM:SS.mmm`
 *   `%T`           Current RTC datetime `YYYY-MM-DD HH:MM:SS`
 *   `%lT`          Current RTC datetime with ms
 *   `%S`           String from indexed table: `LOG_Info("%S", idx, table)`
 *   `%o` `%O`      Object via callback (NOT octal): `LOG_Info("%o", &obj, &fn)`
 *   `%a` `%A`      Array prefix (NOT hex float). Element size from width
 *                  (1/2/4/8 B), separator from text after `%a`:
 *                  `LOG_Info("%4a, %u", count, ptr)`
 * Width/precision (printf semantics):
 *   `%5d`          5-char field, space-padded   (e.g. `"   42"`)
 *   `%05d`         5-char field, zero-padded    (e.g. `"00042"`)
 *   `%.3d`        Minimum 3 digits, zero-pad   (e.g. `"042"`)
 * Logs do NOT include automatic timestamp prefix. Use `%t` or `%T` explicitly
 * when timing is needed. When RTC is not initialized, `%t`/`%T` fall back to
 * the current tick value.
 * Unknown specifiers are echoed raw (e.g. `%q` -> `%q`) so typos are visible.
 * @param[in] template Format string
 * @param[in] ... Format arguments
 */
void print(const char *template, ...);

void LOG_Nope(const char *message, ...);     // No-op log (for disabled levels)
void LOG_Bash(const char *message, ...);     // Bash response (always visible)
void LOG_Debug(const char *message, ...);    // Log debug message (DBG)
void LOG_Info(const char *message, ...);     // Log info message (INF)
void LOG_Warning(const char *message, ...);  // Log warning message (WRN)
void LOG_Error(const char *message, ...);    // Log error message (ERR)
void LOG_Critical(const char *message, ...); // Log critical message and flush (CRT)
void LOG_Panic(const char *message);         // Log panic message and flush blocking (PNC)

/**
 * @brief Log message with specified level.
 * @param[in] lvl Log level
 * @param[in] message Format string
 * @param[in] ... Format arguments
 */
void LOG_Message(LOG_Level_t lvl, char *message, ...);

#define LOG_NOP LOG_Nope      // No-op log (for disabled levels)
#define LOG_DBG LOG_Debug     // Log debug message
#define LOG_INF LOG_Info      // Log info message
#define LOG_WRN LOG_Warning   // Log warning message
#define LOG_ERR LOG_Error     // Log error message
#define LOG_CRT LOG_Critical  // Log critical message and flush
#define LOG_PNC LOG_Panic     // Log panic message and flush blocking
#define LOG_MSG LOG_Message   // Log message with specified level

// Library name tag (cream brackets, append at end of message)
#define LOG_LIB(name) " " ANSI_GREY "[" ANSI_CREAM name ANSI_GREY "]" ANSI_END
// Debug log with library tag
#define LOG_LIB_DBG(name, fmt, ...) LOG_DBG(fmt LOG_LIB(name), ##__VA_ARGS__)
// Info log with library tag
#define LOG_LIB_INF(name, fmt, ...) LOG_INF(fmt LOG_LIB(name), ##__VA_ARGS__)
// Warning log with library tag
#define LOG_LIB_WRN(name, fmt, ...) LOG_WRN(fmt LOG_LIB(name), ##__VA_ARGS__)
// Error log with library tag
#define LOG_LIB_ERR(name, fmt, ...) LOG_ERR(fmt LOG_LIB(name), ##__VA_ARGS__)
// Critical log with library tag
#define LOG_LIB_CRT(name, fmt, ...) LOG_CRT(fmt LOG_LIB(name), ##__VA_ARGS__)

/**
 * @brief Log parse error.
 * @param[in] value Value that failed to parse
 * @param[in] type Expected type name
 */
void LOG_ErrorParse(const char *value, const char *type);

#endif
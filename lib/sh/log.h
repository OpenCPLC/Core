// lib/sh/log.h

#ifndef LOG_H_
#define LOG_H_

#include "dbg.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef LOG_ARYSPACE_MAXLEN
  // Longest separator text between array elements of the `%a` formatter [B]
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
  // Compile-time threshold, messages below it are stripped from the build
  #define LOG_LEVEL LOG_LEVEL_INF
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  LOG_Level_Debug = LOG_LEVEL_DBG,
  LOG_Level_Info = LOG_LEVEL_INF,
  LOG_Level_Warning = LOG_LEVEL_WRN,
  LOG_Level_Error = LOG_LEVEL_ERR,
  LOG_Level_Critical = LOG_LEVEL_CRT,
  LOG_Level_Panic = LOG_LEVEL_PNC,
  LOG_Level_None = LOG_LEVEL_NIL
} LOG_Level_t;

// Levels below critical stay silent while clear, the console drops it while a line is typed
extern bool LogPrintFlag;

//------------------------------------------------------------------------------------------- Print

/**
 * @brief Print formatted text to the debug output.
 * Format: `%[flags][width][.precision][length]conversion`
 * Flags:
 *   `0`: Zero-pad numeric output (default: space-pad)
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
 *   `%.3d`         Minimum 3 digits, zero-pad   (e.g. `"042"`)
 * Logs do NOT include an automatic timestamp prefix.
 * Use `%t` or `%T` explicitly when timing is needed.
 * When RTC is not initialized, `%t`/`%T` fall back to the current tick value.
 * Unknown specifiers are echoed raw (e.g. `%q` -> `%q`) so typos are visible.
 * @param[in] template Format string
 * @param[in] ... Format arguments
 */
void print(const char *template, ...);

//--------------------------------------------------------------------------------------------- Log

// One line per call: colored level tag, formatted message, line break.
// The functions sit behind the macros below, which are the API.
void _LOG_Debug(const char *message, ...);
void _LOG_Info(const char *message, ...);
void _LOG_Warning(const char *message, ...);
void _LOG_Error(const char *message, ...);
void _LOG_Critical(const char *message, ...);  // flushed to the port before returning
void _LOG_Panic(const char *message);          // plain text, flushed blocking
void _LOG_Message(LOG_Level_t lvl, const char *message, ...);
void LOG_Bash(const char *message, ...);       // shell response, always printed
void LOG_Nope(const char *message, ...);       // no-op with a body, for a pointer to hold

// Level gate: at or above `LOG_LEVEL` the call stands;
// under it the whole expression folds away, message and arguments included.
// `LOG_NOP` is that nothing on its own, for a module's debug macro to point at.
#define LOG_Gate(lvl, fn, ...) ((lvl) >= LOG_LEVEL ? fn(__VA_ARGS__) : (void)0)
#define LOG_NOP(...) ((void)0)

#define LOG_Debug(...) LOG_Gate(LOG_LEVEL_DBG, _LOG_Debug, __VA_ARGS__)
#define LOG_Info(...) LOG_Gate(LOG_LEVEL_INF, _LOG_Info, __VA_ARGS__)
#define LOG_Warning(...) LOG_Gate(LOG_LEVEL_WRN, _LOG_Warning, __VA_ARGS__)
#define LOG_Error(...) LOG_Gate(LOG_LEVEL_ERR, _LOG_Error, __VA_ARGS__)
#define LOG_Critical(...) LOG_Gate(LOG_LEVEL_CRT, _LOG_Critical, __VA_ARGS__)
#define LOG_Panic(message) LOG_Gate(LOG_LEVEL_PNC, _LOG_Panic, message)

/**
 * @brief Log with the level chosen at runtime, gated like panic at compile time.
 * @param[in] lvl Log level
 * @param[in] message Format string
 * @param[in] ... Format arguments
 */
#define LOG_Message(lvl, ...) LOG_Gate(LOG_LEVEL_PNC, _LOG_Message, lvl, __VA_ARGS__)

#define LOG_DBG LOG_Debug
#define LOG_INF LOG_Info
#define LOG_WRN LOG_Warning
#define LOG_ERR LOG_Error
#define LOG_CRT LOG_Critical
#define LOG_PNC LOG_Panic
#define LOG_MSG LOG_Message

// Aside in grey: format text, not a value; brings its own space, args follow the message
#define LOG_NOTE(fmt) ANSI_GREY " (" fmt ")" ANSI_END
// Context tag: a module, command or any short phrase. Caller adds the spacing
#define LOG_TAG(name) ANSI_GREY "[" ANSI_CREAM name ANSI_GREY "]" ANSI_END
// Log with the context tag appended
#define LOG_TAG_DBG(name, fmt, ...) LOG_DBG(fmt " " LOG_TAG(name), ##__VA_ARGS__)
#define LOG_TAG_INF(name, fmt, ...) LOG_INF(fmt " " LOG_TAG(name), ##__VA_ARGS__)
#define LOG_TAG_WRN(name, fmt, ...) LOG_WRN(fmt " " LOG_TAG(name), ##__VA_ARGS__)
#define LOG_TAG_ERR(name, fmt, ...) LOG_ERR(fmt " " LOG_TAG(name), ##__VA_ARGS__)
#define LOG_TAG_CRT(name, fmt, ...) LOG_CRT(fmt " " LOG_TAG(name), ##__VA_ARGS__)

/**
 * @brief Error line for a value that failed to parse.
 * @param[in] value Text that failed
 * @param[in] type Expected type name
 */
void LOG_ErrorParse(const char *value, const char *type);

//-------------------------------------------------------------------------------------------------
#endif

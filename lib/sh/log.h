/** @file lib/sh/log.h */

#ifndef LOG_H_
#define LOG_H_

#include "dbg.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef LOG_ARYSPACE_MAXLEN
  #define LOG_ARYSPACE_MAXLEN 32
#endif

#ifndef LOG_INCLUDE_MS
  #define LOG_INCLUDE_MS OFF
#endif

#ifndef LOG_TIME_ONLY
  #define LOG_TIME_ONLY OFF
#endif

#define LOG_LIB(name) ANSI_GREY " <" ANSI_CREAM name ANSI_GREY ">" ANSI_END

#define LOG_LEVEL_DBG 0
#define LOG_LEVEL_INF 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_ERR 3
#define LOG_LEVEL_CRT 4
#define LOG_LEVEL_PAC 5
#define LOG_LEVEL_NIL 6

#ifndef LOG_LEVEL
  #define LOG_LEVEL LOG_LEVEL_INF
#endif

typedef enum {
  LOG_Level_Debug = LOG_LEVEL_DBG,
  LOG_Level_Info = LOG_LEVEL_INF,
  LOG_Level_Warning = LOG_LEVEL_WRN,
  LOG_Level_Error = LOG_LEVEL_ERR,
  LOG_Level_Critical = LOG_LEVEL_CRT,
  LOG_Level_Panic = LOG_LEVEL_PAC,
  LOG_Level_None = LOG_LEVEL_NIL
} LOG_Level_t;

//-------------------------------------------------------------------------------------------------

extern bool LogPrintFlag;

/**
 * @brief Print formatted string to debug output.
 * @param[in] template Format string
 * @param[in] ... Format arguments
 */
void print(const char *template, ...);

/** @brief No-op log function (for disabled levels). */
void LOG_Nope(const char *message, ...);

/** @brief Log bash/shell response. */
void LOG_Bash(const char *message, ...);

/** @brief Log debug message (level DBG). */
void LOG_Debug(const char *message, ...);

/** @brief Log info message (level INF). */
void LOG_Info(const char *message, ...);

/** @brief Log warning message (level WRN). */
void LOG_Warning(const char *message, ...);

/** @brief Log error message (level ERR). */
void LOG_Error(const char *message, ...);

/** @brief Log critical message and flush (level CRT). */
void LOG_Critical(const char *message, ...);

/** @brief Log panic message and flush blocking (level PAC). */
void LOG_Panic(const char *message);

/**
 * @brief Log message with specified level.
 * @param[in] lvl Log level
 * @param[in] message Format string
 * @param[in] ... Format arguments
 */
void LOG_Message(LOG_Level_t lvl, char *message, ...);

#define LOG_NOP LOG_Nope
#define LOG_DBG LOG_Debug
#define LOG_INF LOG_Info
#define LOG_WRN LOG_Warning
#define LOG_ERR LOG_Error
#define LOG_CRT LOG_Critical
#define LOG_PAC LOG_Panic
#define LOG_MSG LOG_Message

/**
 * @brief Log parse error.
 * @param[in] value Value that failed to parse
 * @param[in] type Expected type name
 */
void LOG_ErrorParse(const char *value, const char *type);

//-------------------------------------------------------------------------------------------------

#endif
#ifndef LOG_H_
#define LOG_H_

#include "dbg.h"
#include "xdef.h"
#include "vrts.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

// Maksymalna długość separatora używanego przy wypisywaniu elementów tablicy (iterable)
#ifndef LOG_ARYSPACE_MAXLEN
  #define LOG_ARYSPACE_MAXLEN 32
#endif

// Czy dodawać milisekundy do znacznika czasu w logach?
#ifndef LOG_INCLUDE_MS
  #define LOG_INCLUDE_MS 0
#endif

// Czy wyświetlać tylko czas bez daty?
#ifndef LOG_TIME_ONLY
  #define LOG_TIME_ONLY 0
#endif

#ifndef LOG_COLORS
  #define LOG_COLORS 1
#endif

#if(LOG_COLORS)
  #define LOG_LIB(name) ANSI_GREY " <" ANSI_ORANGE name ANSI_GREY ">" ANSI_END
#else
  #define LOG_LIB(name) " <" name ">"
#endif

#define LOG_LEVEL_DBG 0
#define LOG_LEVEL_INF 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_ERR 3
#define LOG_LEVEL_CRT 4
#define LOG_LEVEL_PAC 5
#define LOG_LEVEL_NIL 6

typedef enum {
  LOG_Level_Debug = LOG_LEVEL_DBG,
  LOG_Level_Info = LOG_LEVEL_INF,
  LOG_Level_Warning = LOG_LEVEL_WRN,
  LOG_Level_Error = LOG_LEVEL_ERR,
  LOG_Level_Critical = LOG_LEVEL_CRT,
  LOG_Level_Panic = LOG_LEVEL_PAC,
  LOG_Level_None = LOG_LEVEL_NIL
} LOG_Level_e;

#ifndef LOG_LEVEL
  #define LOG_LEVEL LOG_LEVEL_INF
#endif

//-------------------------------------------------------------------------------------------------

extern bool LogPrintFlag;

void print(const char *template, ...);
void LOG_Init(const char *greeting, const char *version);
void LOG_Nope(const char *message, ...);
void LOG_Bash(const char *message, ...);
void LOG_Debug(const char *message, ...);
void LOG_Info(const char *message, ...);
void LOG_Warning(const char *message, ...);
void LOG_Error(const char *message, ...);
void LOG_Critical(const char *message, ...);
void LOG_Panic(const char *message);
void LOG_Message(LOG_Level_e lvl, char *message, ...);

#define LOG_INI LOG_Init
#define LOG_NOP LOG_Nope
#define LOG_DBG LOG_Debug
#define LOG_INF LOG_Info
#define LOG_WRN LOG_Warning
#define LOG_ERR LOG_Error
#define LOG_CRT LOG_Critical
#define LOG_PAC LOG_Panic
#define LOG_MSG LOG_Message

void LOG_ErrorParse(const char *value, const char *type);

//-------------------------------------------------------------------------------------------------
#endif
// lib/sh/log.c

#include "log.h"

#include <stdarg.h>
#include <string.h>

bool LogPrintFlag = true;

//------------------------------------------------------------------------------------------- Print

static uint8_t read_number(const char **format)
{
  uint8_t nbr = 0;
  while(**format >= '0' && **format <= '9') {
    nbr = nbr * 10 + (**format - '0');
    (*format)++;
  }
  return nbr;
}

// Next element of an integer array, widened to 64 bits; `ary` moves past it
static int64_t next_element(const uint8_t **ary, uint8_t size, bool sign)
{
  const uint8_t *p = *ary;
  switch(size) {
    case 8: *ary += 8; return *(const int64_t *)p;
    case 4: *ary += 4; return sign ? *(const int32_t *)p : (int64_t)*(const uint32_t *)p;
    case 2: *ary += 2; return sign ? *(const int16_t *)p : (int64_t)*(const uint16_t *)p;
    default: *ary += 1; return sign ? *(const int8_t *)p : (int64_t)*p;
  }
}

// `%a` state of one format pass
typedef struct {
  uint32_t count;
  uint8_t size;
  uint8_t sep_len;
  char sep[LOG_ARYSPACE_MAXLEN + 1];
} array_t;

// Separator between elements, nothing after the last one
static void array_sep(array_t *ary)
{
  if(ary->sep_len && ary->count) DBG_String(ary->sep);
}

// Integer conversion of `%d`, `%u`, `%x`, `%b`: one value, or every array element
static void print_int(va_list *args, array_t *ary, uint8_t base, bool sign, bool long_int,
  uint8_t fill_zero, uint8_t fill_space)
{
  if(ary->count) {
    const uint8_t *data = va_arg(*args, const uint8_t *);
    uint8_t size = base == 2 ? 1 : ary->size;
    while(ary->count) {
      ary->count--;
      DBG_Int(next_element(&data, size, sign), base, sign, fill_zero, fill_space);
      array_sep(ary);
    }
    return;
  }
  int64_t nbr;
  if(base == 2) nbr = (uint8_t)va_arg(*args, int);
  else if(long_int) nbr = va_arg(*args, int64_t);
  else nbr = sign ? va_arg(*args, int32_t) : (int64_t)va_arg(*args, uint32_t);
  DBG_Int(nbr, base, sign, fill_zero, fill_space);
}

static void print_args(const char *format, va_list args)
{
  array_t ary = { .size = 1 };
  while(*format) {
    if(*format != '%') {
      if(ary.count) {
        if(ary.sep_len < LOG_ARYSPACE_MAXLEN) ary.sep[ary.sep_len++] = *format;
      }
      else DBG_Char(*format);
      format++;
      continue;
    }
    format++;
    // A trailing `%` has no specifier, and the parsing below never tests for the end
    if(!*format) { DBG_Char('%'); break; }
    bool flag_zero = false;
    while(*format == '0') { flag_zero = true; format++; }
    uint8_t width = read_number(&format);
    uint8_t precision = 0;
    bool has_precision = false;
    if(*format == '.') {
      format++;
      precision = read_number(&format);
      has_precision = true;
    }
    bool long_int = false;
    while(*format == 'l') { format++; long_int = true; }
    // printf semantics: width is the field width, space-padded unless the `0` flag
    // asks for zeros; precision is the least digit count of an integer
    uint8_t fill_space = flag_zero ? 0 : width;
    uint8_t fill_zero = flag_zero ? width : (has_precision ? precision : 0);
    switch(*format) {
      case 'a': case 'A':
        ary.count = va_arg(args, uint32_t);
        ary.size = maxv(precision, width);
        if(!ary.size) ary.size = 1;
        memset(ary.sep, 0, sizeof(ary.sep));
        ary.sep_len = 0;
        break;
      case 'i': case 'd':
        print_int(&args, &ary, 10, true, long_int, fill_zero, fill_space);
        break;
      case 'u':
        print_int(&args, &ary, 10, false, long_int, fill_zero, fill_space);
        break;
      case 'x': case 'X':
        print_int(&args, &ary, 16, false, long_int, fill_zero, fill_space);
        break;
      case 'b':
        print_int(&args, &ary, 2, false, false, fill_zero, fill_space);
        break;
      case 'f': case 'F': {
        if(!has_precision) precision = *format == 'f' ? 3 : 2;
        if(ary.count) {
          const float *data = va_arg(args, const float *);
          while(ary.count) {
            ary.count--;
            DBG_FloatSpace(*data++, precision, width);
            array_sep(&ary);
          }
        }
        else DBG_FloatSpace((float)va_arg(args, double), precision, width);
        break;
      }
      case 'p': {
        // `0x` and as many hex digits as the platform pointer holds
        uintptr_t ptr = (uintptr_t)va_arg(args, void *);
        uint8_t digits = 2 * sizeof(void *);
        DBG_String("0x");
        DBG_Int((int64_t)ptr, 16, false, digits, digits);
        break;
      }
      case 'c': {
        if(ary.count) {
          const char *data = va_arg(args, const char *);
          while(ary.count) {
            ary.count--;
            DBG_Char(*data++);
            array_sep(&ary);
          }
        }
        else DBG_Char((char)va_arg(args, int));
        break;
      }
      case 's': {
        if(ary.count) {
          const char *const *data = va_arg(args, const char *const *);
          while(ary.count) {
            ary.count--;
            DBG_String(*data++);
            array_sep(&ary);
          }
        }
        else DBG_String(va_arg(args, const char *));
        break;
      }
      case 'S': {
        if(ary.count) {
          const uint8_t *data = va_arg(args, const uint8_t *);
          const char *const *table = va_arg(args, const char *const *);
          while(ary.count) {
            ary.count--;
            DBG_String(table[next_element(&data, ary.size, false)]);
            array_sep(&ary);
          }
        }
        else {
          uint32_t idx = va_arg(args, uint32_t);
          const char *const *table = va_arg(args, const char *const *);
          DBG_String(table[idx]);
        }
        break;
      }
      case 'o': case 'O': {
        void *obj = va_arg(args, void *);
        int32_t (*Print)(void *) = va_arg(args, int32_t (*)(void *));
        if(ary.count) {
          uint32_t size = va_arg(args, uint32_t);
          while(ary.count) {
            ary.count--;
            Print(obj);
            array_sep(&ary);
            obj = (uint8_t *)obj + size;
          }
        }
        else Print(obj);
        break;
      }
      case 'B': {
        if(ary.count) {
          const bool *data = va_arg(args, const bool *);
          while(ary.count) {
            ary.count--;
            DBG_Bool(*data++);
            array_sep(&ary);
          }
        }
        else DBG_Bool((bool)va_arg(args, int));
        break;
      }
      case 't': case 'T': {
        // Time on demand, no argument consumed; the tick stands in before the RTC runs
        if(!RtcInit) {
          DBG_Int(tick_keep(0), 10, false, 0, 0);
          break;
        }
        RTC_Datetime_t dt = RTC_Datetime();
        if(*format == 't') {
          if(long_int) DBG_TimeMs(&dt);
          else DBG_Time(&dt);
        }
        else {
          if(long_int) DBG_DatetimeMs(&dt);
          else DBG_Datetime(&dt);
        }
        break;
      }
      case '%':
        DBG_Char('%');
        break;
      default:
        // Unknown specifier echoed raw, so the typo shows; no argument is consumed
        DBG_Char('%');
        DBG_Char(*format);
        break;
    }
    format++;
  }
}

void print(const char *template, ...)
{
  va_list args;
  va_start(args, template);
  print_args(template, args);
  va_end(args);
}

//--------------------------------------------------------------------------------------------- Log

static const char *const level_tag[] = {
  [LOG_Level_Debug] = ANSI_GREY "DBG " ANSI_END,
  [LOG_Level_Info] = ANSI_BLUE "INF " ANSI_END,
  [LOG_Level_Warning] = ANSI_YELLOW "WRN " ANSI_END,
  [LOG_Level_Error] = ANSI_RED "ERR " ANSI_END,
  [LOG_Level_Critical] = ANSI_MAGNTA "CRT " ANSI_END,
  [LOG_Level_Panic] = ANSI_MAGNTA "PNC " ANSI_END
};

// Tag, message, line break. Critical is pushed to the port at once, panic blocks on it;
// the rest waits for `DBG_Loop` and stays silent while `LogPrintFlag` is clear
static void emit(LOG_Level_t lvl, const char *message, va_list args)
{
  if(lvl < LOG_Level_Critical && !LogPrintFlag) return;
  DBG_String(level_tag[lvl]);
  print_args(message, args);
  DBG_Enter();
  if(lvl == LOG_Level_Critical) {
    DBG_Send(DbgFile->buffer, DbgFile->size);
    MBB_Clear(DbgFile);
  }
  else if(lvl == LOG_Level_Panic) {
    DBG_WaitBlock();
    UART_Send(DbgUart, DbgFile->buffer, DbgFile->size);
    DBG_WaitBlock();
    MBB_Clear(DbgFile);
  }
}

// Variadic front of `emit`, one per level below
#define LOG_EMIT(lvl, message) do { \
  va_list args; \
  va_start(args, message); \
  emit(lvl, message, args); \
  va_end(args); \
} while(0)

void LOG_Nope(const char *message, ...)
{
  unused(message);
}

void LOG_Bash(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  DBG_String(ANSI_GREEN "INF " ANSI_END);
  print_args(message, args);
  DBG_Enter();
  va_end(args);
}

void LOG_Debug(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_DBG)
  LOG_EMIT(LOG_Level_Debug, message);
  #else
  unused(message);
  #endif
}

void LOG_Info(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_INF)
  LOG_EMIT(LOG_Level_Info, message);
  #else
  unused(message);
  #endif
}

void LOG_Warning(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_WRN)
  LOG_EMIT(LOG_Level_Warning, message);
  #else
  unused(message);
  #endif
}

void LOG_Error(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_ERR)
  LOG_EMIT(LOG_Level_Error, message);
  #else
  unused(message);
  #endif
}

void LOG_Critical(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_CRT)
  LOG_EMIT(LOG_Level_Critical, message);
  #else
  unused(message);
  #endif
}

void LOG_Panic(const char *message)
{
  #if(LOG_LEVEL <= LOG_LEVEL_PNC)
  DBG_String(level_tag[LOG_Level_Panic]);
  DBG_String(message);
  DBG_Enter();
  DBG_WaitBlock();
  UART_Send(DbgUart, DbgFile->buffer, DbgFile->size);
  DBG_WaitBlock();
  MBB_Clear(DbgFile);
  #else
  unused(message);
  #endif
}

void LOG_Message(LOG_Level_t lvl, const char *message, ...)
{
  if(lvl < LOG_LEVEL || lvl >= LOG_Level_None) return;
  LOG_EMIT(lvl, message);
}

void LOG_ErrorParse(const char *value, const char *type)
{
  LOG_Error("Parse " ANSI_ORANGE "%s" ANSI_END " to " ANSI_TURQUS "%s" ANSI_END " fault",
    value, type);
}

//-------------------------------------------------------------------------------------------------

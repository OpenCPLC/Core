// lib/sh/log.c

#include "log.h"

bool LogPrintFlag = true;

//---------------------------------------------------------------------------------- print_args

static uint8_t print_args_getstrnbr(const char **format)
{
  uint8_t nbr = 0;
  while(**format >= '0' && **format <= '9') {
    nbr = nbr * 10 + (**format - '0');
    (*format)++;
  }
  return nbr;
}

void print_args(const char *format, va_list args)
{
  uint8_t ary_type = 1, ary_space_len = 0, ary_count = 0;
  char ary_space[LOG_ARYSPACE_MAXLEN + 1];
  while(*format) {
    if(*format == '%') {
      format++;
      // Flag: `0` = zero-pad
      bool flag_zero = false;
      while(*format == '0') { flag_zero = true; format++; }
      uint8_t width = print_args_getstrnbr(&format);
      uint8_t precision = 0;
      bool has_precision = false;
      if(*format == '.') {
        format++;
        precision = print_args_getstrnbr(&format);
        has_precision = true;
      }
      bool long_int = false;
      if(*format == 'l') { format++; long_int = true; }
      if(*format == 'l') { format++; long_int = true; }
      // printf semantics: width = total field width (space-pad by default,
      // zero-pad with `0` flag), precision = min digits for integers.
      // DBG_Int(nbr, base, sign, fill_zero, fill_space).
      uint8_t fill_space = flag_zero ? 0 : width;
      uint8_t fill_zero = flag_zero ? width : (has_precision ? precision : 0);
      switch(*format) {
        case 'a': case 'A': {
          ary_count = va_arg(args, uint32_t);
          ary_type = precision > width ? precision : width;
          if(!ary_type) ary_type = 1;
          memset(ary_space, 0, LOG_ARYSPACE_MAXLEN + 1);
          ary_space_len = 0;
          break;
        }
        case 'i': case 'd': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 8:
                  DBG_Int(*(int64_t *)ary, 10, true, fill_zero, fill_space);
                  ary += 8;
                  break;
                case 4:
                  DBG_Int(*(int32_t *)ary, 10, true, fill_zero, fill_space);
                  ary += 4;
                  break;
                case 2:
                  DBG_Int(*(int16_t *)ary, 10, true, fill_zero, fill_space);
                  ary += 2;
                  break;
                default:
                  DBG_Int(*(int8_t *)ary, 10, true, fill_zero, fill_space);
                  ary++;
                  break;
              }
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            int64_t nbr;
            if(long_int) nbr = va_arg(args, int64_t);
            else nbr = va_arg(args, int32_t);
            DBG_Int(nbr, 10, true, fill_zero, fill_space);
          }
          break;
        }
        case 'u': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 8:
                  DBG_Int(*(uint64_t *)ary, 10, false, fill_zero, fill_space);
                  ary += 8;
                  break;
                case 4:
                  DBG_Int(*(uint32_t *)ary, 10, false, fill_zero, fill_space);
                  ary += 4;
                  break;
                case 2:
                  DBG_Int(*(uint16_t *)ary, 10, false, fill_zero, fill_space);
                  ary += 2;
                  break;
                default:
                  DBG_Int(*(uint8_t *)ary, 10, false, fill_zero, fill_space);
                  ary++;
                  break;
              }
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            uint64_t nbr;
            if(long_int) nbr = va_arg(args, uint64_t);
            else nbr = va_arg(args, uint32_t);
            DBG_Int(nbr, 10, false, fill_zero, fill_space);
          }
          break;
        }
        case 'f': {
          if(!has_precision) precision = 3;
          fallthrough;
        }
        case 'F': {
          if(!has_precision && *format == 'F') precision = 2;
          if(ary_count) {
            float *ary = va_arg(args, float *);
            while(ary_count) {
              DBG_FloatSpace(*ary, precision, width);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              ary++;
            }
          }
          else {
            double nbr = va_arg(args, double);
            DBG_FloatSpace((float)nbr, precision, width);
          }
          break;
        }
        case 'x': case 'X': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 8:
                  DBG_Int(*(uint64_t *)ary, 16, false, fill_zero, fill_space);
                  ary += 8;
                  break;
                case 4:
                  DBG_Int(*(uint32_t *)ary, 16, false, fill_zero, fill_space);
                  ary += 4;
                  break;
                case 2:
                  DBG_Int(*(uint16_t *)ary, 16, false, fill_zero, fill_space);
                  ary += 2;
                  break;
                default:
                  DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_space);
                  ary++;
                  break;
              }
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            uint64_t nbr;
            if(long_int) nbr = va_arg(args, uint64_t);
            else nbr = va_arg(args, uint32_t);
            DBG_Int(nbr, 16, false, fill_zero, fill_space);
          }
          break;
        }
        case 'p': {
          // Pointer: `0x` prefix + hex digits sized to platform pointer width.
          // 32-bit (STM32): 8 digits. 64-bit (host build): 16 digits.
          void *ptr = va_arg(args, void *);
          DBG_String("0x");
          if(sizeof(void *) == 8) {
            DBG_Int((int64_t)(uintptr_t)ptr, 16, false, 16, 16);
          }
          else {
            DBG_Int((uint32_t)(uintptr_t)ptr, 16, false, 8, 8);
          }
          break;
        }
        case 'c': {
          if(ary_count) {
            char *ary = va_arg(args, char *);
            while(ary_count) {
              DBG_Char(*ary);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              ary++;
            }
          }
          else {
            char sing = (char)va_arg(args, int);
            DBG_Char(sing);
          }
          break;
        }
        case 's': {
          if(ary_count) {
            char **str = va_arg(args, char **);
            while(ary_count) {
              DBG_String(*str);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              str++;
            }
          }
          else {
            char *str = va_arg(args, char *);
            DBG_String(str);
          }
          break;
        }
        case 'S': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            char **str = va_arg(args, char **);
            while(ary_count) {
              uint32_t idx;
              switch(ary_type) {
                case 8: idx = (uint32_t)(*(uint64_t *)ary); ary += 8; break;
                case 4: idx = *(uint32_t *)ary; ary += 4; break;
                case 2: idx = *(uint16_t *)ary; ary += 2; break;
                default: idx = *ary; ary++; break;
              }
              DBG_String(str[idx]);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            uint32_t n = va_arg(args, uint32_t);
            char **str = va_arg(args, char **);
            DBG_String(str[n]);
          }
          break;
        }
        case 'o': case 'O': {
          if(ary_count) {
            void *obj = va_arg(args, void *);
            int32_t (*Print)(void *) = va_arg(args, int32_t (*)(void *));
            uint32_t size = va_arg(args, uint32_t);
            while(ary_count) {
              Print(obj);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              obj = (void *)((uint8_t *)obj + size);
            }
          }
          else {
            void *obj = va_arg(args, void *);
            int32_t (*Print)(void *) = va_arg(args, int32_t (*)(void *));
            Print(obj);
          }
          break;
        }
        case 'b': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              DBG_Int(*ary, 2, false, fill_zero, fill_space);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              ary++;
            }
          }
          else {
            uint8_t bin = (uint8_t)va_arg(args, int);
            DBG_Int(bin, 2, false, fill_zero, fill_space);
          }
          break;
        }
        case 'B': {
          if(ary_count) {
            bool *ary = va_arg(args, bool *);
            while(ary_count) {
              DBG_Bool(*ary);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              ary++;
            }
          }
          else {
            bool true_false = (bool)va_arg(args, int);
            DBG_Bool(true_false);
          }
          break;
        }
        case 't': {
          // Current RTC time on demand. No arg consumed.
          // `%t` -> HH:MM:SS, `%lt` -> HH:MM:SS.mmm
          // Falls back to tick value when RTC is not initialized.
          if(RtcInit) {
            RTC_Datetime_t dt = RTC_Datetime();
            if(long_int) DBG_TimeMs(&dt);
            else DBG_Time(&dt);
          }
          else DBG_Int(tick_keep(0), 10, false, 0, 0);
          break;
        }
        case 'T': {
          // Current RTC datetime on demand. No arg consumed.
          // `%T` -> YYYY-MM-DD HH:MM:SS, `%lT` -> with ms
          // Falls back to tick value when RTC is not initialized.
          if(RtcInit) {
            RTC_Datetime_t dt = RTC_Datetime();
            if(long_int) DBG_DatetimeMs(&dt);
            else DBG_Datetime(&dt);
          }
          else DBG_Int(tick_keep(0), 10, false, 0, 0);
          break;
        }
        case '%': {
          DBG_Char('%');
          break;
        }
        default: {
          // Unknown specifier: echo raw `%X` so user can spot the typo.
          // Note: no arg consumed, so subsequent args may misalign.
          DBG_Char('%');
          DBG_Char(*format);
          break;
        }
      }
    }
    else {
      if(ary_count) {
        if(ary_space_len < LOG_ARYSPACE_MAXLEN) ary_space[ary_space_len++] = *format;
      }
      else DBG_Char(*format);
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

//----------------------------------------------------------------------------------------- Log

// Common emit path: colored tag + formatted message + newline.
// No automatic timestamp — use `%t` or `%T` in the format string when needed.
static void log_emit(const char *color_tag, const char *message, va_list args)
{
  DBG_String((char *)color_tag);
  print_args(message, args);
  DBG_Enter();
}

void LOG_Nope(const char *message, ...)
{
  unused(message);
}

void LOG_Bash(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  log_emit(ANSI_GREEN "INF " ANSI_END, message, args);
  va_end(args);
}

void LOG_Debug(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_DBG)
    if(!LogPrintFlag) return;
    va_list args;
    va_start(args, message);
    log_emit(ANSI_GREY "DBG " ANSI_END, message, args);
    va_end(args);
  #else
    unused(message);
  #endif
}

void LOG_Info(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_INF)
    if(!LogPrintFlag) return;
    va_list args;
    va_start(args, message);
    log_emit(ANSI_BLUE "INF " ANSI_END, message, args);
    va_end(args);
  #else
    unused(message);
  #endif
}

void LOG_Warning(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_WRN)
    if(!LogPrintFlag) return;
    va_list args;
    va_start(args, message);
    log_emit(ANSI_YELLOW "WRN " ANSI_END, message, args);
    va_end(args);
  #else
    unused(message);
  #endif
}

void LOG_Error(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_ERR)
    if(!LogPrintFlag) return;
    va_list args;
    va_start(args, message);
    log_emit(ANSI_RED "ERR " ANSI_END, message, args);
    va_end(args);
  #else
    unused(message);
  #endif
}

void LOG_Critical(const char *message, ...)
{
  #if(LOG_LEVEL <= LOG_LEVEL_CRT)
    va_list args;
    va_start(args, message);
    log_emit(ANSI_MAGNTA "CRT " ANSI_END, message, args);
    va_end(args);
    DBG_Send(DbgFile->buffer, DbgFile->size);
    MBB_Clear(DbgFile);
  #else
    unused(message);
  #endif
}

// Panic emit: format message with args, flush blocking, clear buffer.
// Used by `LOG_Message(LOG_Level_Panic, ...)`. `LOG_Panic` itself is
// non-variadic and uses simpler path (raw string only).
static void log_panic_emit(const char *message, va_list args)
{
  DBG_String(ANSI_MAGNTA "PNC " ANSI_END);
  print_args(message, args);
  DBG_Enter();
  DBG_WaitBlock();
  UART_Send(DbgUart, DbgFile->buffer, DbgFile->size);
  DBG_WaitBlock();
  MBB_Clear(DbgFile);
}

void LOG_Panic(const char *message)
{
  #if(LOG_LEVEL <= LOG_LEVEL_PAC)
    DBG_String(ANSI_MAGNTA "PNC " ANSI_END);
    DBG_String((char *)message);
    DBG_Enter();
    DBG_WaitBlock();
    UART_Send(DbgUart, DbgFile->buffer, DbgFile->size);
    DBG_WaitBlock();
    MBB_Clear(DbgFile);
  #else
    unused(message);
  #endif
}

void LOG_Message(LOG_Level_t lvl, char *message, ...)
{
  va_list args;
  va_start(args, message);
  switch(lvl) {
    #if(LOG_LEVEL <= LOG_LEVEL_DBG)
      case LOG_Level_Debug:
        if(LogPrintFlag) log_emit(ANSI_GREY "DBG " ANSI_END, message, args);
        break;
    #endif
    #if(LOG_LEVEL <= LOG_LEVEL_INF)
      case LOG_Level_Info:
        if(LogPrintFlag) log_emit(ANSI_BLUE "INF " ANSI_END, message, args);
        break;
    #endif
    #if(LOG_LEVEL <= LOG_LEVEL_WRN)
      case LOG_Level_Warning:
        if(LogPrintFlag) log_emit(ANSI_YELLOW "WRN " ANSI_END, message, args);
        break;
    #endif
    #if(LOG_LEVEL <= LOG_LEVEL_ERR)
      case LOG_Level_Error:
        if(LogPrintFlag) log_emit(ANSI_RED "ERR " ANSI_END, message, args);
        break;
    #endif
    #if(LOG_LEVEL <= LOG_LEVEL_CRT)
      case LOG_Level_Critical:
        log_emit(ANSI_MAGNTA "CRT " ANSI_END, message, args);
        DBG_Send(DbgFile->buffer, DbgFile->size);
        MBB_Clear(DbgFile);
        break;
    #endif
    case LOG_Level_Panic: log_panic_emit(message, args); break;
    case LOG_Level_None: break;
    default: break;
  }
  va_end(args);
}

//---------------------------------------------------------------------------------------------

void LOG_ErrorParse(const char *value, const char *type)
{
  LOG_Error("Parse " ANSI_ORANGE "%s" ANSI_END " to " ANSI_TURQUS "%s" ANSI_END " fault",
    value, type);
}
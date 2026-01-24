#include "log.h"

bool LogPrintFlag = true;

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
  uint8_t ary_type, ary_space_len, ary_count = 0;
  char ary_space[LOG_ARYSPACE_MAXLEN + 1];
  while(*format) {
    if(*format == '%') {
      format++;
      uint8_t width = print_args_getstrnbr(&format);
      if(*format == '-') format++;
      uint8_t precision = 0;
      bool default_precision = true;
      if(*format == '.') {
        format++;
        precision = print_args_getstrnbr(&format);
        default_precision = false;
      }
      bool long_int = false;
      if(*format == 'l') { format++; long_int = true; }
      if(*format == 'l') { format++; long_int = true; }
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
                case 8: DBG_Int(*(int64_t *)ary, 10, true, width, precision); ary += 8; break;
                case 4: DBG_Int(*(int32_t *)ary, 10, true, width, precision); ary += 4; break;
                case 2: DBG_Int(*(int16_t *)ary, 10, true, width, precision); ary += 2; break;
                default: DBG_Int(*(int8_t *)ary, 10, true, width, precision); ary++; break;
              }
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            int64_t nbr;
            if(long_int) nbr = va_arg(args, int64_t);
            else nbr = va_arg(args, int32_t);
            DBG_Int(nbr, 10, true, width, precision);
          }
          break;
        }
        case 'u': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 8: DBG_Int(*(uint64_t *)ary, 10, false, width, precision); ary += 8; break;
                case 4: DBG_Int(*(uint32_t *)ary, 10, false, width, precision); ary += 4; break;
                case 2: DBG_Int(*(uint16_t *)ary, 10, false, width, precision); ary += 2; break;
                default: DBG_Int(*(uint8_t *)ary, 10, false, width, precision); ary++; break;
              }
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            uint64_t nbr;
            if(long_int) nbr = va_arg(args, uint64_t);
            else nbr = va_arg(args, uint32_t);
            DBG_Int(nbr, 10, false, width, precision);
          }
          break;
        }
        case 'f':
          if(default_precision) precision = 3;
        case 'F': {
          if(default_precision) precision = 2;
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
          uint8_t fill_zero = precision > width ? precision : width;
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 8: DBG_Int(*(uint64_t *)ary, 16, false, fill_zero, fill_zero); ary += 8; break;
                case 4: DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_zero); ary += 4; break;
                case 2: DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_zero); ary += 2; break;
                default: DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_zero); ary++; break;
              }
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
            }
          }
          else {
            uint64_t nbr;
            if(long_int) nbr = va_arg(args, uint64_t);
            else nbr = va_arg(args, uint32_t);
            DBG_Int(nbr, 16, false, fill_zero, fill_zero);
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
              DBG_String(str[*ary]);
              switch(ary_type) {
                case 4: ary += 4; break;
                case 2: ary += 2; break;
                default: ary++; break;
              }
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
          uint8_t fill_zero = precision > width ? precision : width;
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              DBG_Int(*ary, 2, false, fill_zero, fill_zero);
              ary_count--;
              if(ary_space_len && ary_count) DBG_String((char *)&ary_space);
              ary++;
            }
          }
          else {
            uint8_t bin = (uint8_t)va_arg(args, int);
            DBG_Int(bin, 2, false, fill_zero, fill_zero);
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
        case '%': {
          DBG_Char('%');
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

//------------------------------------------------------------------------------------------------- Log

inline static void LOG_Datetime(void)
{
  if(RtcInit) {
    RTC_Datetime_t dt = RTC_Datetime();
    #if(LOG_INCLUDE_MS)
      #if(LOG_TIME_ONLY)
        DBG_TimeMs(&dt);
      #else
        DBG_DatetimeMs(&dt);
      #endif
    #else
      #if(LOG_TIME_ONLY)
        DBG_Time(&dt);
      #else
        DBG_Datetime(&dt);
      #endif
    #endif
  }
  else {
    int64_t tick = tick_keep(0);
    DBG_Int(tick, 10, false, 8, 8);
  }
  DBG_Char(' ');
}

void LOG_Init(const char *greeting, const char *version)
{
  DBG_Enter();
  #if(LOG_COLORS)
    DBG_String(ANSI_END);
  #endif
  LOG_Datetime();
  #if(LOG_COLORS)
    DBG_String(ANSI_CYAN "INI " ANSI_END);
    DBG_String((char *)greeting);
    if(version) {
      DBG_String(ANSI_GREY" {"ANSI_BLUE);
      DBG_String((char *)version);
      DBG_String(ANSI_GREY"}"ANSI_END);
    }
  #else
    DBG_String("INI: ");
    DBG_String(greeting);
    if(version) {
      DBG_String(" {");
      DBG_String((char *)version);
      DBG_Char('}');
    }
  #endif
  DBG_Enter();
}

void LOG_Nope(const char *message, ...)
{
  __NOP();
}

static void LOG_BashArgs(const char *message, va_list args)
{
  LOG_Datetime();
  #if(LOG_COLORS)
    DBG_String(ANSI_GREEN "INF " ANSI_END);
  #else
    DBG_String("INF: ");
  #endif
  print_args(message, args);
  DBG_Enter();
}

void LOG_Bash(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  LOG_BashArgs(message, args);
  va_end(args);
}

static void LOG_DebugArgs(const char *message, va_list args)
{
  #if(LOG_LEVEL <= LOG_LEVEL_DBG)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String(ANSI_GREEN "DBG " ANSI_END);
    #else
      DBG_String("DBG: ");
    #endif
    print_args(message, args);
    DBG_Enter();
  #endif
}

void LOG_Debug(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  LOG_DebugArgs(message, args);
  va_end(args);
}

static void LOG_InfoArgs(const char *message, va_list args)
{
  #if(LOG_LEVEL <= LOG_LEVEL_INF)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String(ANSI_BLUE "INF " ANSI_END);
    #else
      DBG_String("INF: ");
    #endif
    print_args(message, args);
    DBG_Enter();
  #endif
}

void LOG_Info(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  LOG_InfoArgs(message, args);
  va_end(args);
}

static void LOG_WarningArgs(const char *message, va_list args)
{
  #if(LOG_LEVEL <= LOG_LEVEL_WRN)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String(ANSI_YELLOW "WRN " ANSI_END);
    #else
      DBG_String("WRN: ");
    #endif
    print_args(message, args);
    DBG_Enter();
  #endif
}

void LOG_Warning(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  LOG_WarningArgs(message, args);
  va_end(args);
}

static void LOG_ErrorArgs(const char *message, va_list args)
{
  #if(LOG_LEVEL <= LOG_LEVEL_ERR)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String(ANSI_RED "ERR " ANSI_END);
    #else
      DBG_String("ERR: ");`
    #endif
    print_args(message, args);
    DBG_Enter();
  #endif
}

void LOG_Error(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  LOG_ErrorArgs(message, args);
  va_end(args);
}

static void LOG_CriticalArgs(const char *message, va_list args)
{
  #if(LOG_LEVEL <= LOG_LEVEL_CRT)
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String(ANSI_MAGENTA "CRT " ANSI_END);
    #else
      DBG_String("CRT: ");
    #endif
    print_args(message, args);
    DBG_Enter();
    // DbgSendFlag = false;
    DBG_Send(DbgFile->buffer, DbgFile->size);
    FILE_Clear(DbgFile);
    // DbgSendFlag = true;
  #endif
}

void LOG_Critical(const char *message, ...)
{
  va_list args;
  va_start(args, message);
  LOG_CriticalArgs(message, args);
  va_end(args);
}

void LOG_Panic(const char *message)
{
  #if(LOG_LEVEL <= LOG_LEVEL_PAC)
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String(ANSI_MAGENTA "PNC " ANSI_END);
    #else
      DBG_String("PNC: ");
    #endif
    DBG_String((char *)message);
    DBG_Enter();
    DBG_WaitBlock();
    UART_Send(DbgUart, DbgFile->buffer, DbgFile->size);
    DBG_WaitBlock();
    FILE_Clear(DbgFile);
  #endif
}

void LOG_Message(LOG_Level_e lvl, char *message, ...)
{
  va_list args;
  va_start(args, message);
  switch(lvl) {
    case LOG_Level_Debug: LOG_DebugArgs(message, args); break;
    case LOG_Level_Info: LOG_InfoArgs(message, args); break;
    case LOG_Level_Warning: LOG_WarningArgs(message, args); break;
    case LOG_Level_Error: LOG_ErrorArgs(message, args); break;
    case LOG_Level_Critical: LOG_CriticalArgs(message, args); break;
    case LOG_Level_Panic: LOG_Panic(message); break;
    case LOG_Level_None: break;
  }
  va_end(args);
}

//-------------------------------------------------------------------------------------------------

void LOG_ErrorParse(const char *value, const char *type)
{
  #if(LOG_COLORS)
    LOG_Error("Parse " ANSI_ORANGE "%s" ANSI_END " to " ANSI_TURQS "%s" ANSI_END " fault", value, type);
  #else
    LOG_Error("Parse %s to %s fault", value, type);
  #endif
}

//-------------------------------------------------------------------------------------------------

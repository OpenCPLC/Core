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
  uint8_t ary_type;
  uint8_t ary_count = 0;
  while(*format) {
    if(*format == '%') {
      format++;
      uint8_t width = print_args_getstrnbr(&format);
      if(*format == '-') format++;
      uint8_t precision = 0;
      if(*format == '.') {
        format++;
        precision = print_args_getstrnbr(&format);
      }
      switch(*format) {
        case 'a': case 'A': {
          ary_count = va_arg(args, uint32_t);
          ary_type = precision > width ? precision : width;
          if(!ary_type) ary_type = 1;
          break;
        }
        case 'i': case 'd': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 4: DBG_Int(*(int32_t *)ary, 10, true, precision, width); ary += 4; break;
                case 2: DBG_Int(*(int16_t *)ary, 10, true, precision, width); ary += 2; break;
                default: DBG_Int(*(int8_t *)ary, 10, true, precision, width); ary++; break;
              }
              ary_count--;
            }
          }
          else {
            int32_t nbr = va_arg(args, int32_t);
            DBG_Int(nbr, 10, true, precision, width);
          }
          break;
        }
        case 'u': {
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              switch(ary_type) {
                case 4: DBG_Int(*(uint32_t *)ary, 10, false, precision, width); ary += 4; break;
                case 2: DBG_Int(*(uint16_t *)ary, 10, false, precision, width); ary += 2; break;
                default: DBG_Int(*(uint8_t *)ary, 10, false, precision, width); ary++; break;
              }
              ary_count--;
            }
          }
          else {
            uint32_t nbr = va_arg(args, uint32_t);
            DBG_Int(nbr, 10, false, precision, width);
          }
          break;
        }
        case 'f':
          if(!precision) precision = 3;
        case 'F': {
          if(!precision) precision = 2;
          if(ary_count) {
            float *ary = va_arg(args, float *);
            while(ary_count) {
              DBG_FloatSpace(*ary, precision, width);
              ary_count--;
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
                case 4: DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_zero); ary += 4; break;
                case 2: DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_zero); ary += 2; break;
                default: DBG_Int(*(uint8_t *)ary, 16, false, fill_zero, fill_zero); ary++; break;
              }
              ary_count--;
            }
          }
          else {
            uint32_t nbr = va_arg(args, uint32_t);
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
              ary++;
            }
          }
          else {
            char sing = (char)va_arg(args, int);
            DBG_Char(sing);
          }
          break;
        }
        case 's': case 'S': {
          if(ary_count) {
            char **str = va_arg(args, char **);
            while(ary_count) {
              DBG_String(*str);
              ary_count--;
              str++;
            }
          }
          else {
            char *str = va_arg(args, char *);
            DBG_String(str);
          }
          break;
        }
        case 'o': case 'O': {
          if(ary_count) {
            void *obj = va_arg(args, void *);
            int32_t (*Print)(void *) = va_arg(args, int32_t (*)(void *));
            while(ary_count) {
              Print(obj);
              ary_count--;
              obj = (void *)((uint8_t *)obj + ary_type);
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
            bool *ary = va_arg(args, bool *);
            while(ary_count) {
              DBG_Bool(*ary);
              ary_count--;
              ary++;
            }
          }
          else {
            bool true_false = (bool)va_arg(args, int);
            DBG_Bool(true_false);
          }
          break;
        }
        case 'B': {
          uint8_t fill_zero = precision > width ? precision : width;
          if(ary_count) {
            uint8_t *ary = va_arg(args, uint8_t *);
            while(ary_count) {
              DBG_Int(*ary, 2, false, fill_zero, fill_zero);
              ary_count--;
              ary++;
            }
          }
          else {
            uint8_t bin = (uint8_t)va_arg(args, int);
            DBG_Int(bin, 2, false, fill_zero, fill_zero);
          }
          break;
        }
        case '%': {
          DBG_Char('%');
        }
        default: {
          ary_count = 0;
        }
      }
    }
    else {
      DBG_Char(*format);
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
  if(!rtc_init) return;
  RTC_Datetime_t dt = RTC_Datetime();
  #if(LOG_MILLISECONDS)
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
  DBG_Char(' ');
}

void LOG_Init(const char *version, const char *board)
{
  #if(LOG_COLORS)
    DBG_String("\e[0m");
  #endif
  #if(LOG_INIT)
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[36mINIT:\e[0m ");
      DBG_String("OpenCPLC \e[90mversion:\e[0m");
      DBG_String((char *)version);
      DBG_String(" \e[90mboard:\e[0m");
    #else
      DBG_String("INIT: ");
      DBG_String("OpenCPLC version:");
      DBG_String((char *)version);
      DBG_String(" board:");
    #endif
    DBG_String((char *)board);
    DBG_Enter();
  #endif
}

static void LOG_BashArgs(const char *message, va_list args)
{
  LOG_Datetime();
  #if(LOG_COLORS)
    DBG_String("\e[32mBASH:\e[0m ");
  #else
    DBG_String("BASH: ");
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
  #if(LOG_LEVEL <= LOG_LEVEL_DBUG)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[92mDBUG:\e[0m ");
    #else
      DBG_String("DBUG: ");
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
  #if(LOG_LEVEL <= LOG_LEVEL_INFO)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[94mINFO:\e[0m ");
    #else
      DBG_String("INFO: ");
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
  #if(LOG_LEVEL <= LOG_LEVEL_WARN)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[93mWARN:\e[0m ");
    #else
      DBG_String("WARN: ");
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
  #if(LOG_LEVEL <= LOG_LEVEL_ERRO)
    if(!LogPrintFlag) return;
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[91mERRO:\e[0m ");
    #else
      DBG_String("ERRO: ");`
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
  #if(LOG_LEVEL <= LOG_LEVEL_CRIT)
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[95mCRIT:\e[0m ");
    #else
      DBG_String("CRIT: ");
    #endif
    print_args(message, args);
    DBG_Enter();
    DbgSendFlag = false;
    DBG_Send(DbgFile->buffer, DbgFile->size);
    DBG_Wait4Uart();
    FILE_Clear(DbgFile);
    DbgSendFlag = true;
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
  #if(LOG_LEVEL <= LOG_LEVEL_PANC)
    LOG_Datetime();
    #if(LOG_COLORS)
      DBG_String("\e[31mPANC:\e[0m ");
    #else
      DBG_String("PANC: ");
    #endif
    DBG_String((char *)message);
    DBG_Enter();
    DBG_Send(DbgFile->buffer, DbgFile->size);
    // DBG_Wait4Uart() + no-thread-switching
    while(UART_IsBusy(DbgUart)) __WFI();
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
    case LOG_Level_Void: break;
  }
  va_end(args);
}
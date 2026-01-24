#include "cmd.h"

//------------------------------------------------------------------------------------------------- STRUCT

static struct {
  FILE_t *files[CMD_FILE_LIMIT];
  uint32_t files_hash[CMD_FILE_LIMIT];
  uint8_t files_count;
  void (*callbacks[CMD_CALLBACK_LIMIT])(char **, uint16_t);
  uint32_t callbacks_hash[CMD_CALLBACK_LIMIT];
  uint8_t callbacks_count;
  void (*callback_default)(char **, uint16_t);
  bool flash_autosave;
  FILE_t *file_active;
  void (*Sleep)(PWR_SleepMode_e);
  void (*Reset)(void);
  uint16_t trig;
} bash;

/**
 * @brief Dodaje plik do systemu BASH.
 * @param file Wskaźnik do struktury pliku do dodania.
 */
void CMD_AddFile(FILE_t *file)
{
  if(bash.files_count >= CMD_FILE_LIMIT) {
    LOG_Error("BASH Exceeded file limit (max:%u)", CMD_FILE_LIMIT);
    return;
  }
  bash.files[bash.files_count] = file;
  bash.files_hash[bash.files_count] = hash_djb2_ci(file->name);
  if(!bash.files_count) {
    bash.file_active = file;
  }
  bash.files_count++;
  FILE_Flash_Load(file);
}

/**
 * @brief Dodaje nową komendę do systemu BASH, powiązaną z funkcją callback.
 * @param callback Funkcja obsługująca komendę
 * @param argv0 Nazwa komendy, przypisywana do funkcji callback.
 */
void CMD_AddCallback(void (*callback)(char **, uint16_t), char *argv0)
{
  if(!argv0 || !strlen(argv0)) {
    bash.callback_default = callback;
    return;
  }
  if(bash.callbacks_count >= CMD_CALLBACK_LIMIT) {
    LOG_Error("BASH Exceeded callback limit (max:%u)", CMD_CALLBACK_LIMIT);
    return;
  }
  bash.callbacks[bash.callbacks_count] = callback;
  bash.callbacks_hash[bash.callbacks_count] = hash_djb2_ci(argv0);
  bash.callbacks_count++;
}

/**
 * @brief Włącza lub wyłącza automatyczne zapisywanie danych do pamięci flash.
 * @param autosave Flaga włączająca (true) lub wyłączająca (false) autosave.
 */
void CMD_FlashAutosave(bool autosave)
{
  bash.flash_autosave = autosave;
}

static inline void CMD_WrongCommand(char *cmd)
{
  #if(LOG_COLORS)
    LOG_Warning("Wrong "ANSI_ORANGE"%s"ANSI_END" command usage", cmd);
  #else
    LOG_Warning("Wrong '%s' command usage", cmd);
  #endif
}

/**
 * @brief Wyświetla ostrzeżenie o nieprawidłowej liczbie argumentów dla danej komendy.
 * @param cmd Nazwa komendy, która została wywołana z błędną liczbą argumentów.
 * @param argc Ilość argumentów.
 */
void CMD_WrongArgc(char *cmd, uint16_t argc)
{
  CMD_WrongCommand(cmd);
  #if(LOG_COLORS)
    LOG_Warning("Incorrect argument count: "ANSI_LIME"%u"ANSI_END, argc);
  #else
    LOG_Warning("Incorrect argument count: %u", argc);
  #endif
}

/**
 * @brief Wyświetla ostrzeżenie o nieprawidłowym argumencie dla danej komendy.
 * @param cmd Nazwa komendy, która została wywołana z błędnym argumentem.
 * @param argv Wartość argumentu, który jest nieprawidłowy.
 * @param pos Pozycja argumentu, który jest nieprawidłowy.
 */
void CMD_WrongArgv(char *cmd, char *argv, uint16_t pos)
{
  CMD_WrongCommand(cmd);
  #if(LOG_COLORS)
    LOG_Warning("Invalid argument " ANSI_ORANGE "%s" ANSI_END \
      " on " ANSI_LIME "%u" ANSI_END " position", cmd, pos);
  #else
    LOG_Warning("Invalid argument '%s' on %u position", cmd, pos);
  #endif
}

//------------------------------------------------------------------------------------------------- DATA

static void CMD_Data(uint8_t *data, uint16_t size, STREAM_t *stream)
{
  stream->packages--;
  #if(LOG_COLORS)
    LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " data pack:" ANSI_LIME "%d" ANSI_END,
      bash.file_active->name, stream->packages);
  #elif
    LOG_Bash("File %s data pack:%d", bash.file_active->name, stream->packages);
  #endif
  bash.file_active->lock = false;
  FILE_Append(bash.file_active, data, size);
  if(!stream->packages) {
    STREAM_ArgsMode(stream);
    if(bash.flash_autosave) FILE_Flash_Save(bash.file_active);
  }
  else {
    bash.file_active->lock = true;
  }
}

//------------------------------------------------------------------------------------------------- FILE

static FILE_t *CMD_FindFile(char *file_name)
{
  uint32_t file_hash = hash_djb2(file_name);
  for(uint8_t i = 0; i < bash.files_count; i++) {
    if(file_hash == bash.files_hash[i]) {
      return bash.files[i];
    }
  }
  #if(LOG_COLORS)
    LOG_Warning("File " ANSI_ORANGE "%s" ANSI_END " not exist", (char *)file_name);
  #else
    LOG_Warning("File %s not exist", (char *)file_name);
  #endif
  return NULL;
}

static void CMD_AccessDenied(FILE_t *file)
{
  LOG_Warning("File %s access denied", (char *)file->name);
}

static void CMD_File(char **argv, uint16_t argc, STREAM_t *stream)
{
  if(!bash.files_count) {
    LOG_Warning("No file added to bash");
    return;
  }
  CMD_Argc(2, 4);
  switch(hash_djb2(argv[1])) {
    case HASH_List: { // FILE list
      CMD_Argc(2);
      const char *file_names[bash.files_count];
      for(uint16_t i = 0; i < bash.files_count; i++) {
        file_names[i] = bash.files[i]->name;
      }
      #if(LOG_COLORS)
        LOG_Bash("File list: " ANSI_CREAM "%a %s" ANSI_END, bash.files_count, file_names);
      #elif
        LOG_Bash("File list: %a, %s", bash.files_count, file_names);
      #endif
      break;
    }
    case HASH_Active:
    case HASH_Select: { // FILE select <file_name:str>
      CMD_Argc(3);
      FILE_t *file = CMD_FindFile(argv[2]);
      if(!file) return;
      bash.file_active = file;
      #if(LOG_COLORS)
        LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " selected", bash.file_active->name);
      #elif
        LOG_Bash("File %s was selected", bash.file_active->name);
      #endif
      break;
    }
    case HASH_Info: {  // FILE info
      CMD_Argc(2);
      LOG_Bash("File %o", bash.file_active, &DBG_File);
      break;
    }
    case HASH_Clear: { // FILE clear
      CMD_Argc(2);
      if(FILE_Clear(bash.file_active)) {
        CMD_AccessDenied(bash.file_active);
        return;
      }
      LOG_Bash("File %s is empty", bash.file_active->name);
      return;
    }
    case HASH_Save: { // FILE save <packages:uint16>?
      CMD_Argc(2, 3);
      if(FILE_Clear(bash.file_active)) CMD_AccessDenied(bash.file_active);
      else {
        uint16_t packages = 1;
        if(argc == 3) {
          if(!str_is_u16(argv[2])) {
            LOG_ErrorParse(argv[2], "uint16_t");
            CMD_ArgvExit(2);
          }
          packages = str_to_int(argv[2]);
        }
        STREAM_DataMode(stream);
        stream->packages = packages;
        bash.file_active->lock = true;
        #if(LOG_COLORS)
          LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " save pack:" ANSI_LIME "%d" ANSI_END, bash.file_active->name, stream->packages);
        #elif
          LOG_Bash("File %s save pack:%d", bash.file_active->name, stream->packages);
        #endif
      }
      break;
    }
    case HASH_Append: { // FILE append <packages:uint16>?
      uint16_t packages = 1;
      if(argc == 3) {
        if(!str_is_u16(argv[2])) {
          LOG_ErrorParse(argv[2], "uint16_t");
          CMD_ArgvExit(2);
        }
        packages = str_to_int(argv[2]);
      }
      STREAM_DataMode(stream);
      stream->packages = packages;
      bash.file_active->lock = true;
      #if(LOG_COLORS)
        LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " append pack:" ANSI_LIME "%d" ANSI_END, bash.file_active->name, stream->packages);
      #elif
        LOG_Bash("File %s append pack:%d", bash.file_active->name, stream->packages);
      #endif
      break;
    }
    case HASH_Load: { // FILE load <limit:uint16>? <offset:uint16>?
      CMD_Argc(2, 4);
      if(bash.file_active->lock) CMD_AccessDenied(bash.file_active);
      uint16_t limit = bash.file_active->size;
      uint16_t offset = 0;
      if(argc >= 3) {
        if(!str_is_u16(argv[2])) {
          LOG_ErrorParse(argv[2], "uint16_t");
          CMD_ArgvExit(2);
        }
        limit = str_to_int(argv[2]);
      }
      if(argc == 4) {
        if(!str_is_u16(argv[3])) {
          LOG_ErrorParse(argv[3], "uint16_t");
          CMD_ArgvExit(2);
        }
        offset = str_to_int(argv[3]);
      }
      if(offset >= bash.file_active->size) offset = 0;
      if(limit + offset > bash.file_active->size) limit = bash.file_active->size - offset;
      DBG_Data(&bash.file_active->buffer[offset], limit);
      DBG_Enter();
      break;
    }
    case HASH_Flash: { // FILE flash {save|load}
      CMD_Argc(3);
      switch(hash_djb2(argv[2])) {
        case HASH_Save:
          if(FILE_Flash_Save(bash.file_active)) LOG_Error("File %s flash save fault", bash.file_active->name);
          else LOG_Bash("File %s flash save success", bash.file_active->name);
          break;
        case HASH_Load: case HASH_Reset: 
          if(FILE_Flash_Load(bash.file_active)) LOG_Error("File %s flash load fault", bash.file_active->name);
          else LOG_Bash("File %s flash load success", bash.file_active->name);
          break;
        default: CMD_ArgvExit(2);
      }
    }
    case HASH_Mutex: { // FILE mutex {set|rst}
      CMD_Argc(3);
      switch(hash_djb2(argv[2])) {
        case HASH_Set: bash.file_active->lock = true; break;
        case HASH_Rst: case HASH_Reset: bash.file_active->lock = false; break;
        default: CMD_ArgvExit(2);
      }
      break;
    }
    case HASH_Copy: { // FILE copy {from|to} <file_name:str>
      CMD_Argc(4);
      FILE_t *file = CMD_FindFile(argv[3]);
      if(!file) return;
      switch(hash_djb2(argv[2])) {
        case HASH_To:
          if(FILE_Copy(file, bash.file_active)) LOG_Error("File copy fault");
          else LOG_Bash("File copy %s → %s success", bash.file_active->name, file->name);
          break;
        case HASH_From:
          if(FILE_Copy(bash.file_active, file)) LOG_Error("File copy fault");
          else LOG_Bash("File copy %s → %s success", file->name, bash.file_active->name);
          break;
        default: CMD_ArgvExit(2);
      }
      break;
    }
    case HASH_Print: { // FILE print {int8|uint8|int16|uint16|int32|uint32|struce} <limit:uint16>? <offset:uint16>?
      LOG_Bash("%02a %d", 50, bash.file_active->buffer);
      break;
    }
    // TODO: FILE (struct) print <limit:uint16>? <offset:uint16>?
    default: {
      #if(LOG_COLORS)
        LOG_Error("File command doesn't support "ANSI_YELLOW"%s"ANSI_END" option", argv[1]);
      #else
        LOG_Error("File command doesn't support %s option", argv[1]);
      #endif
    }
  }
}

//------------------------------------------------------------------------------------------------- UID

static void CMD_Uid(char **argv, uint16_t argc)
{
  CMD_Argc(1);
  uint8_t *uid = (uint8_t *)UID_BASE;
  LOG_Bash("UID %a%02x", 12, uid);
}

//------------------------------------------------------------------------------------------------- RTC
#ifdef RTC_H_

static RTC_Weekday_e RTC_Str2Weekday(const char *str)
{
  switch(hash_djb2(str)) {
    case RTC_Hash_Everyday: case RTC_Hash_Evd: case HASH_0: return RTC_Weekday_Everyday;
    case RTC_Hash_Monday: case RTC_Hash_Mon: case HASH_1: return RTC_Weekday_Monday;
    case RTC_Hash_Tuesday: case RTC_Hash_Tue: case HASH_2: return RTC_Weekday_Tuesday;
    case RTC_Hash_Wednesday: case RTC_Hash_Wed: case HASH_3: return RTC_Weekday_Wednesday;
    case RTC_Hash_Thursday: case RTC_Hash_Thu: case HASH_4: return RTC_Weekday_Thursday;
    case RTC_Hash_Friday: case RTC_Hash_Fri: case HASH_5: return RTC_Weekday_Friday;
    case RTC_Hash_Saturday: case RTC_Hash_Sat: case HASH_6: return RTC_Weekday_Saturday;
    case RTC_Hash_Sunday: case RTC_Hash_Sun: case HASH_7: return RTC_Weekday_Sunday;
    default: return RTC_Weekday_Error;
  }
}

static void CMD_Rtc(char **argv, uint16_t argc)
{
  CMD_Argc(1, 3);
  switch(argc) {
    case 1: { // RTC
      RTC_Datetime_t dt = RTC_Datetime();
      LOG_Bash("RTC %o %s", &dt, &DBG_Datetime, (char *)RtcWeekdays[dt.week_day]);
      break;
    }
    case 2: { // RTC rst|<timestamp:uint32>
      uint32_t argv1_hash = hash_djb2(argv[1]);
      if(argv1_hash == HASH_Rst || argv1_hash == HASH_Reset) {
        RTC_Reset();
        LOG_Bash("RTC reset");
        return;
      }
      if(str_is_u64(argv[1])) {
        LOG_ErrorParse(argv[1], "uint64_t");
        CMD_ArgvExit(1);
      }
      uint64_t stamp = str_to_int64(argv[1]);
      LOG_Bash("RTC preset timestamp");
      RTC_SetTimestamp(stamp);
      break;
    }
    case 3: { // RTC <date:str(YYYY-MM-DD)> <time:str(hh:mm:ss)>
      char *date = str_replace_chars(argv[1], "\"/,:+-_", ',');
      char *year_str = str_split(date, ',', 0);
      char *month_str = str_split(date, ',', 1);
      char *day_str = str_split(date, ',', 2);
      if(!str_is_u16(year_str) || !str_is_u16(month_str) || !str_is_u16(day_str)) {
        CMD_ArgvExit(1);
      }
      uint16_t year_nbr = str_to_int(year_str);
      if(year_nbr >= 2000) year_nbr -= 2000;
      uint16_t month_nbr = str_to_int(month_str);
      uint16_t day_nbr = str_to_int(day_str);
      if(year_nbr >= 100 || month_nbr == 0 || month_nbr > 12 || day_nbr == 0 || day_nbr > 31) {
        CMD_ArgvExit(1);
      }
      char *time = str_replace_chars(argv[2], "\"/,:+-_", ',');
      char *hour_str = str_split(time, ',', 0);
      char *minute_str = str_split(time, ',', 1);
      char *second_str = str_split(time, ',', 2);
      if(!str_is_u16(hour_str) || !str_is_u16(minute_str) || !str_is_u16(second_str)) {
        CMD_ArgvExit(2);
      }
      uint16_t hour_nbr = str_to_int(hour_str);
      if(hour_nbr == 24) hour_nbr = 0;
      uint16_t minute_nbr = str_to_int(minute_str);
      uint16_t second_nbr = str_to_int(second_str);
      if(hour_nbr >= 24 || minute_nbr >= 60 || second_nbr >= 60) {
        CMD_ArgvExit(2);
      }
      RTC_Datetime_t dt = {
        .year = year_nbr,
        .month = month_nbr,
        .month_day = day_nbr,
        .hour = hour_nbr,
        .minute = minute_nbr,
        .second = second_nbr
      };
      RTC_SetDatetime(&dt);
      LOG_Bash("RTC preset datatime");
      break;
    }
  }
}

static void CMD_Alarm(char **argv, uint16_t argc)
{
  CMD_Argc(2, 4);
  RTC_Alarm_e alarm_type;
  char alarm_char;
  uint32_t argv1_hash = hash_djb2(argv[1]);
  switch(argv1_hash) {
    case HASH_A: alarm_type = RTC_Alarm_A; alarm_char = 'A'; break;
    case HASH_B: alarm_type = RTC_Alarm_B; alarm_char = 'B'; break;
    default: CMD_ArgvExit(1);
  }
  switch(argc) {
    case 2: {
      RTC_Alarm_t alarm = RTC_Alarm(alarm_type);
      if(RTC_AlarmIsEnabled(alarm_type)) return LOG_Bash("Alarm %c %o", alarm_char, &alarm, &DBG_Alarm);
      else return LOG_Bash("Alarm %c disabled", alarm_char);
      break;
    }
    case 4: {
      RTC_Weekday_e weekday = RTC_Str2Weekday(argv[2]);
      if(weekday == RTC_Weekday_Error) {
        CMD_ArgvExit(2);
      }
      bool weekday_mask = weekday ? false : true;
      char *time = str_replace_chars(argv[3], "\"/,:+-_", ',');
      char *hour_str = str_split(time, ',', 0);
      char *minute_str = str_split(time, ',', 1);
      char *second_str = str_split(time, ',', 2);
      if(!str_is_u16(hour_str) || !str_is_u16(minute_str) || !str_is_u16(second_str)) {
        CMD_ArgvExit(3);
      }
      uint16_t hour_nbr = str_to_int(hour_str);
      if(hour_nbr == 24) hour_nbr = 0;
      uint16_t minute_nbr = str_to_int(minute_str);
      uint16_t second_nbr = str_to_int(second_str);
      if(hour_nbr >= 24 || minute_nbr >= 60 || second_nbr >= 60) {
        CMD_ArgvExit(3);
      }
      RTC_Alarm_t alarm = {
        .week = true,
        .day_mask = weekday_mask,
        .day = weekday,
        .hour_mask = false,
        .hour = hour_nbr,
        .minute_mask = false,
        .minute = minute_nbr,
        .second_mask = false,
        .second = second_nbr
      };
      RTC_AlarmEnable(alarm_type, &alarm);
      LOG_Bash("Alarm %c %o", alarm_char, &alarm, &DBG_Alarm);
      break;
    }
    default: CMD_WrongArgc(argv[0], argc);
  }
}

#endif
//------------------------------------------------------------------------------------------------- PWR

PWR_SleepMode_e PWR_StrSleepMode(const char *str)
{
  switch(hash_djb2(str)) {
    case PWR_Hash_Stop0: case HASH_0: return PWR_SleepMode_Stop0;
    case PWR_Hash_Stop: case PWR_Hash_Stop1: case HASH_1: return PWR_SleepMode_Stop1;
    case PWR_Hash_StandbySram: case PWR_Hash_Standbysram: case HASH_2: return PWR_SleepMode_StandbySRAM;
    case PWR_Hash_Standby: case HASH_3: return PWR_SleepMode_Standby;
    case PWR_Hash_Shutdown: case HASH_4: return PWR_SleepMode_Shutdown;
    default: return PWR_SleepMode_Error;
  }
}

static void CMD_Power(char **argv, uint16_t argc)
{
  CMD_Argc(2, 4);
  switch(hash_djb2(argv[1])) {
    case HASH_Sleep: {
      CMD_Argc(3, 4);
      PWR_SleepMode_e mode = PWR_StrSleepMode(argv[2]);
      if(mode == PWR_SleepMode_Error) CMD_ArgvExit(2);
      if(argc == 3) {
        if(bash.Sleep) bash.Sleep(mode);
        else PWR_Sleep(mode);
      }
      else if(argc == 4) {
        if(hash_djb2(argv[3]) == HASH_Now) PWR_Sleep(mode);
        else CMD_ArgvExit(3);
      }
      break;
    }
    case HASH_Reboot:
    case HASH_Restart:
    case HASH_Reset: {
      CMD_Argc(2, 3);
      if(argc == 2) {
        if(bash.Reset) bash.Reset();
        else DbgReset = true;
      }
      else if(argc == 3) {
        if(hash_djb2(argv[2]) == HASH_Now) PWR_Reset();
        else CMD_ArgvExit(2);
      }
      break;
    }
    default: CMD_ArgvExit(1);
  }
}

//------------------------------------------------------------------------------------------------- Addr

#if(STREAM_ADDRESS)
static void CMD_Addr(char **argv, uint16_t argc)
{
  CMD_Argc(1, 2);
  if(argc == 2) {
    stream->address = atoi(argv[1]);
    if(stream->Readdress) stream->Readdress(stream->address);
  }
  LOG_Bash("ADDR %u", stream->address);
}
#endif

//------------------------------------------------------------------------------------------------- Ping

static void CMD_Ping(char **argv, uint16_t argc)
{
  CMD_Argc(1);
  LOG_Bash("PING pong");
}

//------------------------------------------------------------------------------------------------- Trig

/**
 * @brief Get and clear current trigger code.
 * Returns `0` if no trigger is pending.
 * @return Trigger code or `0` if none.
 */
uint16_t TRIG_Event(void)
{
  if(bash.trig) {
    uint16_t trig = bash.trig;
    bash.trig = 0;
    return trig;
  }
  return 0;
}

/**
 * @brief Wait until any trigger occurs.
 * Function blocks until trigger is set, then clears it.
 * @return Trigger code that was received.
 */
uint16_t TRIG_Wait(void)
{
  while(!bash.trig) let();
  uint16_t trig = bash.trig;
  bash.trig = 0;
  return trig;
}

/**
 * @brief Wait until specific trigger code occurs.
 * Function blocks until `code` is received, then clears it.
 * @param[in] code Expected trigger code.
 */
void TRIG_WaitFor(uint16_t code)
{
  while(bash.trig != code) let();
  bash.trig = 0;
}

static void CMD_Trig(char **argv, uint16_t argc)
{
  CMD_Argc(1, 2);
  if(argc == 1) {
    bash.trig = 1;
  }
  else if(str_is_u16(argv[1])) {
    bash.trig = str_to_int(argv[1]);
  }
  else {
    LOG_ErrorParse(argv[1], "uint16_t");
    CMD_ArgvExit(1);
  }
}

//------------------------------------------------------------------------------------------------- Loop

/**
 * @brief Obsługuje komendy odczytane ze strumienia w systemie BASH.
 * Obsługuje wbudowane komendy oraz te dodane przez użytkownika.
 * @param stream Strumień wejściowy (np. UART) z komendami lub danymi.
 * @return `true` jeśli komenda została obsłużona, `false` w przypadku błędu lub nieznanej komendy.
 */
bool CMD_Loop(STREAM_t *stream)
{
  char **argv = NULL;
  uint16_t argc = STREAM_Read(stream, &argv);
  if(argc) {
    if(stream->data_mode) CMD_Data((uint8_t *)argv[0], argc, stream);
    else {
      uint32_t argv0_hash = hash_djb2(argv[0]);
      switch(argv0_hash) {
        case HASH_Ping: CMD_Ping(argv, argc); break;
        case HASH_Trig: CMD_Trig(argv, argc); break;
        case HASH_File: CMD_File(argv, argc, stream); break;
        case HASH_Uid: CMD_Uid(argv, argc); break;
        case HASH_Power: case HASH_Pwr: CMD_Power(argv, argc); break;
        #ifdef RTC_H_
          case HASH_Rtc: CMD_Rtc(argv, argc); break;
          case HASH_Alarm: CMD_Alarm(argv, argc); break;
        #endif
        #if(STREAM_ADDRESS)
          case HASH_Addr: break;
        #endif
        default: {
          for(uint8_t i = 0; i < bash.callbacks_count; i++) {
            if(argv0_hash == bash.callbacks_hash[i]) {
              bash.callbacks[i](argv, argc);
              return true;
            }
          }
          if(bash.callback_default) {
            bash.callback_default(argv, argc);
            return true;
          }
          else {
            #if(LOG_COLORS)
              LOG_Warning("Command " ANSI_ORANGE "%s" ANSI_END " not found", argv[0]);
            #else
              LOG_Warning("Command '%s' not found", argv[0]);
            #endif
            return false;
          }
        }
      }
    }
    return true;
  }
  return false;
}

//-------------------------------------------------------------------------------------------------

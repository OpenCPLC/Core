/** @file lib/sh/cmd.c */

#include "cmd.h"

//------------------------------------------------------------------------------------------------- Internal

static struct {
  MBB_t *files[CMD_MBB_LIMIT];
  uint32_t files_hash[CMD_MBB_LIMIT];
  uint8_t files_count;
  void (*callbacks[CMD_CALLBACK_LIMIT])(char **, uint16_t);
  uint32_t callbacks_hash[CMD_CALLBACK_LIMIT];
  uint8_t callbacks_count;
  void (*callback_default)(char **, uint16_t);
  bool flash_autosave;
  MBB_t *file_active;
  void (*Sleep)(PWR_SleepMode_t);
  void (*Reset)(void);
  uint16_t trig;
} cmd;

void CMD_AddFile(MBB_t *file)
{
  if(cmd.files_count >= CMD_MBB_LIMIT) {
    LOG_Error("CMD exceeded file limit (max:%u)", CMD_MBB_LIMIT);
    return;
  }
  cmd.files[cmd.files_count] = file;
  cmd.files_hash[cmd.files_count] = hash_djb2_ci(file->name);
  if(!cmd.files_count) {
    cmd.file_active = file;
  }
  cmd.files_count++;
  MBB_FlashLoad(file);
}

void CMD_AddCallback(void (*callback)(char **, uint16_t), char *argv0)
{
  if(!argv0 || !strlen(argv0)) {
    cmd.callback_default = callback;
    return;
  }
  if(cmd.callbacks_count >= CMD_CALLBACK_LIMIT) {
    LOG_Error("CMD exceeded callback limit (max:%u)", CMD_CALLBACK_LIMIT);
    return;
  }
  cmd.callbacks[cmd.callbacks_count] = callback;
  cmd.callbacks_hash[cmd.callbacks_count] = hash_djb2_ci(argv0);
  cmd.callbacks_count++;
}

void CMD_FlashAutosave(bool autosave)
{
  cmd.flash_autosave = autosave;
}

void CMD_SetSleep(void (*Sleep)(PWR_SleepMode_t))
{
  cmd.Sleep = Sleep;
}

void CMD_SetReset(void (*Reset)(void))
{
  cmd.Reset = Reset;
}

static inline void CMD_WrongCommand(char *name)
{
  LOG_Warning("Wrong " ANSI_ORANGE "%s" ANSI_END " command usage", name);
}

void CMD_WrongArgc(char *name, uint16_t argc)
{
  CMD_WrongCommand(name);
  LOG_Warning("Incorrect argument count: " ANSI_LIME "%u" ANSI_END, argc);
}

void CMD_WrongArgv(char *name, char *argv, uint16_t pos)
{
  CMD_WrongCommand(name);
  LOG_Warning("Invalid argument " ANSI_ORANGE "%s" ANSI_END " on " ANSI_LIME "%u" ANSI_END " position", argv, pos);
}

//------------------------------------------------------------------------------------------------- Data

static void CMD_Data(uint8_t *data, uint16_t size, STREAM_t *stream)
{
  stream->_packages--;
  LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " data pack:" ANSI_LIME "%d" ANSI_END,
    cmd.file_active->name, stream->_packages);
  cmd.file_active->lock = false;
  MBB_Append(cmd.file_active, data, size);
  if(!stream->_packages) {
    STREAM_ArgsMode(stream);
    if(cmd.flash_autosave) MBB_FlashSave(cmd.file_active);
  }
  else {
    cmd.file_active->lock = true;
  }
}

//------------------------------------------------------------------------------------------------- File

static MBB_t *CMD_FindFile(char *file_name)
{
  uint32_t file_hash = hash_djb2(file_name);
  for(uint8_t i = 0; i < cmd.files_count; i++) {
    if(file_hash == cmd.files_hash[i]) {
      return cmd.files[i];
    }
  }
  LOG_Warning("File " ANSI_ORANGE "%s" ANSI_END " not exist", (char *)file_name);
  return NULL;
}

static void CMD_AccessDenied(MBB_t *file)
{
  LOG_Warning("File %s access denied", (char *)file->name);
}

static void CMD_File(char **argv, uint16_t argc, STREAM_t *stream)
{
  if(!cmd.files_count) {
    LOG_Warning("No file added to cmd");
    return;
  }
  CMD_Argc(2, 4);
  switch(hash_djb2(argv[1])) {
    case HASH_List: { // FILE list
      CMD_Argc(2);
      const char *file_names[cmd.files_count];
      for(uint16_t i = 0; i < cmd.files_count; i++) {
        file_names[i] = cmd.files[i]->name;
      }
      LOG_Bash("File list: " ANSI_CREAM "%a %s" ANSI_END, cmd.files_count, file_names);
      break;
    }
    case HASH_Active:
    case HASH_Select: { // FILE select <file_name:str>
      CMD_Argc(3);
      MBB_t *file = CMD_FindFile(argv[2]);
      if(!file) return;
      cmd.file_active = file;
      LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " selected", cmd.file_active->name);
      break;
    }
    case HASH_Info: { // FILE info
      CMD_Argc(2);
      LOG_Bash("File %o", cmd.file_active, &MBB_Print);
      break;
    }
    case HASH_Clear: { // FILE clear
      CMD_Argc(2);
      if(MBB_Clear(cmd.file_active)) {
        CMD_AccessDenied(cmd.file_active);
        return;
      }
      LOG_Bash("File %s is empty", cmd.file_active->name);
      return;
    }
    case HASH_Save: { // FILE save <packages:uint16>?
      CMD_Argc(2, 3);
      if(MBB_Clear(cmd.file_active)) CMD_AccessDenied(cmd.file_active);
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
        stream->_packages = packages;
        cmd.file_active->lock = true;
        LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " save pack:" ANSI_LIME "%d" ANSI_END, cmd.file_active->name, stream->_packages);
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
      stream->_packages = packages;
      cmd.file_active->lock = true;
      LOG_Bash("File " ANSI_CREAM "%s" ANSI_END " append pack:" ANSI_LIME "%d" ANSI_END, cmd.file_active->name, stream->_packages);
      break;
    }
    case HASH_Load: { // FILE load <limit:uint16>? <offset:uint16>?
      CMD_Argc(2, 4);
      if(cmd.file_active->lock) CMD_AccessDenied(cmd.file_active);
      uint16_t limit = cmd.file_active->size;
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
      if(offset >= cmd.file_active->size) offset = 0;
      if(limit + offset > cmd.file_active->size) limit = cmd.file_active->size - offset;
      DBG_Data(&cmd.file_active->buffer[offset], limit);
      DBG_Enter();
      break;
    }
    case HASH_Flash: { // FILE flash {save|load}
      CMD_Argc(3);
      switch(hash_djb2(argv[2])) {
        case HASH_Save:
          if(MBB_FlashSave(cmd.file_active)) LOG_Error("File %s flash save fault", cmd.file_active->name);
          else LOG_Bash("File %s flash save success", cmd.file_active->name);
          break;
        case HASH_Load: case HASH_Reset:
          if(MBB_FlashLoad(cmd.file_active)) LOG_Error("File %s flash load fault", cmd.file_active->name);
          else LOG_Bash("File %s flash load success", cmd.file_active->name);
          break;
        default: CMD_ArgvExit(2);
      }
      break;
    }
    case HASH_Mutex: { // FILE mutex {set|rst}
      CMD_Argc(3);
      switch(hash_djb2(argv[2])) {
        case HASH_Set: cmd.file_active->lock = true; break;
        case HASH_Rst: case HASH_Reset: cmd.file_active->lock = false; break;
        default: CMD_ArgvExit(2);
      }
      break;
    }
    case HASH_Copy: { // FILE copy {from|to} <file_name:str>
      CMD_Argc(4);
      MBB_t *file = CMD_FindFile(argv[3]);
      if(!file) return;
      switch(hash_djb2(argv[2])) {
        case HASH_To:
          if(MBB_Copy(file, cmd.file_active)) LOG_Error("File copy fault");
          else LOG_Bash("File copy %s -> %s success", cmd.file_active->name, file->name);
          break;
        case HASH_From:
          if(MBB_Copy(cmd.file_active, file)) LOG_Error("File copy fault");
          else LOG_Bash("File copy %s -> %s success", file->name, cmd.file_active->name);
          break;
        default: CMD_ArgvExit(2);
      }
      break;
    }
    case HASH_Print: { // FILE print
      LOG_Bash("%02a %d", 50, cmd.file_active->buffer);
      break;
    }
    default: {
      LOG_Error("File command doesn't support " ANSI_YELLOW "%s" ANSI_END " option", argv[1]);
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

static RTC_Weekday_t RTC_Str2Weekday(const char *str)
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
      LOG_Bash("RTC preset datetime");
      break;
    }
  }
}

static void CMD_Alarm(char **argv, uint16_t argc)
{
  CMD_Argc(2, 4);
  RTC_Alarm_t alarm_type;
  char alarm_char;
  uint32_t argv1_hash = hash_djb2(argv[1]);
  switch(argv1_hash) {
    case HASH_A: alarm_type = RTC_Alarm_A; alarm_char = 'A'; break;
    case HASH_B: alarm_type = RTC_Alarm_B; alarm_char = 'B'; break;
    default: CMD_ArgvExit(1);
  }
  switch(argc) {
    case 2: {
      RTC_AlarmCfg_t alarm = RTC_Alarm(alarm_type);
      if(RTC_AlarmIsEnabled(alarm_type)) return LOG_Bash("Alarm %c %o", alarm_char, &alarm, &DBG_Alarm);
      else return LOG_Bash("Alarm %c disabled", alarm_char);
      break;
    }
    case 4: {
      RTC_Weekday_t weekday = RTC_Str2Weekday(argv[2]);
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
      RTC_AlarmCfg_t alarm = {
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

static PWR_SleepMode_t PWR_StrSleepMode(const char *str)
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
      PWR_SleepMode_t mode = PWR_StrSleepMode(argv[2]);
      if(mode == PWR_SleepMode_Error) CMD_ArgvExit(2);
      if(argc == 3) {
        if(cmd.Sleep) cmd.Sleep(mode);
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
        if(cmd.Reset) cmd.Reset();
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
static void CMD_Addr(char **argv, uint16_t argc, STREAM_t *stream)
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

uint16_t TRIG_Event(void)
{
  if(cmd.trig) {
    uint16_t trig = cmd.trig;
    cmd.trig = 0;
    return trig;
  }
  return 0;
}

uint16_t TRIG_Wait(void)
{
  while(!cmd.trig) let();
  uint16_t trig = cmd.trig;
  cmd.trig = 0;
  return trig;
}

void TRIG_WaitFor(uint16_t code)
{
  while(cmd.trig != code) let();
  cmd.trig = 0;
}

static void CMD_Trig(char **argv, uint16_t argc)
{
  CMD_Argc(1, 2);
  if(argc == 1) {
    cmd.trig = 1;
  }
  else if(str_is_u16(argv[1])) {
    cmd.trig = str_to_int(argv[1]);
  }
  else {
    LOG_ErrorParse(argv[1], "uint16_t");
    CMD_ArgvExit(1);
  }
}

//------------------------------------------------------------------------------------------------- Loop

bool CMD_Loop(STREAM_t *stream)
{
  char **argv = NULL;
  uint16_t argc = STREAM_Read(stream, &argv);
  if(argc) {
    if(stream->_data_mode) CMD_Data((uint8_t *)argv[0], argc, stream);
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
          case HASH_Addr: CMD_Addr(argv, argc, stream); break;
        #endif
        default: {
          for(uint8_t i = 0; i < cmd.callbacks_count; i++) {
            if(argv0_hash == cmd.callbacks_hash[i]) {
              cmd.callbacks[i](argv, argc);
              return true;
            }
          }
          if(cmd.callback_default) {
            cmd.callback_default(argv, argc);
            return true;
          }
          else {
            LOG_Warning("Command " ANSI_ORANGE "%s" ANSI_END " not found", argv[0]);
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
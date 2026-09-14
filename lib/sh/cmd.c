// lib/sh/cmd.c

#include "cmd.h"

#include <string.h>
#include "dbg.h"
#include "rtc.h"
#include "xstring.h"
#if(CMD_BOOT)
  #include "boot.h"
#endif

//------------------------------------------------------------------------------------------- State

static struct {
  MBB_t *mbbs[CMD_MBB_LIMIT];
  uint32_t mbbs_hash[CMD_MBB_LIMIT];
  uint8_t mbbs_count;
  CMD_Handler_t handlers[CMD_HANDLER_LIMIT];
  uint32_t handlers_hash[CMD_HANDLER_LIMIT];
  uint8_t handlers_count;
  CMD_Handler_t handler_default;
  bool autosave;
  void (*Sleep)(PWR_SleepMode_t);
  void (*Reset)(void);
  void (*Activity)(void);
  volatile uint16_t trig;
} cmd;

MBB_t *CmdMbb;

//------------------------------------------------------------------------------------------- Setup

void CMD_AddMemBuff(MBB_t *mbb)
{
  if(cmd.mbbs_count >= CMD_MBB_LIMIT) {
    LOG_Error("CMD exceeded MBB limit (max:%u)", CMD_MBB_LIMIT);
    return;
  }
  cmd.mbbs[cmd.mbbs_count] = mbb;
  cmd.mbbs_hash[cmd.mbbs_count] = hash_djb2_ci(mbb->name);
  if(!cmd.mbbs_count) CmdMbb = mbb;
  cmd.mbbs_count++;
  MBB_FlashLoad(mbb);
}

void CMD_AddCommand(const char *name, CMD_Handler_t handler)
{
  if(!name || !*name) {
    cmd.handler_default = handler;
    return;
  }
  uint32_t name_hash = hash_djb2_ci(name);
  for(uint8_t i = 0; i < cmd.handlers_count; i++) {
    if(cmd.handlers_hash[i] == name_hash) {
      LOG_Warning("CMD overwrite command " ANSI_ORANGE "%s" ANSI_END, name);
      cmd.handlers[i] = handler;
      return;
    }
  }
  if(cmd.handlers_count >= CMD_HANDLER_LIMIT) {
    LOG_Error("CMD exceeded handler limit (max:%u)", CMD_HANDLER_LIMIT);
    return;
  }
  cmd.handlers[cmd.handlers_count] = handler;
  cmd.handlers_hash[cmd.handlers_count] = name_hash;
  cmd.handlers_count++;
}

void CMD_SetAutosave(bool autosave) { cmd.autosave = autosave; }
void CMD_SetSleep(void (*Sleep)(PWR_SleepMode_t)) { cmd.Sleep = Sleep; }
void CMD_SetReset(void (*Reset)(void)) { cmd.Reset = Reset; }
void CMD_SetActivity(void (*Activity)(void)) { cmd.Activity = Activity; }

//------------------------------------------------------------------------------------------ Errors

static void wrong_command(const char *name)
{
  LOG_Warning("Wrong " ANSI_ORANGE "%s" ANSI_END " command usage", name);
}

void CMD_WrongArgc(const char *name, uint16_t argc)
{
  wrong_command(name);
  LOG_Warning("Incorrect argument count: " ANSI_LIME "%u" ANSI_END, argc);
}

void CMD_WrongArgv(const char *name, const char *argv, uint16_t pos)
{
  wrong_command(name);
  LOG_Warning("Invalid argument " ANSI_ORANGE "%s" ANSI_END
    " on " ANSI_LIME "%u" ANSI_END " position", argv, pos);
}

// Parse `argv[pos]` as `uint16_t`, `false` after logging the parse error
static bool arg_u16(char **argv, uint16_t pos, uint16_t *value)
{
  if(!str_is_u16(argv[pos])) {
    LOG_ErrorParse(argv[pos], "uint16_t");
    return false;
  }
  *value = (uint16_t)str_to_int(argv[pos]);
  return true;
}

//--------------------------------------------------------------------------------------------- MBB

// One package of a binary transfer started by `mbb save` or `mbb append`
static void mbb_data(uint8_t *data, uint16_t size, STREAM_t *stream)
{
  stream->_packages--;
  LOG_Bash("MBB " ANSI_CREAM "%s" ANSI_END " data pack:" ANSI_LIME "%d" ANSI_END,
    CmdMbb->name, stream->_packages);
  CmdMbb->lock = false;
  MBB_Append(CmdMbb, data, size);
  if(stream->_packages) {
    CmdMbb->lock = true;
    return;
  }
  STREAM_ArgsMode(stream);
  if(cmd.autosave) MBB_FlashSave(CmdMbb);
}

static MBB_t *mbb_find(const char *name)
{
  uint32_t hash = hash_djb2_ci(name);
  for(uint8_t i = 0; i < cmd.mbbs_count; i++) {
    if(hash == cmd.mbbs_hash[i]) return cmd.mbbs[i];
  }
  LOG_Warning("MBB " ANSI_ORANGE "%s" ANSI_END " not exist", name);
  return NULL;
}

static void mbb_denied(void)
{
  LOG_Warning("MBB %s access denied", CmdMbb->name);
}

// Arm a binary transfer of `packages` frames into the active buffer
static void mbb_receive(STREAM_t *stream, uint16_t packages, const char *verb)
{
  STREAM_DataMode(stream);
  stream->_packages = packages ? packages : 1;
  CmdMbb->lock = true;
  LOG_Bash("MBB " ANSI_CREAM "%s" ANSI_END " %s pack:" ANSI_LIME "%d" ANSI_END,
    CmdMbb->name, verb, stream->_packages);
}

static void cmd_mbb(char **argv, uint16_t argc, STREAM_t *stream)
{
  if(!cmd.mbbs_count) {
    LOG_Warning("No MBB added to cmd");
    return;
  }
  CMD_Argc(2, 4);
  switch(hash_djb2_ci(argv[1])) {
    case HASH_List: { // mbb list
      CMD_Argc(2);
      const char *names[cmd.mbbs_count];
      for(uint16_t i = 0; i < cmd.mbbs_count; i++) names[i] = cmd.mbbs[i]->name;
      LOG_Bash("MBB list: " ANSI_CREAM "%a %s" ANSI_END, cmd.mbbs_count, names);
      break;
    }
    case HASH_Active:
    case HASH_Select: { // mbb select <name>
      CMD_Argc(3);
      MBB_t *mbb = mbb_find(argv[2]);
      if(!mbb) return;
      CmdMbb = mbb;
      LOG_Bash("MBB " ANSI_CREAM "%s" ANSI_END " selected", CmdMbb->name);
      break;
    }
    case HASH_Info: // mbb info
      CMD_Argc(2);
      LOG_Bash("MBB %o", CmdMbb, &MBB_Print);
      break;
    case HASH_Clear: // mbb clear
      CMD_Argc(2);
      if(MBB_Clear(CmdMbb)) mbb_denied();
      else LOG_Bash("MBB %s is empty", CmdMbb->name);
      break;
    case HASH_Save: { // mbb save [packages]
      CMD_Argc(2, 3);
      if(MBB_Clear(CmdMbb)) {
        mbb_denied();
        return;
      }
      uint16_t packages = 1;
      if(argc == 3 && !arg_u16(argv, 2, &packages)) CMD_ArgvExit(2);
      mbb_receive(stream, packages, "save");
      break;
    }
    case HASH_Append: { // mbb append [packages]
      CMD_Argc(2, 3);
      uint16_t packages = 1;
      if(argc == 3 && !arg_u16(argv, 2, &packages)) CMD_ArgvExit(2);
      mbb_receive(stream, packages, "append");
      break;
    }
    case HASH_Load: { // mbb load [limit] [offset]
      CMD_Argc(2, 4);
      uint16_t limit = CmdMbb->size;
      uint16_t offset = 0;
      if(argc >= 3 && !arg_u16(argv, 2, &limit)) CMD_ArgvExit(2);
      if(argc == 4 && !arg_u16(argv, 3, &offset)) CMD_ArgvExit(3);
      if(offset >= CmdMbb->size) offset = 0;
      if(limit + offset > CmdMbb->size) limit = CmdMbb->size - offset;
      DBG_Data(&CmdMbb->buffer[offset], limit);
      DBG_Enter();
      break;
    }
    case HASH_Flash: // mbb flash save|load
      CMD_Argc(3);
      switch(hash_djb2_ci(argv[2])) {
        case HASH_Save:
          if(MBB_FlashSave(CmdMbb)) LOG_Error("MBB %s flash save fault", CmdMbb->name);
          else LOG_Bash("MBB %s flash save success", CmdMbb->name);
          break;
        case HASH_Load: case HASH_Reset:
          if(MBB_FlashLoad(CmdMbb)) LOG_Error("MBB %s flash load fault", CmdMbb->name);
          else LOG_Bash("MBB %s flash load success", CmdMbb->name);
          break;
        default: CMD_ArgvExit(2);
      }
      break;
    case HASH_Mutex: // mbb mutex set|rst
      CMD_Argc(3);
      switch(hash_djb2_ci(argv[2])) {
        case HASH_Set: CmdMbb->lock = true; break;
        case HASH_Rst: case HASH_Reset: CmdMbb->lock = false; break;
        default: CMD_ArgvExit(2);
      }
      break;
    case HASH_Copy: { // mbb copy from|to <name>
      CMD_Argc(4);
      MBB_t *mbb = mbb_find(argv[3]);
      if(!mbb) return;
      MBB_t *dst = CmdMbb, *src = mbb;
      switch(hash_djb2_ci(argv[2])) {
        case HASH_To: dst = mbb; src = CmdMbb; break;
        case HASH_From: break;
        default: CMD_ArgvExit(2);
      }
      if(MBB_Copy(dst, src)) LOG_Error("MBB copy fault");
      else LOG_Bash("MBB copy %s -> %s success", src->name, dst->name);
      break;
    }
    case HASH_Print: // mbb print
      CMD_Argc(2);
      LOG_Bash("%02a %d", CmdMbb->size / 2, CmdMbb->buffer);
      break;
    default:
      LOG_Error("MBB command doesn't support " ANSI_YELLOW "%s" ANSI_END " option", argv[1]);
  }
}

//--------------------------------------------------------------------------------------------- UID

static void cmd_uid(char **argv, uint16_t argc)
{
  CMD_Argc(1);
  LOG_Bash("UID %a%02x", 12, (const uint8_t *)UID_BASE);
}

//--------------------------------------------------------------------------------------------- RTC

static RTC_Weekday_t weekday_of(const char *str)
{
  switch(hash_djb2_ci(str)) {
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

// Three numbers separated by any of `"/,:+-_`, the date and time shape of the shell
static bool three_fields(const char *arg, uint16_t *a, uint16_t *b, uint16_t *c)
{
  char *text = str_replace_chars(arg, "\"/,:+-_", ',');
  char *first = str_split(text, ',', 0);
  char *second = str_split(text, ',', 1);
  char *third = str_split(text, ',', 2);
  if(!str_is_u16(first) || !str_is_u16(second) || !str_is_u16(third)) return false;
  *a = (uint16_t)str_to_int(first);
  *b = (uint16_t)str_to_int(second);
  *c = (uint16_t)str_to_int(third);
  return true;
}

// `hh:mm:ss` with `24:00:00` folded to midnight
static bool time_fields(const char *arg, uint16_t *hour, uint16_t *minute, uint16_t *second)
{
  if(!three_fields(arg, hour, minute, second)) return false;
  if(*hour == 24) *hour = 0;
  return *hour < 24 && *minute < 60 && *second < 60;
}

static void cmd_rtc(char **argv, uint16_t argc)
{
  CMD_Argc(1, 3);
  if(argc == 1) { // rtc
    RTC_Datetime_t dt = RTC_Datetime();
    LOG_Bash("RTC %o %s", &dt, &DBG_Datetime, RtcWeekdays[dt.week_day]);
    return;
  }
  if(argc == 2) { // rtc rst|<timestamp>
    uint32_t hash = hash_djb2_ci(argv[1]);
    if(hash == HASH_Rst || hash == HASH_Reset) {
      RTC_Reset();
      LOG_Bash("RTC reset");
      return;
    }
    if(!str_is_u64(argv[1])) {
      LOG_ErrorParse(argv[1], "uint64_t");
      CMD_ArgvExit(1);
    }
    if(RTC_SetTimestamp(str_to_int64(argv[1]))) LOG_Error("RTC refused write, timestamp dropped");
    else LOG_Bash("RTC preset timestamp");
    return;
  }
  // rtc <YYYY-MM-DD> <hh:mm:ss>
  uint16_t year, month, day, hour, minute, second;
  if(!three_fields(argv[1], &year, &month, &day)) CMD_ArgvExit(1);
  if(year >= 2000) year -= 2000;
  if(year >= 100 || month == 0 || month > 12 || day == 0 || day > 31) CMD_ArgvExit(1);
  if(!time_fields(argv[2], &hour, &minute, &second)) CMD_ArgvExit(2);
  RTC_Datetime_t dt = {
    .year = year, .month = month, .month_day = day,
    .hour = hour, .minute = minute, .second = second
  };
  if(RTC_SetDatetime(&dt)) LOG_Error("RTC refused write, datetime dropped");
  else LOG_Bash("RTC preset datetime");
}

static void cmd_alarm(char **argv, uint16_t argc)
{
  CMD_Argc(2, 4);
  RTC_Alarm_t alarm;
  char letter;
  switch(hash_djb2_ci(argv[1])) {
    case HASH_A: alarm = RTC_Alarm_A; letter = 'A'; break;
    case HASH_B: alarm = RTC_Alarm_B; letter = 'B'; break;
    default: CMD_ArgvExit(1);
  }
  if(argc == 2) { // alarm a|b
    RTC_AlarmCfg_t cfg = RTC_Alarm(alarm);
    if(RTC_AlarmIsEnabled(alarm)) LOG_Bash("Alarm %c %o", letter, &cfg, &DBG_Alarm);
    else LOG_Bash("Alarm %c disabled", letter);
    return;
  }
  CMD_Argc(4); // alarm a|b <weekday> <hh:mm:ss>
  RTC_Weekday_t weekday = weekday_of(argv[2]);
  if(weekday == RTC_Weekday_Error) CMD_ArgvExit(2);
  uint16_t hour, minute, second;
  if(!time_fields(argv[3], &hour, &minute, &second)) CMD_ArgvExit(3);
  RTC_AlarmCfg_t cfg = {
    .week = true, .day_mask = !weekday, .day = weekday,
    .hour = hour, .minute = minute, .second = second
  };
  RTC_AlarmEnable(alarm, &cfg);
  LOG_Bash("Alarm %c %o", letter, &cfg, &DBG_Alarm);
}

//--------------------------------------------------------------------------------------------- PWR

static PWR_SleepMode_t sleep_mode_of(const char *str)
{
  switch(hash_djb2_ci(str)) {
    case PWR_Hash_Stop0: case HASH_0: return PWR_SleepMode_Stop0;
    case PWR_Hash_Stop: case PWR_Hash_Stop1: case HASH_1: return PWR_SleepMode_Stop1;
    case PWR_Hash_StandbySram: case PWR_Hash_Standbysram: case HASH_2:
      return PWR_SleepMode_StandbySRAM;
    case PWR_Hash_Standby: case HASH_3: return PWR_SleepMode_Standby;
    case PWR_Hash_Shutdown: case HASH_4: return PWR_SleepMode_Shutdown;
    default: return PWR_SleepMode_Error;
  }
}

static void cmd_power(char **argv, uint16_t argc)
{
  CMD_Argc(2, 4);
  switch(hash_djb2_ci(argv[1])) {
    case HASH_Sleep: { // pwr sleep <mode> [now]
      CMD_Argc(3, 4);
      PWR_SleepMode_t mode = sleep_mode_of(argv[2]);
      if(mode == PWR_SleepMode_Error) CMD_ArgvExit(2);
      if(argc == 4) {
        if(hash_djb2_ci(argv[3]) != HASH_Now) CMD_ArgvExit(3);
        PWR_Sleep(mode);
      }
      else if(cmd.Sleep) cmd.Sleep(mode);
      else PWR_Sleep(mode);
      break;
    }
    case HASH_Reboot:
    case HASH_Restart:
    case HASH_Reset: // pwr reset [now]
      CMD_Argc(2, 3);
      if(argc == 3) {
        if(hash_djb2_ci(argv[2]) != HASH_Now) CMD_ArgvExit(2);
        PWR_Reset();
      }
      else if(cmd.Reset) cmd.Reset();
      else DbgReset = true;
      break;
    default: CMD_ArgvExit(1);
  }
}

//-------------------------------------------------------------------------------------------- Addr

#if(STREAM_ADDRESS)
static void cmd_addr(char **argv, uint16_t argc, STREAM_t *stream)
{
  CMD_Argc(1, 2);
  if(argc == 2) {
    uint16_t address;
    if(!arg_u16(argv, 1, &address)) CMD_ArgvExit(1);
    stream->address = (uint8_t)address;
    if(stream->Readdress) stream->Readdress(stream->address);
  }
  LOG_Bash("ADDR %u", stream->address);
}
#endif

//-------------------------------------------------------------------------------------------- Trig

uint16_t TRIG_Event(void)
{
  uint16_t trig = cmd.trig;
  cmd.trig = 0;
  return trig;
}

uint16_t TRIG_Wait(void)
{
  while(!cmd.trig) let();
  return TRIG_Event();
}

void TRIG_WaitFor(uint16_t code)
{
  while(cmd.trig != code) let();
  cmd.trig = 0;
}

static void cmd_trig(char **argv, uint16_t argc)
{
  CMD_Argc(1, 2);
  uint16_t code = 1;
  if(argc == 2 && !arg_u16(argv, 1, &code)) CMD_ArgvExit(1);
  cmd.trig = code;
}

//-------------------------------------------------------------------------------------------- Step

bool CMD_Step(STREAM_t *stream)
{
  char **argv = NULL;
  uint16_t argc = STREAM_Read(stream, &argv);
  if(!argc) return false;
  if(cmd.Activity) cmd.Activity(); // a line arrived, whatever it holds
  if(stream->_data_mode) {
    mbb_data((uint8_t *)argv[0], argc, stream);
    return true;
  }
  uint32_t hash = hash_djb2_ci(argv[0]);
  switch(hash) {
    case HASH_Ping:
      if(argc != 1) CMD_WrongArgc(argv[0], argc);
      else LOG_Bash("PING pong");
      return true;
    case HASH_Trig: cmd_trig(argv, argc); return true;
    case HASH_Mbb: cmd_mbb(argv, argc, stream); return true;
    case HASH_Uid: cmd_uid(argv, argc); return true;
    case HASH_Power: case HASH_Pwr: cmd_power(argv, argc); return true;
    case HASH_Rtc: cmd_rtc(argv, argc); return true;
    case HASH_Alarm: cmd_alarm(argv, argc); return true;
    #if(CMD_BOOT)
    case HASH_Boot: BOOT_Bash(argv, argc); return true;
    #endif
    #if(STREAM_ADDRESS)
    case HASH_Addr: cmd_addr(argv, argc, stream); return true;
    #endif
    default: break;
  }
  for(uint8_t i = 0; i < cmd.handlers_count; i++) {
    if(hash == cmd.handlers_hash[i]) {
      cmd.handlers[i](argv, argc);
      return true;
    }
  }
  if(cmd.handler_default) {
    cmd.handler_default(argv, argc);
    return true;
  }
  LOG_Warning("Command " ANSI_ORANGE "%s" ANSI_END " not found", argv[0]);
  return false;
}

//-------------------------------------------------------------------------------------------------

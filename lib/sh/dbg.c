// lib/sh/dbg.c

#include "dbg.h"
#include "cmd.h"
#include "log.h"
#include "pwr.h"

//------------------------------------------------------------------------------------------------- Basic

static uint8_t dbg_buffer_rx[DBG_RX_SIZE];
static uint8_t dbg_buffer_tx[DBG_TX_SIZE];

static BUFF_t dbg_buff = {
  .memory = dbg_buffer_rx,
  .size = DBG_RX_SIZE,
  .console_mode = true
};

static MBB_t dbg_file = {
  .name = "debug",
  .buffer = dbg_buffer_tx,
  .limit = DBG_TX_SIZE
};

UART_t *DbgUart;
MBB_t *DbgFile = &dbg_file;
bool DbgEcho = true;

void DBG_SwitchMode(bool data_mode)
{
  if(data_mode) {
    DbgUart->buff->console_mode = false;
    DbgEcho = false;
    UART_SetTimeout(DbgUart, DBG_DATAMODE_TIMEOUT);
  }
  else {
    DbgUart->buff->console_mode = true;
    DbgEcho = true;
    UART_SetTimeout(DbgUart, 0);
  }
}

STREAM_t dbg_stream = {
  .name = "debug",
  .modify = STREAM_Modify_Lowercase,
  .Size = DBG_Size,
  .Read = DBG_ReadString,
  .SwitchMode = DBG_SwitchMode
};

void DBG_Init(UART_t *uart)
{
  DbgUart = uart;
  DbgUart->buff = &dbg_buff;
  UART_Init(DbgUart);
}

volatile bool DbgReset;

#if(DBG_ECHO_MODE)

char EchoValue;
bool EchoEnter = false;
bool EchoInput = false;

void DBG_Echo(void)
{
  while(BUFF_Echo(&dbg_buff, &EchoValue)) {
    if(EchoValue == '\b' || EchoValue == 0x7F) {
      BUFF_Pop(&dbg_buff, NULL);
      if(BUFF_Pop(&dbg_buff, NULL)) DBG_Char(0x7F);
      continue;
    }
    if(EchoValue == '\n' || EchoValue == '\f') {
      LogPrintFlag = true;
      if(!EchoEnter) {
        DBG_String(EchoValue == '\n' ? ANSI_GREEN "^E" ANSI_END : ANSI_RED "^C" ANSI_END);
        DBG_Enter();
      }
      EchoEnter = true;
      EchoInput = false;
      continue;
    }
    if(!EchoInput) {
      DBG_String(ANSI_ORANGE ">> " ANSI_END);
      LogPrintFlag = false;
    }
    EchoInput = true;
    EchoEnter = false;
    DBG_Char(EchoValue);
  }
}

bool BUFF_EchoIdle(BUFF_t *buff)
{
  return buff->echo == buff->head;
}

#endif

void DBG_Loop(void)
{
  while(1) {
    #if(DBG_ECHO_MODE)
      DBG_Echo();
    #endif
    if(BUFF_EchoIdle(&dbg_buff)) {
      CMD_Step(&dbg_stream);
    }
    if(UART_IsFree(DbgUart)) {
      heap_clear();
      if(DbgFile->size) {
        uint8_t *buffer = (uint8_t *)heap_new(DbgFile->size);
        memcpy(buffer, DbgFile->buffer, DbgFile->size);
        UART_Send(DbgUart, buffer, DbgFile->size);
        MBB_Clear(DbgFile);
      }
      else if(DbgReset && UART_SendCompleted(DbgUart)) PWR_Reset();
    }
    let();
  }
}

//-------------------------------------------------------------------------------------------------

void DBG_Wait(void)
{
  while(UART_IsBusy(DbgUart)) let();
}

void DBG_WaitBlock(void)
{
  while(UART_IsBusy(DbgUart)) __NOP();
}

void DBG_Send(uint8_t *array, uint16_t length)
{
  DBG_Wait();
  UART_Send(DbgUart, array, length);
  DBG_Wait();
}

void DBG_SendFile(MBB_t *file)
{
  DBG_Send(file->buffer, file->size);
}

void DBG_DefaultFile(void)
{
  DbgFile = &dbg_file;
}

void DBG_SetFile(MBB_t *file)
{
  DbgFile = file;
}

//------------------------------------------------------------------------------------------------- Read

uint16_t DBG_Size(void)
{
  return UART_Size(DbgUart);
}

uint16_t DBG_Read(uint8_t *array)
{
  return UART_Read(DbgUart, array);
}

char *DBG_ReadString(void)
{
  return UART_ReadString(DbgUart);
}

//------------------------------------------------------------------------------------------------- Add

int32_t DBG_Char(uint8_t data) { return MBB_Char(DbgFile, data); }
int32_t DBG_Char16(uint16_t data) { return MBB_Char16(DbgFile, data); }
int32_t DBG_Char32(uint32_t data) { return MBB_Char32(DbgFile, data); }
int32_t DBG_Char64(uint64_t data) { return MBB_Char64(DbgFile, data); }
int32_t DBG_Data(uint8_t *array, uint16_t length) { return MBB_Data(DbgFile, array, length); }
int32_t DBG_String(char *string) { return MBB_String(DbgFile, string); }
int32_t DBG_Enter(void) { return MBB_Enter(DbgFile); }
int32_t DBG_DropLastLine(void) { return MBB_DropLastLine(DbgFile); }
int32_t DBG_Bool(bool value) { return MBB_Bool(DbgFile, value); }
int32_t DBG_Int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space) { return MBB_Int(DbgFile, nbr, base, sign, fill_zero, fill_space); }
int32_t DBG_Float(float nbr, uint8_t accuracy) { return MBB_Float(DbgFile, nbr, accuracy, 1); }
int32_t DBG_FloatSpace(float nbr, uint8_t accuracy, uint8_t fill_space) { return MBB_Float(DbgFile, nbr, accuracy, fill_space); }
int32_t DBG_Dec(int64_t nbr) { return MBB_Dec(DbgFile, nbr); }
int32_t DBG_uDec(uint64_t nbr) { return MBB_uDec(DbgFile, nbr); }
int32_t DBG_Hex8(uint8_t nbr) { return MBB_Hex8(DbgFile, nbr); }
int32_t DBG_Hex16(uint16_t nbr) { return MBB_Hex16(DbgFile, nbr); }
int32_t DBG_Hex32(uint32_t nbr) { return MBB_Hex32(DbgFile, nbr); }
int32_t DBG_Bin8(uint8_t nbr) { return MBB_Bin8(DbgFile, nbr); }

int32_t DBG_Date(RTC_Datetime_t *datetime) { return MBB_Date(DbgFile, datetime); }
int32_t DBG_Time(RTC_Datetime_t *datetime) { return MBB_Time(DbgFile, datetime); }
int32_t DBG_TimeMs(RTC_Datetime_t *datetime) { return MBB_TimeMs(DbgFile, datetime); }
int32_t DBG_Datetime(RTC_Datetime_t *datetime) { return MBB_Datetime(DbgFile, datetime); }
int32_t DBG_DatetimeMs(RTC_Datetime_t *datetime) { return MBB_DatetimeMs(DbgFile, datetime); }
int32_t DBG_AlarmTime(RTC_AlarmCfg_t *alarm) { return MBB_AlarmTime(DbgFile, alarm); }
int32_t DBG_Alarm(RTC_AlarmCfg_t *alarm) { return MBB_Alarm(DbgFile, alarm); }

int32_t MBB_Print(MBB_t *mbb)
{
  int32_t size = 0;
  size += DBG_String(ANSI_CREAM);
  size += DBG_String((char *)mbb->name);
  size += DBG_String(ANSI_END);
  size += DBG_Char(' ');
  size += DBG_uDec(mbb->size);
  size += DBG_String(ANSI_GREY "/" ANSI_END);
  size += DBG_uDec(mbb->limit);
  if(mbb->lock) size += DBG_String(" mutex");
  if(mbb->flash_page) {
    size += DBG_String(ANSI_GREY " flash:" ANSI_END);
    size += DBG_uDec(mbb->flash_page);
  }
  return size;
}

int32_t MBB_PrintContent(MBB_t *mbb)
{
  int32_t size = 0;
  uint8_t *byte = mbb->buffer;
  uint16_t count = mbb->size;
  while(count) {
    size += DBG_Hex8(*byte);
    size += DBG_Char(' ');
    count--;
    byte++;
  }
  return size;
}

//-------------------------------------------------------------------------------------------------
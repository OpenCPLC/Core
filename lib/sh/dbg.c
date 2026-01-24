#include "dbg.h"
#include "cmd.h"
#include "log.h"

//------------------------------------------------------------------------------------------------- Basic

static uint8_t dbg_buffer_rx[DBG_RX_SIZE];
static uint8_t dbg_buffer_tx[DBG_TX_SIZE];

static BUFF_t dbg_buff = {
  .memory = dbg_buffer_rx,
  .size = DBG_RX_SIZE,
  .console_mode = true
};

static FILE_t dbg_file = {
  .name = "debug",
  .buffer = dbg_buffer_tx,
  .limit = DBG_TX_SIZE
};

UART_t *DbgUart;
FILE_t *DbgFile = &dbg_file;
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
bool EchoInput  = false;

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
        #if(LOG_COLORS)
          DBG_String(EchoValue == '\n' ? ANSI_GREEN "^E" ANSI_END : ANSI_RED "^C" ANSI_END);
        #else
          DBG_String(EchoValue == '\n' ? "^E" : "^C");
        #endif
        DBG_Enter();
      }
      EchoEnter = true;
      EchoInput = false;
      continue;
    }
    if(!EchoInput) {
      #if(LOG_COLORS)
        DBG_String(ANSI_ORANGE">> "ANSI_END);
      #else
        DBG_String(">> ");
      #endif
      LogPrintFlag = false;
    }
    EchoInput = true;
    EchoEnter = false;
    DBG_Char(EchoValue);
  }
}
#endif

void DBG_Loop(void)
{
  while(1) {
    #if(DBG_ECHO_MODE)
      DBG_Echo();
    #endif
    CMD_Loop(&dbg_stream);
    if(UART_IsFree(DbgUart)) {
      heap_clear();
      if(DbgFile->size) {
        uint8_t *buffer = (uint8_t *)heap_new(DbgFile->size);
        memcpy(buffer, DbgFile->buffer, DbgFile->size);
        UART_Send(DbgUart, buffer, DbgFile->size);
        FILE_Clear(DbgFile);
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

void DBG_SendFile(FILE_t *file)
{
  DBG_Send(file->buffer, file->size);
}

void DBG_DefaultFile(void)
{
  DbgFile = &dbg_file;
}

void DBG_SetFile(FILE_t *file)
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

int32_t DBG_Char(uint8_t data) { return FILE_Char(DbgFile, data); }
int32_t DBG_Char16(uint16_t data) { return FILE_Char16(DbgFile, data); }
int32_t DBG_Char32(uint32_t data) { return FILE_Char32(DbgFile, data); }
int32_t DBG_Char64(uint64_t data) { return FILE_Char64(DbgFile, data); }
int32_t DBG_Data(uint8_t *array, uint16_t length) { return FILE_Data(DbgFile, array, length); }
int32_t DBG_String(char *string) { return FILE_String(DbgFile, string); }
int32_t DBG_Enter(void) { return FILE_Enter(DbgFile); }
int32_t DBG_DropLastLine(void) { return FILE_DropLastLine(DbgFile); }
int32_t DBG_Bool(bool value) { return FILE_Bool(DbgFile, value); }
int32_t DBG_Int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space) { return FILE_Int(DbgFile, nbr, base, sign, fill_zero, fill_space); }
int32_t DBG_Float(float nbr, uint8_t accuracy) { return FILE_Float(DbgFile, nbr, accuracy, 1); }
int32_t DBG_FloatSpace(float nbr, uint8_t accuracy, uint8_t fill_space) { return FILE_Float(DbgFile, nbr, accuracy, fill_space); }
int32_t DBG_Dec(int64_t nbr) { return FILE_Dec(DbgFile, nbr); }
int32_t DBG_uDec(uint64_t nbr) { return FILE_uDec(DbgFile, nbr); }
int32_t DBG_Hex8(uint8_t nbr) { return FILE_Hex8(DbgFile, nbr); }
int32_t DBG_Hex16(uint16_t nbr) { return FILE_Hex16(DbgFile, nbr); }
int32_t DBG_Hex32(uint32_t nbr) { return FILE_Hex32(DbgFile, nbr); }
int32_t DBG_Bin8(uint8_t nbr) { return FILE_Bin8(DbgFile, nbr); }

int32_t DBG_Date(RTC_Datetime_t *datetime) { return FILE_Date(DbgFile, datetime); }
int32_t DBG_Time(RTC_Datetime_t *datetime) { return FILE_Time(DbgFile, datetime); }
int32_t DBG_TimeMs(RTC_Datetime_t *datetime) { return FILE_TimeMs(DbgFile, datetime); }
int32_t DBG_Datetime(RTC_Datetime_t *datetime) { return FILE_Datetime(DbgFile, datetime); }
int32_t DBG_DatetimeMs(RTC_Datetime_t *datetime) { return FILE_DatetimeMs(DbgFile, datetime); }
int32_t DBG_AlarmTime(RTC_Alarm_t *alarm) { return FILE_AlarmTime(DbgFile, alarm); }
int32_t DBG_Alarm(RTC_Alarm_t *alarm) { return FILE_Alarm(DbgFile, alarm); }

int32_t DBG_File(FILE_t *file)
{
  int32_t size = 0;
  #if(LOG_COLORS)
    size += DBG_String(ANSI_CREAM);
  #endif
  size += DBG_String((char *)file->name);
  #if(LOG_COLORS)
    size += DBG_String(ANSI_END);
  #endif
  size += DBG_Char(' ');
  size += DBG_uDec(file->size);
  #if(LOG_COLORS)
    size += DBG_String(ANSI_GREY"/"ANSI_END);
  #else
    size += DBG_Char('/');
  #endif
  size += DBG_uDec(file->limit);
  if(file->lock) size += DBG_String(" mutex");
  if(file->flash_page) {
    #if(LOG_COLORS)
      size += DBG_String(ANSI_GREY" flash:"ANSI_END);
    #else
      size += DBG_String(" flash:");
    #endif
    size += DBG_uDec(file->flash_page);
  }
  return size;
}

//-------------------------------------------------------------------------------------------------
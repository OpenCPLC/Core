// lib/sh/dbg.c

#include "dbg.h"

#include <string.h>
#include "cmd.h"
#include "heap.h"
#include "log.h"
#include "pwr.h"

// The whole batch is copied out through one allocation, so the heap has to hold it
// with room left for the block header
_Static_assert(HEAP_SIZE > DBG_TX_SIZE + 128, "HEAP_SIZE must exceed DBG_TX_SIZE");

//----------------------------------------------------------------------------------------- Console

static uint8_t rx_memory[DBG_RX_SIZE];
static uint8_t tx_memory[DBG_TX_SIZE];

static BUFF_t rx_buff = { .memory = rx_memory, .size = DBG_RX_SIZE, .console_mode = true };
static MBB_t tx_file = { .name = "debug", .buffer = tx_memory, .limit = DBG_TX_SIZE };

UART_t *DbgUart;
MBB_t *DbgFile = &tx_file;
volatile bool DbgReset;
bool DbgEcho = true;

// Binary transfer: no console parsing, no echo, frames closed by the port timeout
static void switch_mode(bool data_mode)
{
  DbgUart->buff->console_mode = !data_mode;
  DbgEcho = !data_mode;
  UART_SetTimeout(DbgUart, data_mode ? DBG_DATAMODE_TIMEOUT : 0);
}

static STREAM_t stream = {
  .name = "debug",
  .modify = STREAM_Modify_Lowercase,
  .Size = DBG_Size,
  .Read = DBG_ReadString,
  .SwitchMode = switch_mode
};

void DBG_Init(UART_t *uart)
{
  DbgUart = uart;
  DbgUart->buff = &rx_buff;
  UART_Init(DbgUart);
}

// Holds `CMD_Step` back until pending input has been echoed, so the echoed line
// always precedes the command output. With `DBG_ECHO_MODE` off nothing calls
// `BUFF_Echo`, so `_echo` never follows `_head` (console mode skips the auto-advance
// in `BUFF_Push`): the gate has to stay open or the shell stops after the first byte
static bool echo_idle(void)
{
  #if(DBG_ECHO_MODE)
  return rx_buff._echo == rx_buff._head;
  #else
  return true;
  #endif
}

#if(DBG_ECHO_MODE)

// Typed bytes back to the console: backspace erases, enter closes the line,
// a line in progress hides the logs until it is closed
static void echo(void)
{
  static bool entered = false;
  static bool typing = false;
  char value;
  while(BUFF_Echo(&rx_buff, &value)) {
    if(value == '\b' || value == 0x7F) {
      BUFF_Pop(&rx_buff, NULL);
      if(BUFF_Pop(&rx_buff, NULL)) DBG_Char(0x7F);
      continue;
    }
    if(value == '\n' || value == '\f') {
      LogPrintFlag = true;
      if(!entered) {
        DBG_String(value == '\n' ? ANSI_GREEN "^E" ANSI_END : ANSI_RED "^C" ANSI_END);
        DBG_Enter();
      }
      entered = true;
      typing = false;
      continue;
    }
    if(!typing) {
      DBG_String(ANSI_ORANGE ">> " ANSI_END);
      LogPrintFlag = false;
    }
    typing = true;
    entered = false;
    DBG_Char(value);
  }
}

#endif

void DBG_Loop(void)
{
  while(1) {
    #if(DBG_ECHO_MODE)
    echo();
    #endif
    if(echo_idle()) CMD_Step(&stream);
    if(UART_IsFree(DbgUart)) {
      heap_clear();
      if(DbgFile->size) {
        uint8_t *batch = heap_new(DbgFile->size);
        // A heap too small to copy the batch out costs log lines, never the device
        if(batch) {
          memcpy(batch, DbgFile->buffer, DbgFile->size);
          UART_Send(DbgUart, batch, DbgFile->size);
        }
        MBB_Clear(DbgFile);
      }
      else if(DbgReset && UART_SendCompleted(DbgUart)) PWR_Reset();
    }
    let();
  }
}

//-------------------------------------------------------------------------------------------- Port

void DBG_Wait(void)
{
  while(UART_IsBusy(DbgUart)) let();
}

void DBG_WaitBlock(void)
{
  while(UART_IsBusy(DbgUart)) __NOP();
}

void DBG_Send(const uint8_t *data, uint16_t len)
{
  DBG_Wait();
  UART_Send(DbgUart, data, len);
  DBG_Wait();
}

void DBG_SendFile(MBB_t *file) { DBG_Send(file->buffer, file->size); }
void DBG_SetFile(MBB_t *file) { DbgFile = file; }
void DBG_DefaultFile(void) { DbgFile = &tx_file; }

//-------------------------------------------------------------------------------------------- Read

uint16_t DBG_Size(void) { return UART_Size(DbgUart); }
uint16_t DBG_Read(uint8_t *data) { return UART_Read(DbgUart, data); }
char *DBG_ReadString(void) { return UART_ReadString(DbgUart); }

//------------------------------------------------------------------------------------------ Output

int32_t DBG_Char(uint8_t data) { return MBB_Char(DbgFile, data); }
int32_t DBG_Char16(uint16_t data) { return MBB_Char16(DbgFile, data); }
int32_t DBG_Char32(uint32_t data) { return MBB_Char32(DbgFile, data); }
int32_t DBG_Char64(uint64_t data) { return MBB_Char64(DbgFile, data); }
int32_t DBG_Data(const uint8_t *data, uint16_t len) { return MBB_Data(DbgFile, data, len); }
int32_t DBG_String(const char *str) { return MBB_String(DbgFile, str); }
int32_t DBG_Enter(void) { return MBB_Enter(DbgFile); }
int32_t DBG_DropLastLine(void) { return MBB_DropLastLine(DbgFile); }
int32_t DBG_Bool(bool value) { return MBB_Bool(DbgFile, value); }

int32_t DBG_Int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space) {
  return MBB_Int(DbgFile, nbr, base, sign, fill_zero, fill_space);
}

int32_t DBG_Float(float nbr, uint8_t accuracy) { return MBB_Float(DbgFile, nbr, accuracy, 1); }

int32_t DBG_FloatSpace(float nbr, uint8_t accuracy, uint8_t fill_space) {
  return MBB_Float(DbgFile, nbr, accuracy, fill_space);
}

int32_t DBG_Dec(int64_t nbr) { return MBB_Dec(DbgFile, nbr); }
int32_t DBG_uDec(uint64_t nbr) { return MBB_uDec(DbgFile, nbr); }
int32_t DBG_Hex8(uint8_t nbr) { return MBB_Hex8(DbgFile, nbr); }
int32_t DBG_Hex16(uint16_t nbr) { return MBB_Hex16(DbgFile, nbr); }
int32_t DBG_Hex32(uint32_t nbr) { return MBB_Hex32(DbgFile, nbr); }
int32_t DBG_Bin8(uint8_t nbr) { return MBB_Bin8(DbgFile, nbr); }

int32_t DBG_Date(const RTC_Datetime_t *dt) { return MBB_Date(DbgFile, dt); }
int32_t DBG_Time(const RTC_Datetime_t *dt) { return MBB_Time(DbgFile, dt); }
int32_t DBG_TimeMs(const RTC_Datetime_t *dt) { return MBB_TimeMs(DbgFile, dt); }
int32_t DBG_Datetime(const RTC_Datetime_t *dt) { return MBB_Datetime(DbgFile, dt); }
int32_t DBG_DatetimeMs(const RTC_Datetime_t *dt) { return MBB_DatetimeMs(DbgFile, dt); }
int32_t DBG_AlarmTime(const RTC_AlarmCfg_t *alarm) { return MBB_AlarmTime(DbgFile, alarm); }
int32_t DBG_Alarm(const RTC_AlarmCfg_t *alarm) { return MBB_Alarm(DbgFile, alarm); }

int32_t MBB_Print(MBB_t *mbb)
{
  int32_t size = 0;
  size += DBG_String(ANSI_CREAM);
  size += DBG_String(mbb->name);
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
  for(uint16_t i = 0; i < mbb->size; i++) {
    size += DBG_Hex8(mbb->buffer[i]);
    size += DBG_Char(' ');
  }
  return size;
}

//-------------------------------------------------------------------------------------------------

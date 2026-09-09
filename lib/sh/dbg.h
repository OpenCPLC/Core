// lib/sh/dbg.h

#ifndef DBG_H_
#define DBG_H_

#include "uart.h"
#include "mbb.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef DBG_ECHO_MODE
  // Echo typed characters back to the console
  #define DBG_ECHO_MODE ON
#endif

#ifndef DBG_RX_SIZE
  // Console input ring [B]
  #define DBG_RX_SIZE 2048
#endif

#ifndef DBG_TX_SIZE
  // Output batch handed to the UART per `DBG_Loop` pass [B]
  #define DBG_TX_SIZE 2048
#endif

#ifndef DBG_DATAMODE_TIMEOUT
  // UART frame timeout while a binary transfer runs [bit times]
  #define DBG_DATAMODE_TIMEOUT 200
#endif

//----------------------------------------------------------------------------------------- Globals

extern UART_t *DbgUart;       // console port
extern MBB_t *DbgFile;        // output sink every printer writes to
extern volatile bool DbgReset; // reset requested, taken once the output drains
extern bool DbgEcho;

#ifdef HOST
  // Host build: let the pending output out, then end the process
  #define DBG_PrintAndTerminate() (DbgReset = true)
#endif

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Attach the console to a port and start it.
 * @param[in,out] uart Port, its `buff` is replaced by the console ring
 */
void DBG_Init(UART_t *uart);

// Console thread: echo, shell, output drain; never returns
void DBG_Loop(void);

// Wait for the port to finish sending, yielding or blocking
void DBG_Wait(void);
void DBG_WaitBlock(void);

/**
 * @brief Send bytes straight to the port, waiting before and after.
 * @param[in] data Bytes to send
 * @param[in] len Number of bytes
 */
void DBG_Send(const uint8_t *data, uint16_t len);

// Send the content of a buffer straight to the port
void DBG_SendFile(MBB_t *file);

// Redirect the printers to another buffer, or back to the console one
void DBG_SetFile(MBB_t *file);
void DBG_DefaultFile(void);

//-------------------------------------------------------------------------------------------- Read

// Console input, one message at a time: bytes waiting, copy out, or as a heap string
uint16_t DBG_Size(void);
uint16_t DBG_Read(uint8_t *data);
char *DBG_ReadString(void);

//------------------------------------------------------------------------------------------ Output

// Printers into `DbgFile`, parameters as the `MBB_...` counterparts; every one returns
// the bytes appended
int32_t DBG_Char(uint8_t data);
int32_t DBG_Char16(uint16_t data);
int32_t DBG_Char32(uint32_t data);
int32_t DBG_Char64(uint64_t data);
int32_t DBG_Data(const uint8_t *data, uint16_t len);
int32_t DBG_String(const char *str);
int32_t DBG_Enter(void);
int32_t DBG_DropLastLine(void);
int32_t DBG_Bool(bool value);

int32_t DBG_Int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space);
int32_t DBG_Float(float nbr, uint8_t accuracy);
int32_t DBG_FloatSpace(float nbr, uint8_t accuracy, uint8_t fill_space);
int32_t DBG_Dec(int64_t nbr);
int32_t DBG_uDec(uint64_t nbr);
int32_t DBG_Hex8(uint8_t nbr);
int32_t DBG_Hex16(uint16_t nbr);
int32_t DBG_Hex32(uint32_t nbr);
int32_t DBG_Bin8(uint8_t nbr);

int32_t DBG_Date(const RTC_Datetime_t *datetime);
int32_t DBG_Time(const RTC_Datetime_t *datetime);
int32_t DBG_TimeMs(const RTC_Datetime_t *datetime);
int32_t DBG_Datetime(const RTC_Datetime_t *datetime);
int32_t DBG_DatetimeMs(const RTC_Datetime_t *datetime);
int32_t DBG_AlarmTime(const RTC_AlarmCfg_t *alarm);
int32_t DBG_Alarm(const RTC_AlarmCfg_t *alarm);

// Buffer summary and hex dump, `%o` printers for the shell
int32_t MBB_Print(MBB_t *mbb);
int32_t MBB_PrintContent(MBB_t *mbb);

//-------------------------------------------------------------------------------------------------
#endif

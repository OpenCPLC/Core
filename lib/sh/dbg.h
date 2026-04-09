// lib/sh/dbg.h

#ifndef DBG_H_
#define DBG_H_

#include "uart.h"
#include "mbb.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef DBG_ECHO_MODE
  #define DBG_ECHO_MODE ON
#endif

#ifndef DBG_RX_SIZE
  #define DBG_RX_SIZE 2048
#endif

#ifndef DBG_TX_SIZE
  #define DBG_TX_SIZE 2048
#endif

#ifndef DBG_DATAMODE_TIMEOUT
  #define DBG_DATAMODE_TIMEOUT 200
#endif

//-------------------------------------------------------------------------------------------------

extern UART_t *DbgUart;
extern MBB_t *DbgFile;
extern volatile bool DbgReset;
extern bool DbgEcho;
#ifdef HOST
  #define DBG_PrintAndTerminate() (DbgReset = true)
#endif

/**
 * @brief Initialize debug interface.
 * @param[in] uart UART peripheral for debug I/O
 */
void DBG_Init(UART_t *uart);

/** @brief Main debug loop (blocking). */
void DBG_Loop(void);

/** @brief Wait for UART transmission to complete (cooperative). */
void DBG_Wait(void);

/** @brief Wait for UART transmission to complete (blocking). */
void DBG_WaitBlock(void);

/**
 * @brief Send raw data over debug UART.
 * @param[in] array Data buffer
 * @param[in] length Data length
 */
void DBG_Send(uint8_t *array, uint16_t length);

/**
 * @brief Send file contents over debug UART.
 * @param[in] file File to send
 */
void DBG_SendFile(MBB_t *file);

/** @brief Reset debug file to internal buffer. */
void DBG_DefaultFile(void);

/**
 * @brief Set custom output file.
 * @param[in] file Output file
 */
void DBG_SetFile(MBB_t *file);

//-------------------------------------------------------------------------------------------------

/** @brief Get available data size. */
uint16_t DBG_Size(void);

/**
 * @brief Read raw data from debug input.
 * @param[out] array Destination buffer
 * @return Bytes read
 */
uint16_t DBG_Read(uint8_t *array);

/** @brief Read string from debug input. */
char *DBG_ReadString(void);

//-------------------------------------------------------------------------------------------------

int32_t DBG_Char(uint8_t data);
int32_t DBG_Char16(uint16_t data);
int32_t DBG_Char32(uint32_t data);
int32_t DBG_Char64(uint64_t data);
int32_t DBG_Data(uint8_t *array, uint16_t length);
int32_t DBG_String(char *string);
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

int32_t DBG_Date(RTC_Datetime_t *datetime);
int32_t DBG_Time(RTC_Datetime_t *datetime);
int32_t DBG_TimeMs(RTC_Datetime_t *datetime);
int32_t DBG_Datetime(RTC_Datetime_t *datetime);
int32_t DBG_DatetimeMs(RTC_Datetime_t *datetime);
int32_t DBG_AlarmTime(RTC_AlarmCfg_t *alarm);
int32_t DBG_Alarm(RTC_AlarmCfg_t *alarm);
int32_t MBB_Print(MBB_t *mmb);
int32_t MBB_PrintContent(MBB_t *mbb);

//-------------------------------------------------------------------------------------------------

#endif
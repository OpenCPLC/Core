#ifndef DBG_H_
#define DBG_H_

#include "uart.h"
#include "file.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef DBG_ECHO_MODE
  #define DBG_ECHO_MODE 1
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
extern FILE_t *DbgFile;
extern volatile bool DbgReset;

void DBG_Init(UART_t *uart);
void DBG_Loop(void);
void DBG_Wait(void);
void DBG_WaitBlock(void);
void DBG_Send(uint8_t *array, uint16_t length);
void DBG_SendFile(FILE_t *file);
void DBG_DefaultFile(void);
void DBG_SetFile(FILE_t *file);

uint16_t DBG_Size(void);
uint16_t DBG_Read(uint8_t *array);
char *DBG_ReadString(void);

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
int32_t DBG_AlarmTime(RTC_Alarm_t *alarm);
int32_t DBG_Alarm(RTC_Alarm_t *alarm);
int32_t DBG_File(FILE_t *file);

//-------------------------------------------------------------------------------------------------
#endif

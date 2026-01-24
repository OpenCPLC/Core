#ifndef HD44780_H_
#define HD44780_H_

#include <stdint.h>
#include "twi.h"

//-------------------------------------------------------------------------------------------------

#define HD44780_EN 0b0100
#define HD44780_RW 0b0010
#define HD44780_RS 0b0001

typedef enum {
  HD44780_CMD_ClearDisplay = 0x01,
  HD44780_CMD_ReturnHome = 0x02,
  HD44780_CMD_EntryModeSet = 0x04,
  HD44780_CMD_DisplayControl = 0x08,
  HD44780_CMD_CursorShift = 0x10,
  HD44780_CMD_FunctionSet = 0x20,
  HD44780_CMD_SetAddrCGRAM = 0x40,
  HD44780_CMD_SetAddrDDRAM = 0x80
} HD44780_CMD_e;

//-------------------------------------------------------------------------------------------------

typedef enum {
  HD44780_Entry_Right = 0x01,
  HD44780_Entry_Left = 0x02,
  HD44780_Entry_ShiftIncrement = 0x04,
  HD44780_Entry_ShiftDecrement = 0x08
} HD44780_Entry_e;

typedef enum {
  HD44780_Display_ScreanOn = 0x04,
  HD44780_Display_ScreanOff = 0x00,
  HD44780_Display_CursorOn = 0x02,
  HD44780_Display_CursorOff = 0x00,
  HD44780_Display_BlinkOn = 0x02,
  HD44780_Display_BlinkOff = 0x00,
} HD44780_Display_e;

typedef enum {
  HD44780_Move_Display = 0x08,
  HD44780_Move_Cursor = 0x00,
  HD44780_Move_Right = 0x04,
  HD44780_Move_Left = 0x00
} HD44780_Move_e;

typedef enum {
  HD44780_Mode_8Bit = 0x10,
  HD44780_Mode_4Bit = 0x00,
  HD44780_Mode_2Line = 0x08,
  HD44780_Mode_1Line = 0x00,
  HD44780_Mode_Dots5x10 = 0x04,
  HD44780_Mode_Dots5x8 = 0x00
} HD44780_Mode_e;

typedef enum {
  HD44780_Backlight_On = 0x08,
  HD44780_Backlight_Off = 0x00
} HD44780_Backlight_e;

typedef enum {
  HD44780_Exec_ScreanOn,
  HD44780_Exec_ScreanOff,
  HD44780_Exec_CursorOn,
  HD44780_Exec_CursorOff,
  HD44780_Exec_BlinkOn,
  HD44780_Exec_BlinkOff,
  HD44780_Exec_ScrollLeft,
  HD44780_Exec_ScrollRight,
  HD44780_Exec_EntryLeft2Right,
  HD44780_Exec_EntryRight2Left,
  HD44780_Exec_AutoscrollOn,
  HD44780_Exec_AutoscrollOff,
  HD44780_Exec_BacklightOn,
  HD44780_Exec_BacklightOff,
} HD44780_Exec_e;

typedef enum {
  HD44780_Char_ArrowRight = 1,
  HD44780_Char_ArrowLeft = 2,
  HD44780_Char_ArrowUp = 3,
  HD44780_Char_ArrowDown = 4,
  HD44780_Char_BoxSet = 5,
  HD44780_Char_BoxRst = 6,
  HD44780_Char_Degree = 7,
} HD44780_Char_e;

//-------------------------------------------------------------------------------------------------

typedef struct {
  uint8_t address; // Adres I2C Expander'a PCF8574 (domyślnie 0x27)
  uint8_t columns; //
  uint8_t rows;
  bool size5x10;
  uint8_t _row_offsets[4];
  uint8_t _backlight;
  uint8_t _display;
  uint8_t _entry;
} HD44780_t;

//-------------------------------------------------------------------------------------------------

bool HD44780_Write(HD44780_t *hd, uint8_t value);
bool HD44780_Command(HD44780_t *hd, uint8_t cmd);
bool HD44780_Loc(HD44780_t *hd, uint8_t x, uint8_t y);
bool HD44780_Char(HD44780_t *hd, char value, uint8_t x, uint8_t y);
bool HD44780_Str(HD44780_t *hd, char *str, uint8_t x, uint8_t y);
bool HD44780_CreateChar(HD44780_t *hd, uint8_t loc, uint8_t *charmap);
bool HD44780_Clear(HD44780_t *hd);
bool HD44780_Home(HD44780_t *hd);
bool HD44780_Exec(HD44780_t *hd, HD44780_Exec_e exec);
bool HD44780_Init(HD44780_t *hd);
bool HD44780_ExtraChars(HD44780_t *hd);

//-------------------------------------------------------------------------------------------------
#endif

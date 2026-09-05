// dvr/hd44780.h

#ifndef HD44780_H_
#define HD44780_H_

#include <stdint.h>
#include "i2c_master.h"

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
} HD44780_CMD_t;

//-------------------------------------------------------------------------------------------------

// Entry Mode Set operands: bit 1 = I/D (cursor direction), bit 0 = S (autoscroll)
typedef enum {
  HD44780_Entry_Right = 0x00,
  HD44780_Entry_Left = 0x02,
  HD44780_Entry_ShiftIncrement = 0x01,
  HD44780_Entry_ShiftDecrement = 0x00
} HD44780_Entry_t;

// Display Control operands: bit 2 = display, bit 1 = cursor, bit 0 = blink
typedef enum {
  HD44780_Display_ScreenOn = 0x04,
  HD44780_Display_ScreenOff = 0x00,
  HD44780_Display_CursorOn = 0x02,
  HD44780_Display_CursorOff = 0x00,
  HD44780_Display_BlinkOn = 0x01,
  HD44780_Display_BlinkOff = 0x00,
} HD44780_Display_t;

typedef enum {
  HD44780_Move_Display = 0x08,
  HD44780_Move_Cursor = 0x00,
  HD44780_Move_Right = 0x04,
  HD44780_Move_Left = 0x00
} HD44780_Move_t;

typedef enum {
  HD44780_Mode_8Bit = 0x10,
  HD44780_Mode_4Bit = 0x00,
  HD44780_Mode_2Line = 0x08,
  HD44780_Mode_1Line = 0x00,
  HD44780_Mode_Dots5x10 = 0x04,
  HD44780_Mode_Dots5x8 = 0x00
} HD44780_Mode_t;

typedef enum {
  HD44780_Backlight_On = 0x08,
  HD44780_Backlight_Off = 0x00
} HD44780_Backlight_t;

typedef enum {
  HD44780_Exec_ScreenOn,
  HD44780_Exec_ScreenOff,
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
} HD44780_Exec_t;

typedef enum {
  HD44780_Char_ArrowRight = 1,
  HD44780_Char_ArrowLeft = 2,
  HD44780_Char_ArrowUp = 3,
  HD44780_Char_ArrowDown = 4,
  HD44780_Char_BoxSet = 5,
  HD44780_Char_BoxRst = 6,
  HD44780_Char_Degree = 7,
} HD44780_Char_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Character LCD driven through a PCF8574 I2C expander.
 * @param[in] i2c Bus the expander sits on
 * @param[in] address Expander address (default `0x27`)
 * @param[in] columns Display width in characters
 * @param[in] rows Display height in characters
 * @param[in] size5x10 Use 5x10 dot font instead of 5x8
 * Internal:
 * @param _row_offsets DDRAM address of every row
 * @param _backlight Backlight bit merged into each transfer
 * @param _display Display control operands
 * @param _entry Entry mode operands
 */
typedef struct {
  I2C_Master_t *i2c;
  uint8_t address;
  uint8_t columns;
  uint8_t rows;
  bool size5x10;
  // internal
  uint8_t _row_offsets[4];
  uint8_t _backlight;
  uint8_t _display;
  uint8_t _entry;
} HD44780_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Declare display of a given size.
 * @param name Variable name.
 * @param bus `I2C_Master_t` the expander sits on.
 * @param cols Display width in characters.
 * @param lines Display height in characters.
 */
#define HD44780_New(name, bus, cols, lines) \
  HD44780_t name = { .i2c = (bus), .address = 0x27, .columns = (cols), .rows = (lines) }

bool HD44780_Write(HD44780_t *hd, uint8_t value);
bool HD44780_Command(HD44780_t *hd, uint8_t cmd);
bool HD44780_Loc(HD44780_t *hd, uint8_t x, uint8_t y);
bool HD44780_Char(HD44780_t *hd, char value, uint8_t x, uint8_t y);
bool HD44780_Str(HD44780_t *hd, char *str, uint8_t x, uint8_t y);
bool HD44780_CreateChar(HD44780_t *hd, uint8_t loc, uint8_t *charmap);
bool HD44780_Clear(HD44780_t *hd);
bool HD44780_Home(HD44780_t *hd);
bool HD44780_Exec(HD44780_t *hd, HD44780_Exec_t exec);
bool HD44780_Init(HD44780_t *hd);
bool HD44780_ExtraChars(HD44780_t *hd);

//-------------------------------------------------------------------------------------------------
#endif

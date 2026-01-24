#include "hd44780.h"

//-------------------------------------------------------------------------------------------------

static bool HD44780_ExpanderWrite(HD44780_t *hd, uint8_t data)
{
  data |= hd->_backlight;
  return TWI_Write(hd->address, &data, 1);
}

static bool HD44780_Set4Bits(HD44780_t *hd, uint8_t value)
{
  if(!HD44780_ExpanderWrite(hd, value & ~HD44780_EN)) return false;
  if(!HD44780_ExpanderWrite(hd,value | HD44780_EN)) return false;
  if(!HD44780_ExpanderWrite(hd, value & ~HD44780_EN)) return false;
  return true;
}

static bool HD44780_Send(HD44780_t *hd, uint8_t value, uint8_t mode)
{
  uint8_t high = value & 0xF0;
  uint8_t low = (value << 4) & 0xF0;
  if(!HD44780_Set4Bits(hd, high | mode)) return false;
  if(!HD44780_Set4Bits(hd, low | mode)) return false;
  return true;
}

//-------------------------------------------------------------------------------------------------

bool HD44780_Write(HD44780_t *hd, uint8_t value)
{
  return HD44780_Send(hd, value, HD44780_RS);
}

bool HD44780_Command(HD44780_t *hd, uint8_t cmd)
{
  return HD44780_Send(hd, cmd, 0);
}

bool HD44780_Loc(HD44780_t *hd, uint8_t x, uint8_t y)
{
  if(x > hd->columns) x = hd->columns - 1;
  if(y > hd->rows) y = hd->rows - 1;
  return HD44780_Command(hd, HD44780_CMD_SetAddrDDRAM | (x + hd->_row_offsets[y]));
}

bool HD44780_Char(HD44780_t *hd, char value, uint8_t x, uint8_t y)
{
  if(!HD44780_Loc(hd, x, y)) return false;
  if(!HD44780_Write(hd, value)) return false;
  return true;
}

bool HD44780_Str(HD44780_t *hd, char *str, uint8_t x, uint8_t y)
{
  if(!HD44780_Loc(hd, x, y)) return false;
  while(*str) {
    if(!HD44780_Write(hd, *str)) return false;
    str++;
  }
  return true;
}

bool HD44780_CreateChar(HD44780_t *hd, uint8_t loc, uint8_t *charmap)
{
  loc &= 0x07;
  if(!HD44780_Command(hd, HD44780_CMD_SetAddrCGRAM | (loc << 3))) return false;
  for(uint8_t i = 0; i < 8; i++) {
    if(!HD44780_Write(hd, charmap[i])) return false;
  }
  return true;
}

bool HD44780_Clear(HD44780_t *hd)
{
  if(!HD44780_Command(hd, HD44780_CMD_ClearDisplay)) return false;
  delay(2);
  return true;
}

bool HD44780_Home(HD44780_t *hd)
{
  if(!HD44780_Command(hd, HD44780_CMD_ReturnHome)) return false;
  delay(2);
  return true;
}

bool HD44780_Exec(HD44780_t *hd, HD44780_Exec_e exec)
{
  switch(exec) {
    case HD44780_Exec_ScreanOn: hd->_display |= HD44780_Display_ScreanOn; break;
    case HD44780_Exec_ScreanOff: hd->_display &= ~HD44780_Display_ScreanOn; break;
    case HD44780_Exec_CursorOn: hd->_display |= HD44780_Display_CursorOn; break;
    case HD44780_Exec_CursorOff: hd->_display &= ~HD44780_Display_CursorOn; break;
    case HD44780_Exec_BlinkOn: hd->_display |= HD44780_Display_BlinkOn; break;
    case HD44780_Exec_BlinkOff: hd->_display &= ~HD44780_Display_BlinkOn; break;
    case HD44780_Exec_EntryLeft2Right: hd->_entry |= HD44780_Entry_Left; break;
    case HD44780_Exec_EntryRight2Left: hd->_entry &= ~HD44780_Entry_Left; break;
    case HD44780_Exec_AutoscrollOn: hd->_entry |= HD44780_Entry_ShiftIncrement; break;
    case HD44780_Exec_AutoscrollOff: hd->_entry &= ~HD44780_Entry_ShiftDecrement; break;
    case HD44780_Exec_BacklightOn: hd->_backlight = HD44780_Backlight_On; break;
    case HD44780_Exec_BacklightOff: hd->_backlight = HD44780_Backlight_Off; break;
    default: break;
  }
  switch(exec) {
    case HD44780_Exec_ScreanOn:
    case HD44780_Exec_ScreanOff:
    case HD44780_Exec_CursorOn:
    case HD44780_Exec_CursorOff:
    case HD44780_Exec_BlinkOn:
    case HD44780_Exec_BlinkOff:
      if(!HD44780_Command(hd, HD44780_CMD_DisplayControl | hd->_display)) return false;
      break;
    case HD44780_Exec_ScrollLeft:
      if(!HD44780_Command(hd, HD44780_CMD_CursorShift | HD44780_Move_Display | HD44780_Move_Left)) return false;
      break;
    case HD44780_Exec_ScrollRight:
      if(!HD44780_Command(hd, HD44780_CMD_CursorShift | HD44780_Move_Display | HD44780_Move_Right)) return false;
      break;
    case HD44780_Exec_EntryLeft2Right:
    case HD44780_Exec_EntryRight2Left:
    case HD44780_Exec_AutoscrollOn:
    case HD44780_Exec_AutoscrollOff:
      if(!HD44780_Command(hd, HD44780_CMD_EntryModeSet | hd->_entry)) return false;
      break;
    case HD44780_Exec_BacklightOn:
    case HD44780_Exec_BacklightOff:
      if(!HD44780_ExpanderWrite(hd, 0)) return false;
      break;
  }
  return true;
}

bool HD44780_Init(HD44780_t *hd)
{
  if(!hd->address) hd->address = 0x27;
  if(!hd->columns) hd->columns = 16;
  if(!hd->rows) hd->rows = 2;
  hd->_row_offsets[0] = 0x00;
  hd->_row_offsets[1] = 0x40;
  hd->_row_offsets[2] = 0x00 + hd->columns;
  hd->_row_offsets[3] = 0x40 + hd->columns;
  hd->_backlight = HD44780_Backlight_On;
  if(!HD44780_Set4Bits(hd, 0x03 << 4)) return false;
  delay(4);
  if(!HD44780_Set4Bits(hd, 0x03 << 4)) return false;
  delay(1);
  if(!HD44780_Set4Bits(hd, 0x03 << 4)) return false;
  delay(1);
  if(!HD44780_Set4Bits(hd, 0x02 << 4)) return false;
  uint8_t mode = HD44780_Mode_4Bit;
  if(hd->rows > 1) mode |= HD44780_Mode_2Line;
  if(hd->size5x10) mode |= HD44780_Mode_Dots5x10;
  if(!HD44780_Command(hd, HD44780_CMD_FunctionSet | mode)) return false;
  hd->_display = HD44780_Display_ScreanOn;
  if(!HD44780_Exec(hd, HD44780_Exec_ScreanOn)) return false;
  if(!HD44780_Exec(hd, HD44780_Exec_CursorOff)) return false;
  if(!HD44780_Exec(hd, HD44780_Exec_BlinkOff)) return false;
  if(!HD44780_Clear(hd)) return false;
  hd->_entry = HD44780_Entry_Left;
  if(!HD44780_Command(hd, HD44780_CMD_EntryModeSet | hd->_entry)) return false;
  if(!HD44780_Home(hd)) return false;
  return true;
}

//-------------------------------------------------------------------------------------------------

bool HD44780_ExtraChars(HD44780_t *hd)
{
  uint8_t nchar[8];
  nchar[7] = 0;
  // Arrow Right
  nchar[0] = 0b01000;
  nchar[1] = 0b01100;
  nchar[2] = 0b01110;
  nchar[3] = 0b01111;
  nchar[4] = 0b01110;
  nchar[5] = 0b01100;
  nchar[6] = 0b01000;
  if(!HD44780_CreateChar(hd, HD44780_Char_ArrowRight, nchar)) return false;
  // Arrow Left
  nchar[0] = 0b00010;
  nchar[1] = 0b00110;
  nchar[2] = 0b01110;
  nchar[3] = 0b11110;
  nchar[4] = 0b01110;
  nchar[5] = 0b00110;
  nchar[6] = 0b00010;
  if(!HD44780_CreateChar(hd, HD44780_Char_ArrowLeft, nchar)) return false;
  // Arrow Up
  nchar[0] = 0b00100;
  nchar[1] = 0b01110;
  nchar[2] = 0b11111;
  nchar[3] = 0b00000;
  nchar[4] = 0b00100;
  nchar[5] = 0b01110;
  nchar[6] = 0b11111;
  if(!HD44780_CreateChar(hd, HD44780_Char_ArrowUp, nchar)) return false;
  // Arrow Down
  nchar[0] = 0b11111;
  nchar[1] = 0b01110;
  nchar[2] = 0b00100;
  nchar[3] = 0b00000;
  nchar[4] = 0b11111;
  nchar[5] = 0b01110;
  nchar[6] = 0b00100;
  if(!HD44780_CreateChar(hd, HD44780_Char_ArrowDown, nchar)) return false;
  // Checkbox Checked
  nchar[0] = 0b00000;
  nchar[1] = 0b11111;
  nchar[2] = 0b11111;
  nchar[3] = 0b11111;
  nchar[4] = 0b11111;
  nchar[5] = 0b11111;
  nchar[6] = 0b00000;
  if(!HD44780_CreateChar(hd, HD44780_Char_BoxSet, nchar)) return false;
  // Checkbox Unchecked
  nchar[0] = 0b00000;
  nchar[1] = 0b11111;
  nchar[2] = 0b10001;
  nchar[3] = 0b10001;
  nchar[4] = 0b10001;
  nchar[5] = 0b11111;
  nchar[6] = 0b00000;
  if(!HD44780_CreateChar(hd, HD44780_Char_BoxRst, nchar)) return false;
  // Degree
  nchar[0] = 0b01110;
  nchar[1] = 0b01010;
  nchar[2] = 0b01110;
  nchar[3] = 0b00000;
  nchar[4] = 0b00000;
  nchar[5] = 0b00000;
  nchar[6] = 0b00000;
  if(!HD44780_CreateChar(hd, HD44780_Char_Degree, nchar)) return false;
  if(!HD44780_Home(hd)) return false;
  return true;
}

//-------------------------------------------------------------------------------------------------

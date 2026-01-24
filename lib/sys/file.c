#include "file.h"
#include "dbg.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Clear file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @return `OK` on success, `ERR` if locked
 */
status_t FILE_Clear(FILE_t *file)
{
  if(file->lock) return ERR;
  file->size = 0;
  return OK;
}

/**
 * @brief Copy content from one file to another.
 * @param[out] file_to Destination `FILE_t`
 * @param[in] file_from Source `FILE_t`
 * @return `OK` on success, `ERR` on error
 */
status_t FILE_Copy(FILE_t *file_to, FILE_t *file_from)
{
  if(file_to->lock) return ERR;
  if(file_from->size > file_to->limit) return ERR;
  memcpy(file_to->buffer, file_from->buffer, file_from->size);
  file_to->size = file_from->size;
  return OK;
}

/**
 * @brief Save raw data to file buffer (overwrite).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data Pointer to source data
 * @param[in] size Number of bytes to copy
 * @return `OK` on success, `ERR` on error
 */
status_t FILE_Save(FILE_t *file, const uint8_t *data, uint16_t size)
{
  if(file->lock) return ERR;
  if(size > file->limit) return ERR;
  memcpy(file->buffer, data, size);
  file->size = size;
  return OK;
}

/**
 * @brief Append raw data block to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data Pointer to source data
 * @param[in] size Number of bytes to append
 * @return `OK` on success, `ERR` on error
 */
status_t FILE_Append(FILE_t *file, const uint8_t *data, uint16_t size)
{
  return FILE_Data(file, data, size) ? OK : ERR;
}

/**
 * @brief Try to lock file for access.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @return `OK` if locked, `ERR` if already locked
 */
status_t FILE_Lock(FILE_t *file)
{
  if(file->lock) return ERR;
  file->lock = true;
  return OK;
}

/**
 * @brief Unlock file for access.
 * @param[in,out] file Pointer to `FILE_t` structure
 */
void FILE_Unlock(FILE_t *file)
{
  if(file) file->lock = false;
}

/**
 * @brief Try to lock two files for access.
 * Locks `primary` first, then `secondary` if different.
 * On failure, releases any acquired locks.
 * @param[in,out] primary Pointer to first `FILE_t`
 * @param[in,out] secondary Pointer to second `FILE_t` (can be NULL or same as primary)
 * @return `OK` if both locked, `ERR` on failure
 */
status_t FILE_Lock2(FILE_t *primary, FILE_t *secondary)
{
  if(FILE_Lock(primary)) return ERR;
  if(secondary && secondary != primary) {
    if(FILE_Lock(secondary)) {
      FILE_Unlock(primary);
      return ERR;
    }
  }
  return OK;
}

//------------------------------------------------------------------------------------------------- str

/**
 * @brief Append 8-bit value to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data 8-bit value
 * @return Number of bytes written `1` or `0` on error
 */
int32_t FILE_Char(FILE_t *file, uint8_t data)
{
  if(file->lock) return 0;
  if(file->size + 1 > file->limit) return 0;
  file->buffer[file->size++] = data;
  return 1;
}

/**
 * @brief Append 16-bit value in big-endian order to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data 16-bit value
 * @return Number of bytes written `2` or `0` on error
 */
int32_t FILE_Char16(FILE_t *file, uint16_t data)
{
  if(file->lock) return 0;
  if(file->size + 2 > file->limit) return 0;
  FILE_Char(file, (uint8_t)(data >> 8));
  FILE_Char(file, (uint8_t)data);
  return 2;
}

/**
 * @brief Append 32-bit value in big-endian order to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data 32-bit value
 * @return Number of bytes written `4` or `0` on error
 */
int32_t FILE_Char32(FILE_t *file, uint32_t data)
{
  if(file->lock) return 0;
  if(file->size + 4 > file->limit) return 0;
  FILE_Char16(file, (uint16_t)(data >> 16));
  FILE_Char16(file, (uint16_t)data);
  return 4;
}

/**
 * @brief Append 64-bit value in big-endian order to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data 64-bit value
 * @return Number of bytes written `8` or `0` on error
 */
int32_t FILE_Char64(FILE_t *file, uint64_t data)
{
  if(file->lock) return 0;
  if(file->size + 8 > file->limit) return 0;
  FILE_Char32(file, (uint32_t)(data >> 32));
  FILE_Char32(file, (uint32_t)data);
  return 8;
}

/**
 * @brief Append data block to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] data Pointer to source data
 * @param[in] len Number of bytes to copy
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Data(FILE_t *file, const uint8_t *data, uint16_t len)
{
  if(file->lock) return 0;
  if(file->size + len > file->limit) return 0;
  memcpy(&file->buffer[file->size], data, len);
  file->size += len;
  return len;
}

/**
 * @brief Append null-terminated string to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] string Pointer to null-terminated string
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_String(FILE_t *file, const char *string)
{
  if(file->lock || !string) return 0;
  uint16_t len = 0;
  while(string[len]) len++;
  if(file->size + len > file->limit) return 0;
  memcpy(&file->buffer[file->size], string, len);
  file->size += len;
  return len;
}

/**
 * @brief Append newline to file buffer.
 * Uses `\r\n` if `FILE_ENTER_RETURN` is enabled, otherwise `\n`.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Enter(FILE_t *file)
{
  #if(FILE_ENTER_RETURN)
    return FILE_String(file, "\r\n");
  #else
    return FILE_Char(file, '\n');
  #endif
}

/**
 * @brief Remove last line from file buffer.
 * Scans backward until first line break (`\n` or `\r`) and truncates buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @return Number of bytes removed or 0 if no line found
 */
int32_t FILE_DropLastLine(FILE_t *file)
{
  if(file->lock || file->size == 0) return 0;
  int32_t len = 0;
  bool found = false;
  for(int32_t i = file->size - 1; i >= 0; i--) {
    uint8_t c = file->buffer[i];
    if(c == '\r' || c == '\n') {
      if(found) {
        file->size = i + 1;
        return len;
      }
    } else found = true;
    file->size--;
    len++;
  }
  return len;
}

/**
 * @brief Append boolean value ("true"/"false") to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] value Boolean value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Bool(FILE_t *file, bool value)
{
  return value ? FILE_String(file, "true") : FILE_String(file, "false");
}

//------------------------------------------------------------------------------------------------- nbr

/**
 * @brief Append integer as text to file buffer.
 * @param[in,out] file Pointer to `FILE_t` (buffer is modified)
 * @param[in] nbr 64-bit value to format
 * @param[in] base Numeric base (2–36)
 * @param[in] sign If true, treat as signed and prepend '-' for negatives; if false, format as unsigned (cast)
 * @param[in] fill_zero Minimum digit count for magnitude (left zero padding; sign not counted)
 * @param[in] fill_space Minimum field width (left space padding; applied after sign/zero padding)
 * @return Number of bytes written; `0` on lock/invalid params; `ERR` if insufficient space
 */
int32_t FILE_Int(FILE_t *file, int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space)
{
  if(file->lock) return 0;
  int32_t len = (int32_t)itoa_encode(nbr, StrTempMem, base, sign, fill_zero, fill_space);
  if(file->size + len > file->limit) return 0;
  int32_t n = len;
  while(n) {
    n--;
    file->buffer[file->size++] = StrTempMem[n];
  }
  return len;
}

/**
 * @brief Append float value to file buffer.
 * Supports NaN, Inf, decimal point, zero/space padding.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr Float number
 * @param[in] accuracy Number of digits after decimal point
 * @param[in] fill_space Minimum field width (space padded)
 * @return Number of bytes written, `0` on lock/error
 */
int32_t FILE_Float(FILE_t *file, float nbr, uint8_t accuracy, uint8_t fill_space)
{
  if(file->lock) return 0;
  #if(FILE_PRINT_NAN_INF)
    if(isnan(nbr)) {
      const char *txt = "NaN";
      uint8_t len = strlen(txt);
      if(fill_space > len) {
        char buf[fill_space + 1];
        memset(buf, ' ', fill_space - len);
        memcpy(buf + fill_space - len, txt, len + 1);
        return FILE_String(file, buf);
      }
      return FILE_String(file, txt);
    }
    else if(isinf(nbr)) {
      const char *txt = signbit(nbr) ? "-Inf" : "Inf";
      uint8_t len = strlen(txt);
      if(fill_space > len) {
        char buf[fill_space + 1];
        memset(buf, ' ', fill_space - len);
        memcpy(buf + fill_space - len, txt, len + 1);
        return FILE_String(file, buf);
      }
      return FILE_String(file, txt);
    }
  #else
    if(isnan(nbr) || isinf(nbr)) {
      uint8_t len = fill_space > 1 ? fill_space : 1; 
      char nan[len + 1];
      memset(nan, ' ', len - 1);
      nan[len - 1] = '-'; 
      nan[len] = '\0';
      return FILE_String(file, nan);
    }
  #endif
  // move '.'
  for(uint16_t i = 0; i < accuracy; i++) nbr *= 10;
  if(!fill_space) fill_space = 1;
  int32_t length = (int32_t)itoa_encode((int32_t)nbr, StrTempMem, 10, true, nbr < 0 ? accuracy + 2 : accuracy + 1, fill_space - 1);
  int32_t n = length + (accuracy ? 1 : 0);
  if(file->size + n > file->limit) return 0;
  while(length) {
    if(accuracy && length == accuracy) {
      file->buffer[file->size++] = '.';
    }
    file->buffer[file->size++] = StrTempMem[--length];
  }
  return n;
}

/**
 * @brief Append signed decimal number.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr Signed integer value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Dec(FILE_t *file, int64_t nbr) 
{
  return FILE_Int(file, nbr, 10, true, 0, 0);
}

/**
 * @brief Append unsigned decimal number.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr Unsigned integer value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_uDec(FILE_t *file, uint64_t nbr)
{
  return FILE_Int(file, (int64_t)nbr, 10, false, 0, 0);
}

/**
 * @brief Append 8-bit number as hex (2 digits).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr 8-bit value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Hex8(FILE_t *file, uint8_t nbr)
{
  return FILE_Int(file, (int64_t)nbr, 16, false, 2, 2);
}

/**
 * @brief Append 16-bit number as hex (4 digits).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr 16-bit value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Hex16(FILE_t *file, uint16_t nbr)
{
  return FILE_Int(file, (int64_t)nbr, 16, false, 4, 4);
}

/**
 * @brief Append 32-bit number as hex (8 digits).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr 32-bit value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Hex32(FILE_t *file, uint32_t nbr)
{
  return FILE_Int(file, (int64_t)nbr, 16, false, 8, 8);
}

/**
 * @brief Append 8-bit number as binary (8 digits).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr 8-bit value
 * @return Number of bytes written or `0` on error
 */
int32_t FILE_Bin8(FILE_t *file, uint8_t nbr)
{
  return FILE_Int(file, (int64_t)nbr, 2, false, 8, 8);
}

/**
 * @brief Append float number with default format.
 * Accuracy = 3, no space padding.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] nbr Float value
 * @return Number of bytes written or 0/ERR on error
 */
int32_t FILE_Nbr(FILE_t *file, float nbr)
{
  return FILE_Float(file, nbr, 3, 0);
}

//------------------------------------------------------------------------------------------------- struct

/**
 * @brief Append one struct to file buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] object Pointer to source struct
 * @return Number of bytes written, `0` on error
 */
int32_t FILE_Struct_Add(FILE_t *file, const uint8_t *object)
{
  if(file->lock) return 0;
  if(file->size + file->struct_size > file->limit) return 0;
  memcpy(&file->buffer[file->size], object, file->struct_size);
  file->size += file->struct_size;
  return file->struct_size;
}

/**
 * @brief Get number of structs stored in file buffer.
 * @param[in] file Pointer to `FILE_t` structure
 * @return Count of structs
 */
uint16_t FILE_Struct_Count(FILE_t *file)
{
  return file->size / file->struct_size;
}

/**
 * @brief Get number of structs still fitting in buffer.
 * @param[in] file Pointer to `FILE_t` structure
 * @param[in] margin Reserved space at end of buffer
 * @return Count of free structs
 */
uint16_t FILE_Struct_FreeSpace(FILE_t *file, uint16_t margin)
{
  if((int32_t)file->limit - margin <= 0) return 0;
  return ((file->limit - margin) / file->struct_size) - (file->size / file->struct_size);
}

/**
 * @brief Move structs to beginning of buffer (drop from head).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] count Number of structs to drop
 * @return Negative size of dropped data, 0 if nothing dropped
 */
int32_t FILE_Struct_Move(FILE_t *file, uint16_t count)
{
  if(file->lock) return 0;
  int32_t size = (int32_t)file->size - (file->struct_size * count);
  if(size > 0) {
    memcpy(file->buffer, &file->buffer[file->struct_size * count], (size_t)size);
    file->size = size;
    return -(int32_t)(file->struct_size * count);
  }
  file->size = 0;
  return -((int32_t)file->size);
}

/**
 * @brief Drop structs from end of buffer.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] count Number of structs to drop
 * @return Negative size of dropped data, 0 if nothing dropped
 */
int32_t FILE_Struct_Drop(FILE_t *file, uint16_t count)
{
  if(file->lock) return 0;
  if(!count) return 0;
  int32_t size = file->struct_size * count;
  if(size > file->size) {
    size = file->size;
    file->size = 0;
  } else {
    file->size -= size;
  }
  return -size;
}

/**
 * @brief Get struct at index into destination buffer.
 * @param[in] file Pointer to `FILE_t` structure
 * @param[in] index Index of struct (0-based)
 * @param[out] object Pointer to destination buffer
 * @return Struct size on success, `0` on error
 */
int32_t FILE_Struct_Get(FILE_t *file, uint16_t index, uint8_t *object)
{
  uint32_t pos = index * file->struct_size;
  if(pos + file->struct_size > file->size) return 0;
  memcpy(object, &file->buffer[pos], file->struct_size);
  return file->struct_size;
}

/**
 * @brief Peek pointer to struct at index (read-only).
 * @param[in] file Pointer to `FILE_t` structure
 * @param[in] index Index of struct (0-based)
 * @return Pointer to struct in buffer, NULL if out of range
 */
const uint8_t *FILE_Struct_Peek(FILE_t *file, uint16_t index)
{
  uint32_t pos = index * file->struct_size;
  if(pos + file->struct_size > file->size) return NULL;
  return &file->buffer[pos];
}

//------------------------------------------------------------------------------------------------- offset

/**
 * @brief Restore file buffer pointer to base (drop offset).
 * @param[in,out] file Pointer to `FILE_t` structure
 * @return `OK` on success, `ERR` if locked
 */
status_t FILE_Offset_Rst(FILE_t *file)
{
  if(file->lock) return ERR;
  if(!file->offset) return OK; // nothing to drop
  file->buffer = file->base;
  file->limit = file->limit + file->offset;
  file->size = file->size  + file->offset;
  file->offset = 0;
  return OK;
}

/**
 * @brief Shift file buffer start by offset.
 * Calls Drop first to reset any previous offset.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] offset Number of bytes to shift
 * @return `OK` on success, `ERR` on error
 */
status_t FILE_Offset_Set(FILE_t *file, uint16_t offset)
{
  if(FILE_Offset_Rst(file)) return ERR;
  if(offset > file->limit) return ERR;
  file->buffer = file->base + offset;
  file->limit -= offset;
  if(file->size > offset) file->size -= offset;
  else file->size = 0;
  file->offset = offset;
  return OK;
}

//------------------------------------------------------------------------------------------------- flash

/**
 * @brief Save file buffer into Flash memory page.
 * Does nothing if data in Flash is the same.
 * @param[in] file Pointer to `FILE_t` structure
 * @return `OK` on success, `ERR` on error or if page = 0
 */
status_t FILE_Flash_Save(FILE_t *file)
{
  if(!file->flash_page) return ERR; // page 0 = invalid
  if(FLASH_Compare(file->flash_page, file->buffer, file->size)) {
    return OK; // no change
  }
  if(FLASH_Save(file->flash_page, file->buffer, file->size)) {
    return ERR;
  }
  return OK;
}

/**
 * @brief Load file buffer from Flash memory page.
 * Updates `size` field with number of bytes loaded.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @return `OK` on success, `ERR` on error, lock or if page = 0
 */
status_t FILE_Flash_Load(FILE_t *file)
{
  if(file->lock) return ERR;
  if(!file->flash_page) return ERR; // page 0 = invalid
  file->size = FLASH_Load(file->flash_page, file->buffer);
  return file->size ? OK : ERR;
}

//------------------------------------------------------------------------------------------------- rtc

/**
 * @brief Append date as YYYY-MM-DD.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] datetime Pointer to RTC datetime
 * @return Number of bytes written, `0` on error
 */
int32_t FILE_Date(FILE_t *file, RTC_Datetime_t *datetime)
{
  if(file->lock) return 0;
  if(file->size + 10 >= file->limit) return 0;
  FILE_String(file, "20");
  FILE_Int(file, datetime->year, 10, false, 2, 2);
  FILE_Char(file, '-');
  FILE_Int(file, datetime->month, 10, false, 2, 2);
  FILE_Char(file, '-');
  FILE_Int(file, datetime->month_day, 10, false, 2, 2);
  return 10;
}

/**
 * @brief Append time as HH:MM:SS.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] datetime Pointer to RTC datetime
 * @return Number of bytes written, `0` on error
 */
int32_t FILE_Time(FILE_t *file, RTC_Datetime_t *datetime)
{
  if(file->lock) return 0;
  if(file->size + 8 >= file->limit) return 0;
  FILE_Int(file, datetime->hour, 10, false, 2, 2);
  FILE_Char(file, ':');
  FILE_Int(file, datetime->minute, 10, false, 2, 2);
  FILE_Char(file, ':');
  FILE_Int(file, datetime->second, 10, false, 2, 2);
  return 8;
}

/**
 * @brief Append time with milliseconds as HH:MM:SS.mmm.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] datetime Pointer to RTC datetime
 * @return Number of bytes written, `0` on error
 */
int32_t FILE_TimeMs(FILE_t *file, RTC_Datetime_t *datetime)
{
  if(file->lock) return 0;
  if(file->size + 12 >= file->limit) return 0;
  FILE_Time(file, datetime);
  FILE_Char(file, '.');
  FILE_Int(file, datetime->ms, 10, false, 3, 3);
  return 12;
}

/**
 * @brief Append date and time as "YYYY-MM-DD HH:MM:SS".
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] datetime Pointer to RTC datetime
 * @return Always `19` on success, `0` on error
 */
int32_t FILE_Datetime(FILE_t *file, RTC_Datetime_t *datetime)
{
  if(file->lock) return 0;
  if(file->size + 19 >= file->limit) return 0;
  FILE_Date(file, datetime);
  FILE_Char(file, ' ');
  FILE_Time(file, datetime);
  return 19;
}

/**
 * @brief Append date and time with milliseconds as "YYYY-MM-DD HH:MM:SS.mmm".
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] datetime Pointer to RTC datetime
 * @return Always `23` on success, `0` on error
 */
int32_t FILE_DatetimeMs(FILE_t *file, RTC_Datetime_t *datetime)
{
  if(file->lock) return 0;
  if(file->size + 23 >= file->limit) return 0;
  FILE_Datetime(file, datetime);
  FILE_Char(file, '.');
  FILE_Int(file, datetime->ms, 10, false, 3, 3);
  return 23;
}

/**
 * @brief Append alarm time as "HH:MM:SS" with masks.
 * Uses "**" for masked fields.
 * Example: "**:15:**" means alarm triggers every minute at second 15.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] alarm Pointer to RTC alarm
 * @return Always `8` on success, `0` on error
 */
int32_t FILE_AlarmTime(FILE_t *file, RTC_Alarm_t *alarm)
{
  if(file->lock) return 0;
  if(file->size + 8 >= file->limit) return 0;
  if(alarm->hour_mask) FILE_String(file, "**:");
  else {
    FILE_Int(file, alarm->hour, 10, false, 2, 2);
    FILE_Char(file, ':');
  }
  if(alarm->minute_mask) FILE_String(file, "**:");
  else {
    FILE_Int(file, alarm->minute, 10, false, 2, 2);
    FILE_Char(file, ':');
  }
  if(alarm->second_mask) FILE_String(file, "**");
  else {
    FILE_Int(file, alarm->second, 10, false, 2, 2);
  }
  return 8;
}

/**
 * @brief Append alarm as "DAY HH:MM:SS" with masks.
 * Day can be replaced by first entry of `RtcWeekdays` if masked.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] alarm Pointer to RTC alarm
 * @return Always 12 on success, `0` on error
 */
int32_t FILE_Alarm(FILE_t *file, RTC_Alarm_t *alarm)
{
  if(file->lock) return 0;
  if(file->size + 12 > file->limit) return 0;
  if(alarm->day_mask) FILE_String(file, (char *)RtcWeekdays[0]);
  else FILE_String(file, (char *)RtcWeekdays[alarm->day]);
  FILE_Char(file, ' ');
  FILE_AlarmTime(file, alarm);
  return 12;
}

//------------------------------------------------------------------------------------------------- crc

/**
 * @brief Append CRC checksum at end of file buffer.
 * Uses given CRC configuration and updates buffer size.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] crc Pointer to CRC config
 * @return Number of CRC bytes appended, `0` on error
 */
int32_t FILE_CrcAppend(FILE_t *file, const CRC_t *crc)
{
  if(file->limit - file->size < crc->width / 8) return 0;
  file->size = CRC_Append(crc, file->buffer, file->size);
  return crc->width / 8;
}

/**
 * @brief Check CRC at end of file buffer.
 * On success, removes CRC bytes from buffer size.
 * @param[in,out] file Pointer to `FILE_t` structure
 * @param[in] crc Pointer to CRC config
 * @return `true` if error, `false` if CRC is valid
 */
bool FILE_CrcError(FILE_t *file, const CRC_t *crc)
{
  if(file->size < crc->width / 8) return true;
  if(CRC_Error(crc, file->buffer, file->size)) return true;
  file->size -= crc->width / 8;
  return false;
}

//-------------------------------------------------------------------------------------------------

/** @file lib/sys/mbb.c */

#include "mbb.h"
#include "dbg.h"

//-------------------------------------------------------------------------------------------------

status_t MBB_Clear(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  mbb->size = 0;
  return OK;
}

status_t MBB_Copy(MBB_t *dst, MBB_t *src)
{
  if(dst->lock) return ERR;
  if(src->size > dst->limit) return ERR;
  memcpy(dst->buffer, src->buffer, src->size);
  dst->size = src->size;
  return OK;
}

status_t MBB_Save(MBB_t *mbb, const uint8_t *data, uint16_t size)
{
  if(mbb->lock) return ERR;
  if(size > mbb->limit) return ERR;
  memcpy(mbb->buffer, data, size);
  mbb->size = size;
  return OK;
}

status_t MBB_Append(MBB_t *mbb, const uint8_t *data, uint16_t size)
{
  return MBB_Data(mbb, data, size) ? OK : ERR;
}

status_t MBB_Lock(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  mbb->lock = true;
  return OK;
}

void MBB_Unlock(MBB_t *mbb)
{
  if(mbb) mbb->lock = false;
}

status_t MBB_Lock2(MBB_t *primary, MBB_t *secondary)
{
  if(MBB_Lock(primary)) return ERR;
  if(secondary && secondary != primary) {
    if(MBB_Lock(secondary)) {
      MBB_Unlock(primary);
      return ERR;
    }
  }
  return OK;
}

//------------------------------------------------------------------------------------------------- str

int32_t MBB_Char(MBB_t *mbb, uint8_t data)
{
  if(mbb->lock) return 0;
  if(mbb->size + 1 > mbb->limit) return 0;
  mbb->buffer[mbb->size++] = data;
  return 1;
}

int32_t MBB_Char16(MBB_t *mbb, uint16_t data)
{
  if(mbb->lock) return 0;
  if(mbb->size + 2 > mbb->limit) return 0;
  MBB_Char(mbb, (uint8_t)(data >> 8));
  MBB_Char(mbb, (uint8_t)data);
  return 2;
}

int32_t MBB_Char32(MBB_t *mbb, uint32_t data)
{
  if(mbb->lock) return 0;
  if(mbb->size + 4 > mbb->limit) return 0;
  MBB_Char16(mbb, (uint16_t)(data >> 16));
  MBB_Char16(mbb, (uint16_t)data);
  return 4;
}

int32_t MBB_Char64(MBB_t *mbb, uint64_t data)
{
  if(mbb->lock) return 0;
  if(mbb->size + 8 > mbb->limit) return 0;
  MBB_Char32(mbb, (uint32_t)(data >> 32));
  MBB_Char32(mbb, (uint32_t)data);
  return 8;
}

int32_t MBB_Data(MBB_t *mbb, const uint8_t *data, uint16_t len)
{
  if(mbb->lock) return 0;
  if(mbb->size + len > mbb->limit) return 0;
  memcpy(&mbb->buffer[mbb->size], data, len);
  mbb->size += len;
  return len;
}

int32_t MBB_String(MBB_t *mbb, const char *str)
{
  if(mbb->lock || !str) return 0;
  uint16_t len = 0;
  while(str[len]) len++;
  if(mbb->size + len > mbb->limit) return 0;
  memcpy(&mbb->buffer[mbb->size], str, len);
  mbb->size += len;
  return len;
}

int32_t MBB_Enter(MBB_t *mbb)
{
  #if(MBB_ENTER_RETURN)
    return MBB_String(mbb, "\r\n");
  #else
    return MBB_Char(mbb, '\n');
  #endif
}

int32_t MBB_DropLastLine(MBB_t *mbb)
{
  if(mbb->lock || mbb->size == 0) return 0;
  int32_t len = 0;
  bool found = false;
  for(int32_t i = mbb->size - 1; i >= 0; i--) {
    uint8_t c = mbb->buffer[i];
    if(c == '\r' || c == '\n') {
      if(found) {
        mbb->size = i + 1;
        return len;
      }
    }
    else found = true;
    mbb->size--;
    len++;
  }
  return len;
}

int32_t MBB_Bool(MBB_t *mbb, bool value)
{
  return value ? MBB_String(mbb, "true") : MBB_String(mbb, "false");
}

//------------------------------------------------------------------------------------------------- nbr

int32_t MBB_Int(MBB_t *mbb, int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space)
{
  if(mbb->lock) return 0;
  int32_t len = (int32_t)itoa_encode(nbr, StrTempMem, base, sign, fill_zero, fill_space);
  if(mbb->size + len > mbb->limit) return 0;
  int32_t n = len;
  while(n) {
    n--;
    mbb->buffer[mbb->size++] = StrTempMem[n];
  }
  return len;
}

int32_t MBB_Float(MBB_t *mbb, float nbr, uint8_t accuracy, uint8_t fill_space)
{
  if(mbb->lock) return 0;
  #if(MBB_PRINT_NAN_INF)
    if(isnan(nbr)) {
      const char *txt = "NaN";
      uint8_t len = 3;
      while(fill_space > len) { MBB_Char(mbb, ' '); fill_space--; }
      return MBB_String(mbb, txt);
    }
    else if(isinf(nbr)) {
      const char *txt = signbit(nbr) ? "-Inf" : "Inf";
      uint8_t len = signbit(nbr) ? 4 : 3;
      while(fill_space > len) { MBB_Char(mbb, ' '); fill_space--; }
      return MBB_String(mbb, txt);
    }
  #else
    if(isnan(nbr) || isinf(nbr)) {
      uint8_t len = fill_space > 1 ? fill_space : 1;
      while(len > 1) { MBB_Char(mbb, ' '); len--; }
      return MBB_Char(mbb, '-');
    }
  #endif
  for(uint16_t i = 0; i < accuracy; i++) nbr *= 10;
  if(!fill_space) fill_space = 1;
  int32_t length = (int32_t)itoa_encode((int32_t)nbr, StrTempMem, 10, true, nbr < 0 ? accuracy + 2 : accuracy + 1, fill_space - 1);
  int32_t n = length + (accuracy ? 1 : 0);
  if(mbb->size + n > mbb->limit) return 0;
  while(length) {
    if(accuracy && length == accuracy) {
      mbb->buffer[mbb->size++] = '.';
    }
    mbb->buffer[mbb->size++] = StrTempMem[--length];
  }
  return n;
}

int32_t MBB_Dec(MBB_t *mbb, int64_t nbr)
{
  return MBB_Int(mbb, nbr, 10, true, 0, 0);
}

int32_t MBB_uDec(MBB_t *mbb, uint64_t nbr)
{
  return MBB_Int(mbb, (int64_t)nbr, 10, false, 0, 0);
}

int32_t MBB_Hex8(MBB_t *mbb, uint8_t nbr)
{
  return MBB_Int(mbb, (int64_t)nbr, 16, false, 2, 2);
}

int32_t MBB_Hex16(MBB_t *mbb, uint16_t nbr)
{
  return MBB_Int(mbb, (int64_t)nbr, 16, false, 4, 4);
}

int32_t MBB_Hex32(MBB_t *mbb, uint32_t nbr)
{
  return MBB_Int(mbb, (int64_t)nbr, 16, false, 8, 8);
}

int32_t MBB_Bin8(MBB_t *mbb, uint8_t nbr)
{
  return MBB_Int(mbb, (int64_t)nbr, 2, false, 8, 8);
}

int32_t MBB_Nbr(MBB_t *mbb, float nbr)
{
  return MBB_Float(mbb, nbr, 3, 0);
}

//------------------------------------------------------------------------------------------------- struct

int32_t MBB_StructAdd(MBB_t *mbb, const uint8_t *object)
{
  if(mbb->lock) return 0;
  if(mbb->size + mbb->struct_size > mbb->limit) return 0;
  memcpy(&mbb->buffer[mbb->size], object, mbb->struct_size);
  mbb->size += mbb->struct_size;
  return mbb->struct_size;
}

uint16_t MBB_StructCount(const MBB_t *mbb)
{
  return mbb->size / mbb->struct_size;
}

uint16_t MBB_StructFree(const MBB_t *mbb, uint16_t margin)
{
  if((int32_t)mbb->limit - margin <= 0) return 0;
  return ((mbb->limit - margin) / mbb->struct_size) - (mbb->size / mbb->struct_size);
}

int32_t MBB_StructShift(MBB_t *mbb, uint16_t count)
{
  if(mbb->lock) return 0;
  int32_t shift_bytes = mbb->struct_size * count;
  int32_t remaining = (int32_t)mbb->size - shift_bytes;
  if(remaining > 0) {
    memcpy(mbb->buffer, &mbb->buffer[shift_bytes], (size_t)remaining);
    mbb->size = remaining;
    return -shift_bytes;
  }
  int32_t dropped = mbb->size;
  mbb->size = 0;
  return -(int32_t)dropped;
}

int32_t MBB_StructDrop(MBB_t *mbb, uint16_t count)
{
  if(mbb->lock) return 0;
  if(!count) return 0;
  int32_t size = mbb->struct_size * count;
  if(size > mbb->size) {
    size = mbb->size;
    mbb->size = 0;
  }
  else {
    mbb->size -= size;
  }
  return -size;
}

int32_t MBB_StructGet(const MBB_t *mbb, uint16_t index, uint8_t *dst)
{
  uint32_t pos = index * mbb->struct_size;
  if(pos + mbb->struct_size > mbb->size) return 0;
  memcpy(dst, &mbb->buffer[pos], mbb->struct_size);
  return mbb->struct_size;
}

const uint8_t *MBB_StructPeek(const MBB_t *mbb, uint16_t index)
{
  uint32_t pos = index * mbb->struct_size;
  if(pos + mbb->struct_size > mbb->size) return NULL;
  return &mbb->buffer[pos];
}

//------------------------------------------------------------------------------------------------- offset

status_t MBB_OffsetRst(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  if(!mbb->_offset) return OK;
  mbb->buffer = mbb->_base;
  mbb->limit = mbb->limit + mbb->_offset;
  mbb->size = mbb->size + mbb->_offset;
  mbb->_offset = 0;
  return OK;
}

status_t MBB_OffsetSet(MBB_t *mbb, uint16_t offset)
{
  if(MBB_OffsetRst(mbb)) return ERR;
  if(offset > mbb->limit) return ERR;
  mbb->buffer = mbb->_base + offset;
  mbb->limit -= offset;
  if(mbb->size > offset) mbb->size -= offset;
  else mbb->size = 0;
  mbb->_offset = offset;
  return OK;
}

//------------------------------------------------------------------------------------------------- flash

status_t MBB_FlashSave(MBB_t *mbb)
{
  if(!mbb->flash_page) return ERR;
  if(FLASH_Compare(mbb->flash_page, mbb->buffer, mbb->size)) {
    return OK;
  }
  if(FLASH_Save(mbb->flash_page, mbb->buffer, mbb->size)) {
    return ERR;
  }
  return OK;
}

status_t MBB_FlashLoad(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  if(!mbb->flash_page) return ERR;
  mbb->size = FLASH_Load(mbb->flash_page, mbb->buffer);
  return mbb->size ? OK : ERR;
}

//------------------------------------------------------------------------------------------------- rtc

int32_t MBB_Date(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 10 >= mbb->limit) return 0;
  MBB_String(mbb, "20");
  MBB_Int(mbb, dt->year, 10, false, 2, 2);
  MBB_Char(mbb, '-');
  MBB_Int(mbb, dt->month, 10, false, 2, 2);
  MBB_Char(mbb, '-');
  MBB_Int(mbb, dt->month_day, 10, false, 2, 2);
  return 10;
}

int32_t MBB_Time(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 8 >= mbb->limit) return 0;
  MBB_Int(mbb, dt->hour, 10, false, 2, 2);
  MBB_Char(mbb, ':');
  MBB_Int(mbb, dt->minute, 10, false, 2, 2);
  MBB_Char(mbb, ':');
  MBB_Int(mbb, dt->second, 10, false, 2, 2);
  return 8;
}

int32_t MBB_TimeMs(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 12 >= mbb->limit) return 0;
  MBB_Time(mbb, dt);
  MBB_Char(mbb, '.');
  MBB_Int(mbb, dt->ms, 10, false, 3, 3);
  return 12;
}

int32_t MBB_Datetime(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 19 >= mbb->limit) return 0;
  MBB_Date(mbb, dt);
  MBB_Char(mbb, ' ');
  MBB_Time(mbb, dt);
  return 19;
}

int32_t MBB_DatetimeMs(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 23 >= mbb->limit) return 0;
  MBB_Datetime(mbb, dt);
  MBB_Char(mbb, '.');
  MBB_Int(mbb, dt->ms, 10, false, 3, 3);
  return 23;
}

int32_t MBB_AlarmTime(MBB_t *mbb, const RTC_AlarmCfg_t *alarm)
{
  if(mbb->lock) return 0;
  if(mbb->size + 8 >= mbb->limit) return 0;
  if(alarm->hour_mask) MBB_String(mbb, "**:");
  else {
    MBB_Int(mbb, alarm->hour, 10, false, 2, 2);
    MBB_Char(mbb, ':');
  }
  if(alarm->minute_mask) MBB_String(mbb, "**:");
  else {
    MBB_Int(mbb, alarm->minute, 10, false, 2, 2);
    MBB_Char(mbb, ':');
  }
  if(alarm->second_mask) MBB_String(mbb, "**");
  else {
    MBB_Int(mbb, alarm->second, 10, false, 2, 2);
  }
  return 8;
}

int32_t MBB_Alarm(MBB_t *mbb, const RTC_AlarmCfg_t *alarm)
{
  if(mbb->lock) return 0;
  if(mbb->size + 12 > mbb->limit) return 0;
  if(alarm->day_mask) MBB_String(mbb, (char *)RtcWeekdays[0]);
  else MBB_String(mbb, (char *)RtcWeekdays[alarm->day]);
  MBB_Char(mbb, ' ');
  MBB_AlarmTime(mbb, alarm);
  return 12;
}

//------------------------------------------------------------------------------------------------- crc

int32_t MBB_CrcAppend(MBB_t *mbb, const CRC_t *crc)
{
  if(mbb->limit - mbb->size < crc->width / 8) return 0;
  mbb->size = CRC_Append(crc, mbb->buffer, mbb->size);
  return crc->width / 8;
}

bool MBB_CrcError(MBB_t *mbb, const CRC_t *crc)
{
  if(mbb->size < crc->width / 8) return true;
  if(CRC_Error(crc, mbb->buffer, mbb->size)) return true;
  mbb->size -= crc->width / 8;
  return false;
}

//-------------------------------------------------------------------------------------------------
#ifdef HOST

#include "sys.h"

bool MBB_FileLoad(const char *name, MBB_t *mbb)
{
  uint8_t *bytes;
  uint16_t size = (uint16_t)file_load(name, &bytes);
  if(size > mbb->limit) return false;
  memcpy(mbb->buffer, bytes, size);
  free(bytes);
  mbb->size = size;
  return true;
}

bool MBB_FileSave(const char *name, MBB_t *mbb)
{
  return file_save(name, mbb->buffer, mbb->size);
}

#endif
//-------------------------------------------------------------------------------------------------

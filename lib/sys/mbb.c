// lib/sys/mbb.c

#include "mbb.h"

#include <string.h>
#include "xstring.h"

//------------------------------------------------------------------------------------------ Buffer

status_t MBB_Clear(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  mbb->size = 0;
  return OK;
}

status_t MBB_Copy(MBB_t *dst, const MBB_t *src)
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
  if(secondary && secondary != primary && MBB_Lock(secondary)) {
    MBB_Unlock(primary);
    return ERR;
  }
  return OK;
}

status_t MBB_OffsetRst(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  if(!mbb->_offset) return OK;
  mbb->buffer = mbb->_base;
  mbb->limit += mbb->_offset;
  mbb->size += mbb->_offset;
  mbb->_offset = 0;
  return OK;
}

status_t MBB_OffsetSet(MBB_t *mbb, uint16_t offset)
{
  if(MBB_OffsetRst(mbb)) return ERR;
  if(offset > mbb->limit) return ERR;
  mbb->buffer = mbb->_base + offset;
  mbb->limit -= offset;
  mbb->size = mbb->size > offset ? mbb->size - offset : 0;
  mbb->_offset = offset;
  return OK;
}

//----------------------------------------------------------------------------------------- Writers

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
  if(!str) return 0;
  return MBB_Data(mbb, (const uint8_t *)str, (uint16_t)strlen(str));
}

int32_t MBB_Enter(MBB_t *mbb)
{
  #if(MBB_ENTER_RETURN)
  return MBB_String(mbb, "\r\n");
  #else
  return MBB_Char(mbb, '\n');
  #endif
}

int32_t MBB_Bool(MBB_t *mbb, bool value)
{
  return MBB_String(mbb, value ? "true" : "false");
}

int32_t MBB_DropLastLine(MBB_t *mbb)
{
  if(mbb->lock || mbb->size == 0) return 0;
  int32_t len = 0;
  bool text = false;
  for(int32_t i = mbb->size - 1; i >= 0; i--) {
    uint8_t c = mbb->buffer[i];
    // The trailing line break belongs to the dropped line, the one before it stays
    if((c == '\r' || c == '\n') && text) {
      mbb->size = i + 1;
      return len;
    }
    if(c != '\r' && c != '\n') text = true;
    mbb->size--;
    len++;
  }
  return len;
}

int32_t MBB_Int(MBB_t *mbb, int64_t nbr, uint8_t base, bool sign,
  uint8_t fill_zero, uint8_t fill_space)
{
  if(mbb->lock) return 0;
  int32_t len = (int32_t)itoa_encode(nbr, StrTempMem, base, sign, fill_zero, fill_space);
  if(mbb->size + len > mbb->limit) return 0;
  for(int32_t n = len; n; ) mbb->buffer[mbb->size++] = StrTempMem[--n];
  return len;
}

int32_t MBB_Float(MBB_t *mbb, float nbr, uint8_t accuracy, uint8_t fill_space)
{
  if(mbb->lock) return 0;
  if(isNaN(nbr) || isInf(nbr)) {
    #if(MBB_PRINT_NAN_INF)
    const char *txt = isNaN(nbr) ? "NaN" : (signbit(nbr) ? "-Inf" : "Inf");
    #else
    const char *txt = "-";
    #endif
    uint8_t len = (uint8_t)strlen(txt);
    while(fill_space > len) { MBB_Char(mbb, ' '); fill_space--; }
    return MBB_String(mbb, txt);
  }
  bool negative = nbr < 0;
  for(uint16_t i = 0; i < accuracy; i++) nbr *= 10;
  // Round half away from zero. Scaling lands just under the target, so a bare
  // cast would print `0.01` at accuracy `2` as `0.00`
  nbr += negative ? -0.5f : 0.5f;
  if(!fill_space) fill_space = 1;
  int32_t length = (int32_t)itoa_encode((int32_t)nbr, StrTempMem, 10, true,
    negative ? accuracy + 2 : accuracy + 1, fill_space - 1);
  int32_t n = length + (accuracy ? 1 : 0);
  if(mbb->size + n > mbb->limit) return 0;
  while(length) {
    if(accuracy && length == accuracy) mbb->buffer[mbb->size++] = '.';
    mbb->buffer[mbb->size++] = StrTempMem[--length];
  }
  return n;
}

int32_t MBB_Dec(MBB_t *mbb, int64_t nbr) { return MBB_Int(mbb, nbr, 10, true, 0, 0); }
int32_t MBB_uDec(MBB_t *mbb, uint64_t nbr) { return MBB_Int(mbb, (int64_t)nbr, 10, false, 0, 0); }
int32_t MBB_Hex8(MBB_t *mbb, uint8_t nbr) { return MBB_Int(mbb, nbr, 16, false, 2, 2); }
int32_t MBB_Hex16(MBB_t *mbb, uint16_t nbr) { return MBB_Int(mbb, nbr, 16, false, 4, 4); }
int32_t MBB_Hex32(MBB_t *mbb, uint32_t nbr) { return MBB_Int(mbb, nbr, 16, false, 8, 8); }
int32_t MBB_Bin8(MBB_t *mbb, uint8_t nbr) { return MBB_Int(mbb, nbr, 2, false, 8, 8); }
int32_t MBB_Nbr(MBB_t *mbb, float nbr) { return MBB_Float(mbb, nbr, 3, 0); }

// Two-digit field followed by a separator, the piece every date and time is built from
static void field(MBB_t *mbb, uint8_t value, char sep)
{
  MBB_Int(mbb, value, 10, false, 2, 2);
  if(sep) MBB_Char(mbb, sep);
}

int32_t MBB_Date(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 10 >= mbb->limit) return 0;
  MBB_String(mbb, "20");
  field(mbb, dt->year, '-');
  field(mbb, dt->month, '-');
  field(mbb, dt->month_day, 0);
  return 10;
}

int32_t MBB_Time(MBB_t *mbb, const RTC_Datetime_t *dt)
{
  if(mbb->lock) return 0;
  if(mbb->size + 8 >= mbb->limit) return 0;
  field(mbb, dt->hour, ':');
  field(mbb, dt->minute, ':');
  field(mbb, dt->second, 0);
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
  else field(mbb, alarm->hour, ':');
  if(alarm->minute_mask) MBB_String(mbb, "**:");
  else field(mbb, alarm->minute, ':');
  if(alarm->second_mask) MBB_String(mbb, "**");
  else field(mbb, alarm->second, 0);
  return 8;
}

int32_t MBB_Alarm(MBB_t *mbb, const RTC_AlarmCfg_t *alarm)
{
  if(mbb->lock) return 0;
  if(mbb->size + 12 > mbb->limit) return 0;
  MBB_String(mbb, RtcWeekdays[alarm->day_mask ? 0 : alarm->day]);
  MBB_Char(mbb, ' ');
  MBB_AlarmTime(mbb, alarm);
  return 12;
}

//----------------------------------------------------------------------------------------- Records

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
  if(margin >= mbb->limit) return 0;
  uint16_t capacity = (mbb->limit - margin) / mbb->struct_size;
  uint16_t used = mbb->size / mbb->struct_size;
  return capacity > used ? capacity - used : 0;
}

int32_t MBB_StructShift(MBB_t *mbb, uint16_t count)
{
  if(mbb->lock) return 0;
  int32_t shift_bytes = mbb->struct_size * count;
  int32_t remaining = (int32_t)mbb->size - shift_bytes;
  if(remaining <= 0) {
    int32_t dropped = mbb->size;
    mbb->size = 0;
    return -dropped;
  }
  memmove(mbb->buffer, &mbb->buffer[shift_bytes], (size_t)remaining);
  mbb->size = remaining;
  return -shift_bytes;
}

int32_t MBB_StructDrop(MBB_t *mbb, uint16_t count)
{
  if(mbb->lock || !count) return 0;
  int32_t size = mbb->struct_size * count;
  if(size > mbb->size) size = mbb->size;
  mbb->size -= size;
  return -size;
}

int32_t MBB_StructGet(const MBB_t *mbb, uint16_t index, uint8_t *dst)
{
  const uint8_t *src = MBB_StructPeek(mbb, index);
  if(!src) return 0;
  memcpy(dst, src, mbb->struct_size);
  return mbb->struct_size;
}

const uint8_t *MBB_StructPeek(const MBB_t *mbb, uint16_t index)
{
  uint32_t pos = index * mbb->struct_size;
  if(pos + mbb->struct_size > mbb->size) return NULL;
  return &mbb->buffer[pos];
}

//------------------------------------------------------------------------------------- Persistence

status_t MBB_FlashSave(MBB_t *mbb)
{
  if(!mbb->flash_page) return ERR;
  if(FLASH_Compare(mbb->flash_page, mbb->buffer, mbb->size)) return OK;
  return FLASH_Save(mbb->flash_page, mbb->buffer, mbb->size);
}

status_t MBB_FlashLoad(MBB_t *mbb)
{
  if(mbb->lock) return ERR;
  if(!mbb->flash_page) return ERR;
  // Record length is the first word of the page and `FLASH_Load` copies it unchecked.
  // A record written by a firmware with a larger buffer would overrun this one
  if((uint16_t)FLASH_Read(FLASH_GetAddress(mbb->flash_page, 0)) > mbb->limit) return ERR;
  mbb->size = FLASH_Load(mbb->flash_page, mbb->buffer);
  return mbb->size ? OK : ERR;
}

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

#ifdef HOST

#include <stdlib.h>
#include "sys.h"

bool MBB_FileLoad(const char *name, MBB_t *mbb)
{
  uint8_t *bytes;
  size_t size = file_load(name, &bytes);
  if(!bytes) return false;
  if(size > mbb->limit) {
    free(bytes);
    return false;
  }
  memcpy(mbb->buffer, bytes, size);
  free(bytes);
  mbb->size = (uint16_t)size;
  return true;
}

bool MBB_FileSave(const char *name, MBB_t *mbb)
{
  return file_save(name, mbb->buffer, mbb->size);
}

#endif
//-------------------------------------------------------------------------------------------------

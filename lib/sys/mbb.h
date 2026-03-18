/** @file lib/sys/mbb.h */

#ifndef MBB_H_
#define MBB_H_

#include <stdarg.h>
#include "flash.h"
#include "vrts.h"
#include "rtc.h"
#include "crc.h"
#include "xdef.h"
#include "xstring.h"
#include "xmath.h"
#include "main.h"

#ifndef MBB_PRINT_NAN_INF
  #define MBB_PRINT_NAN_INF 0
#endif

#ifndef MBB_ENTER_RETURN
  #define MBB_ENTER_RETURN 1
#endif

//-------------------------------------------------------------------------------------------------

#define MBB_Init2(name, size) \
  static uint8_t name##_buffer[size]; \
  MBB_t name = { \
    .name = #name, \
    .buffer = name##_buffer, \
    .limit = (size), \
    ._base = name##_buffer \
  }

#define MBB_Init3(name, size, flash_page) \
  static uint8_t name##_buffer[size]; \
  MBB_t name = { \
    .name = #name, \
    .buffer = name##_buffer, \
    .limit = (size), \
    .flash_page = (flash_page), \
    ._base = name##_buffer \
  }

#define MBB_Init4(name, size, struct_size, StructPrint) \
  static uint8_t name##_buffer[size]; \
  MBB_t name = { \
    .name = #name, \
    .buffer = name##_buffer, \
    .limit = (size), \
    .struct_size = (struct_size), \
    .StructPrint = (StructPrint), \
    ._base = name##_buffer \
  }

#define MBB_Init5(name, size, flash_page, struct_size, StructPrint) \
  static uint8_t name##_buffer[size]; \
  MBB_t name = { \
    .name = #name, \
    .buffer = name##_buffer, \
    .limit = (size), \
    .flash_page = (flash_page), \
    .struct_size = (struct_size), \
    .StructPrint = (StructPrint), \
    ._base = name##_buffer \
  }

/**
 * @brief Statically define MBB_t with internal buffer.
 * @param name Variable name (also used as MBB_t::name)
 * @param size Buffer capacity in bytes
 * @param ... Optional: flash_page, struct_size, StructPrint
 */
#define MBB_Init(...) \
  _args5(__VA_ARGS__, MBB_Init5, MBB_Init4, MBB_Init3, MBB_Init2)(__VA_ARGS__)

/**
 * @brief Memory byte buffer control structure.
 * @param name Logical name (for shell/debug)
 * @param buffer Pointer to RAM buffer
 * @param size Current data size
 * @param limit Max buffer capacity
 * @param flash_page Flash page for persistence (0 = disabled)
 * @param struct_size Size of stored struct (0 = raw mode)
 * @param StructPrint Print callback for struct display
 * @param lock Mutex flag
 */
typedef struct {
  const char *name;
  uint8_t *buffer;
  uint16_t size;
  uint16_t limit;
  uint8_t flash_page;
  uint16_t struct_size;
  int32_t (*StructPrint)(void *);
  bool lock;
  uint8_t *_base;
  uint16_t _offset;
} MBB_t;

//-------------------------------------------------------------------------------------------------

status_t MBB_Clear(MBB_t *mbb);
status_t MBB_Copy(MBB_t *dst, MBB_t *src);
status_t MBB_Save(MBB_t *mbb, const uint8_t *data, uint16_t size);
status_t MBB_Append(MBB_t *mbb, const uint8_t *data, uint16_t size);
status_t MBB_Lock(MBB_t *mbb);
void MBB_Unlock(MBB_t *mbb);
status_t MBB_Lock2(MBB_t *primary, MBB_t *secondary);

int32_t MBB_Char(MBB_t *mbb, uint8_t data);
int32_t MBB_Char16(MBB_t *mbb, uint16_t data);
int32_t MBB_Char32(MBB_t *mbb, uint32_t data);
int32_t MBB_Char64(MBB_t *mbb, uint64_t data);
int32_t MBB_Data(MBB_t *mbb, const uint8_t *data, uint16_t len);
int32_t MBB_String(MBB_t *mbb, const char *str);
int32_t MBB_Enter(MBB_t *mbb);
int32_t MBB_DropLastLine(MBB_t *mbb);
int32_t MBB_Bool(MBB_t *mbb, bool value);

int32_t MBB_Int(MBB_t *mbb, int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space);
int32_t MBB_Float(MBB_t *mbb, float nbr, uint8_t accuracy, uint8_t fill_space);
int32_t MBB_Dec(MBB_t *mbb, int64_t nbr);
int32_t MBB_uDec(MBB_t *mbb, uint64_t nbr);
int32_t MBB_Hex8(MBB_t *mbb, uint8_t nbr);
int32_t MBB_Hex16(MBB_t *mbb, uint16_t nbr);
int32_t MBB_Hex32(MBB_t *mbb, uint32_t nbr);
int32_t MBB_Bin8(MBB_t *mbb, uint8_t nbr);
int32_t MBB_Nbr(MBB_t *mbb, float nbr);

int32_t MBB_StructAdd(MBB_t *mbb, const uint8_t *object);
uint16_t MBB_StructCount(const MBB_t *mbb);
uint16_t MBB_StructFree(const MBB_t *mbb, uint16_t margin);
int32_t MBB_StructShift(MBB_t *mbb, uint16_t count);
int32_t MBB_StructDrop(MBB_t *mbb, uint16_t count);
int32_t MBB_StructGet(const MBB_t *mbb, uint16_t index, uint8_t *dst);
const uint8_t *MBB_StructPeek(const MBB_t *mbb, uint16_t index);

status_t MBB_OffsetSet(MBB_t *mbb, uint16_t offset);
status_t MBB_OffsetRst(MBB_t *mbb);

status_t MBB_FlashSave(MBB_t *mbb);
status_t MBB_FlashLoad(MBB_t *mbb);

int32_t MBB_Date(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_Time(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_TimeMs(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_Datetime(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_DatetimeMs(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_AlarmTime(MBB_t *mbb, const RTC_AlarmCfg_t *alarm);
int32_t MBB_Alarm(MBB_t *mbb, const RTC_AlarmCfg_t *alarm);

int32_t MBB_CrcAppend(MBB_t *mbb, const CRC_t *crc);
bool MBB_CrcError(MBB_t *mbb, const CRC_t *crc);

#ifdef HOST
  bool MBB_FileLoad(const char *name, MBB_t *mbb);
  bool MBB_FileSave(const char *name, MBB_t *mbb);
#endif

//-------------------------------------------------------------------------------------------------
#endif
// lib/sys/mbb.h

#ifndef MBB_H_
#define MBB_H_

#include <stdbool.h>
#include <stdint.h>
#include "flash.h"
#include "rtc.h"
#include "crc.h"
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef MBB_PRINT_NAN_INF
  // `MBB_Float` prints `NaN` and `Inf` by name, otherwise a single `-`
  #define MBB_PRINT_NAN_INF 0
#endif

#ifndef MBB_ENTER_RETURN
  // `MBB_Enter` writes `\r\n`, otherwise `\n`
  #define MBB_ENTER_RETURN 1
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Memory byte buffer: a byte sink with printers, a record store and a flash mirror.
 * @param[in] name Logical name, the shell addresses the buffer by it
 * @param[in] buffer RAM storage
 * @param[in,out] size Bytes held
 * @param[in] limit Capacity [B]
 * @param[in] flash_page Page of the flash mirror, `0` = none
 * @param[in] struct_size Record size of the `MBB_Struct...` API [B], `0` = raw bytes
 * @param[in] StructPrint Printer of one record for the shell
 * @param[in,out] lock Writes refused while set, the shell takes it during a data transfer
 * Internal:
 * @param _base Start of the storage, `buffer` moves with `MBB_OffsetSet`
 * @param _offset Bytes `buffer` sits past `_base`
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
  // internal
  uint8_t *_base;
  uint16_t _offset;
} MBB_t;

#define _MBB_Init2(id, size) \
  static uint8_t id##_buffer[size]; \
  MBB_t id = { \
    .name = #id, \
    .buffer = id##_buffer, \
    .limit = (size), \
    ._base = id##_buffer \
  }

#define _MBB_Init3(id, size, flash_page) \
  static uint8_t id##_buffer[size]; \
  MBB_t id = { \
    .name = #id, \
    .buffer = id##_buffer, \
    .limit = (size), \
    .flash_page = (flash_page), \
    ._base = id##_buffer \
  }

#define _MBB_Init4(id, size, struct_size, StructPrint) \
  static uint8_t id##_buffer[size]; \
  MBB_t id = { \
    .name = #id, \
    .buffer = id##_buffer, \
    .limit = (size), \
    .struct_size = (struct_size), \
    .StructPrint = (StructPrint), \
    ._base = id##_buffer \
  }

#define _MBB_Init5(id, size, flash_page, struct_size, StructPrint) \
  static uint8_t id##_buffer[size]; \
  MBB_t id = { \
    .name = #id, \
    .buffer = id##_buffer, \
    .limit = (size), \
    .flash_page = (flash_page), \
    .struct_size = (struct_size), \
    .StructPrint = (StructPrint), \
    ._base = id##_buffer \
  }

/**
 * @brief Declare an `MBB_t` with its storage, named after the variable.
 * @param id Variable name, also `MBB_t.name`
 * @param size Capacity [B]
 * @param ... `flash_page`, or `struct_size, StructPrint`, or all three in that order
 */
#define MBB_Init(...) \
  _args5(__VA_ARGS__, _MBB_Init5, _MBB_Init4, _MBB_Init3, _MBB_Init2)(__VA_ARGS__)

//------------------------------------------------------------------------------------------ Buffer

// Whole-buffer operations, `ERR` when locked or the data does not fit
status_t MBB_Clear(MBB_t *mbb);
status_t MBB_Copy(MBB_t *dst, const MBB_t *src);
status_t MBB_Save(MBB_t *mbb, const uint8_t *data, uint16_t size);
status_t MBB_Append(MBB_t *mbb, const uint8_t *data, uint16_t size);

// Lock refuses writes, `ERR` when taken already. `MBB_Lock2` takes both or neither
status_t MBB_Lock(MBB_t *mbb);
void MBB_Unlock(MBB_t *mbb);
status_t MBB_Lock2(MBB_t *primary, MBB_t *secondary);

/**
 * @brief Move the start of the buffer, the bytes before it stay in storage.
 * @param[in,out] mbb Buffer
 * @param[in] offset Bytes to skip from the storage start
 * @return `OK`, `ERR` when locked or `offset` exceeds the capacity
 */
status_t MBB_OffsetSet(MBB_t *mbb, uint16_t offset);

// Bring the start back to the storage
status_t MBB_OffsetRst(MBB_t *mbb);

//----------------------------------------------------------------------------------------- Writers

// Every writer returns the bytes appended, `0` when locked or out of room.
// Multi-byte values land big-endian
int32_t MBB_Char(MBB_t *mbb, uint8_t data);
int32_t MBB_Char16(MBB_t *mbb, uint16_t data);
int32_t MBB_Char32(MBB_t *mbb, uint32_t data);
int32_t MBB_Char64(MBB_t *mbb, uint64_t data);
int32_t MBB_Data(MBB_t *mbb, const uint8_t *data, uint16_t len);
int32_t MBB_String(MBB_t *mbb, const char *str);
int32_t MBB_Enter(MBB_t *mbb);
int32_t MBB_Bool(MBB_t *mbb, bool value);

// Drop the last line with its terminator, returns the bytes removed
int32_t MBB_DropLastLine(MBB_t *mbb);

/**
 * @brief Integer as text, parameters as in `itoa_encode`.
 * @param[in,out] mbb Buffer
 * @param[in] nbr Number to write
 * @param[in] base Numeric base, `2` to `36`
 * @param[in] sign `true` treats `nbr` as signed
 * @param[in] fill_zero Least digits, padded with leading zeros
 * @param[in] fill_space Least width, padded with leading spaces
 * @return Bytes appended
 */
int32_t MBB_Int(MBB_t *mbb, int64_t nbr, uint8_t base, bool sign,
  uint8_t fill_zero, uint8_t fill_space);

/**
 * @brief Float as fixed-point text, rounded half away from zero.
 * @param[in,out] mbb Buffer
 * @param[in] nbr Number to write
 * @param[in] accuracy Decimal places
 * @param[in] fill_space Least width, padded with leading spaces
 * @return Bytes appended
 */
int32_t MBB_Float(MBB_t *mbb, float nbr, uint8_t accuracy, uint8_t fill_space);

int32_t MBB_Dec(MBB_t *mbb, int64_t nbr);
int32_t MBB_uDec(MBB_t *mbb, uint64_t nbr);
int32_t MBB_Hex8(MBB_t *mbb, uint8_t nbr);
int32_t MBB_Hex16(MBB_t *mbb, uint16_t nbr);
int32_t MBB_Hex32(MBB_t *mbb, uint32_t nbr);
int32_t MBB_Bin8(MBB_t *mbb, uint8_t nbr);
int32_t MBB_Nbr(MBB_t *mbb, float nbr); // three decimal places

// Date and time fields as text: `YYYY-MM-DD`, `hh:mm:ss`, `hh:mm:ss.mmm`, alarm as `**:mm:ss`
int32_t MBB_Date(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_Time(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_TimeMs(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_Datetime(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_DatetimeMs(MBB_t *mbb, const RTC_Datetime_t *dt);
int32_t MBB_AlarmTime(MBB_t *mbb, const RTC_AlarmCfg_t *alarm);
int32_t MBB_Alarm(MBB_t *mbb, const RTC_AlarmCfg_t *alarm);

//----------------------------------------------------------------------------------------- Records

// Fixed-size records of `struct_size` bytes. Shift and drop return the bytes removed,
// negative, so a caller summing writer results stays balanced
int32_t MBB_StructAdd(MBB_t *mbb, const uint8_t *object);
uint16_t MBB_StructCount(const MBB_t *mbb);
int32_t MBB_StructShift(MBB_t *mbb, uint16_t count); // drop the oldest `count`
int32_t MBB_StructDrop(MBB_t *mbb, uint16_t count);  // drop the newest `count`
int32_t MBB_StructGet(const MBB_t *mbb, uint16_t index, uint8_t *dst);
const uint8_t *MBB_StructPeek(const MBB_t *mbb, uint16_t index);

/**
 * @brief Records that still fit while `margin` bytes stay free.
 *   The margin is a reserve asked for at query time, never enforced on writes,
 *   so the buffer can already hold more records than it leaves room for.
 * @param[in] mbb Buffer
 * @param[in] margin Bytes to keep free
 * @return Records that fit
 */
uint16_t MBB_StructFree(const MBB_t *mbb, uint16_t margin);

//------------------------------------------------------------------------------------- Persistence

// Flash mirror on `flash_page`: save skips a page holding the same bytes,
// load refuses a record larger than the buffer
status_t MBB_FlashSave(MBB_t *mbb);
status_t MBB_FlashLoad(MBB_t *mbb);

// Checksum appended to the content and checked from its tail
int32_t MBB_CrcAppend(MBB_t *mbb, const CRC_t *crc);
bool MBB_CrcError(MBB_t *mbb, const CRC_t *crc);

#ifdef HOST
  // Host build: the buffer as a file on disk
  bool MBB_FileLoad(const char *name, MBB_t *mbb);
  bool MBB_FileSave(const char *name, MBB_t *mbb);
#endif

//-------------------------------------------------------------------------------------------------
#endif

#ifndef FILE_H_
#define FILE_H_

#include <stdarg.h>
#include "flash.h"
#include "vrts.h"
#include "rtc.h"
#include "crc.h"
#include "xdef.h"
#include "xstring.h"
#include "xmath.h"
#include "main.h"

#ifndef FILE_PRINT_NAN_INF
  /** @brief Enable printing of `NaN`/`Inf` in `FILE_Float` */
  #define FILE_PRINT_NAN_INF 0
#endif

#ifndef FILE_ENTER_RETURN
  /** @brief Use `\r\n` instead of `\n` in `FILE_Enter` */
  #define FILE_ENTER_RETURN 1
#endif

//-------------------------------------------------------------------------------------------------

#define FILE_Init2(file_name, file_size) \
  static uint8_t file_name##_buffer[file_size]; \
  FILE_t file_name = { \
    .name = #file_name, \
    .buffer = file_name##_buffer, \
    .size = 0, \
    .limit = (file_size), \
    .flash_page = 0, \
    .struct_size = 0, \
    .StructPrint = NULL, \
    .lock = false, \
    .base = file_name##_buffer, \
    .offset = 0, \
  }

#define FILE_Init3(file_name, file_size, flash_page) \
  static uint8_t file_name##_buffer[file_size]; \
  FILE_t file_name = { \
    .name = #file_name, \
    .buffer = file_name##_buffer, \
    .size = 0, \
    .limit = (file_size), \
    .flash_page = (flash_page), \
    .struct_size = 0, \
    .StructPrint = NULL, \
    .lock = false, \
    .base = file_name##_buffer, \
    .offset = 0, \
  }

#define FILE_Init4(file_name, file_size, struct_size, StructPrint) \
  static uint8_t file_name##_buffer[file_size]; \
  FILE_t file_name = { \
    .name = #file_name, \
    .buffer = file_name##_buffer, \
    .size = 0, \
    .limit = (file_size), \
    .flash_page = 0, \
    .struct_size = (struct_size), \
    .StructPrint = (StructPrint), \
    .lock = false, \
    .base = file_name##_buffer, \
    .offset = 0, \
  }

#define FILE_Init5(file_name, file_size, flash_page, struct_size, StructPrint) \
  static uint8_t file_name##_buffer[file_size]; \
  FILE_t file_name = { \
    .name = #file_name, \
    .buffer = file_name##_buffer, \
    .size = 0, \
    .limit = (file_size), \
    .flash_page = (flash_page), \
    .struct_size = (struct_size), \
    .StructPrint = (StructPrint), \
    .lock = false, \
    .base = file_name##_buffer, \
    .offset = 0, \
  }

/**
 * @brief Statically define FILE_t with internal buffer.
 * @param[in] file_name Variable name (also used as FILE_t::name).
 * @param[in] file_size Buffer capacity in bytes.
 * @param[in] ... Optional arguments by position:
 *   3 args total: `flash_page`,
 *   4 args total: `struct_size`, `StructPrint`,
 *   5 args total: `flash_page`, `struct_size`, `StructPrint`
 */
#define FILE_Init(...) \
  _args5(__VA_ARGS__, FILE_Init5, FILE_Init4, FILE_Init3, FILE_Init2)(__VA_ARGS__)

/**
 * @brief File control structure.
 * @param[in] name File logical name (used e.g. in bash)
 * @param[in] buffer Pointer to RAM buffer
 * @param[in] size Current size of data in buffer
 * @param[in] limit Max allowed size of buffer
 * @param[in] flash_page Flash page index for persistent save
 * @param[in] struct_size Size of associated struct
 * @param[in] StructPrint Callback for printing associated struct
 * @param lock Lock flag for exclusive access
 * @param base Helper pointer to real start of RAM buffer (used only for offset restore)
 * @param offset Current offset in buffer
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
  uint8_t *base;
  uint16_t offset;
} FILE_t;

//-------------------------------------------------------------------------------------------------

status_t FILE_Clear(FILE_t *file);
status_t FILE_Copy(FILE_t *file_to, FILE_t *file_from);
status_t FILE_Save(FILE_t *file, const uint8_t *data, uint16_t size);
status_t FILE_Append(FILE_t *file, const uint8_t *data, uint16_t size);
status_t FILE_Lock(FILE_t *file);
void FILE_Unlock(FILE_t *file);
status_t FILE_Lock2(FILE_t *primary, FILE_t *secondary);

int32_t FILE_Char(FILE_t *file, uint8_t data);
int32_t FILE_Char16(FILE_t *file, uint16_t data);
int32_t FILE_Char32(FILE_t *file, uint32_t data);
int32_t FILE_Char64(FILE_t *file, uint64_t data);
int32_t FILE_Data(FILE_t *file, const uint8_t *data, uint16_t len);
int32_t FILE_String(FILE_t *file, const char *string);
int32_t FILE_Enter(FILE_t *file);
int32_t FILE_DropLastLine(FILE_t *file);
int32_t FILE_Bool(FILE_t *file, bool value);

int32_t FILE_Int(FILE_t *file, int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space);
int32_t FILE_Float(FILE_t *file, float nbr, uint8_t accuracy, uint8_t fill_space);
int32_t FILE_Dec(FILE_t *file, int64_t nbr);
int32_t FILE_uDec(FILE_t *file, uint64_t nbr);
int32_t FILE_Hex8(FILE_t *file, uint8_t nbr);
int32_t FILE_Hex16(FILE_t *file, uint16_t nbr);
int32_t FILE_Hex32(FILE_t *file, uint32_t nbr);
int32_t FILE_Bin8(FILE_t *file, uint8_t nbr);
int32_t FILE_Nbr(FILE_t *file, float nbr);

int32_t FILE_Struct_Add(FILE_t *file, const uint8_t *object);
uint16_t FILE_Struct_Count(FILE_t *file);
uint16_t FILE_Struct_FreeSpace(FILE_t *file, uint16_t margin);
int32_t FILE_Struct_Move(FILE_t *file, uint16_t count);
int32_t FILE_Struct_Drop(FILE_t *file, uint16_t count);
int32_t FILE_Struct_Get(FILE_t *file, uint16_t index, uint8_t *object);
const uint8_t *FILE_Struct_Peek(FILE_t *file, uint16_t index);
status_t FILE_Offset_Set(FILE_t *file, uint16_t offset);
status_t FILE_Offset_Rst(FILE_t *file);

status_t FILE_Flash_Save(FILE_t *file);
status_t FILE_Flash_Load(FILE_t *file);
int32_t FILE_Date(FILE_t *file, RTC_Datetime_t *datetime);
int32_t FILE_Time(FILE_t *file, RTC_Datetime_t *datetime);
int32_t FILE_TimeMs(FILE_t *file, RTC_Datetime_t *datetime);
int32_t FILE_Datetime(FILE_t *file, RTC_Datetime_t *datetime);
int32_t FILE_DatetimeMs(FILE_t *file, RTC_Datetime_t *datetime);
int32_t FILE_AlarmTime(FILE_t *file, RTC_Alarm_t *alarm);
int32_t FILE_Alarm(FILE_t *file, RTC_Alarm_t *alarm);
int32_t FILE_CrcAppend(FILE_t *file, const CRC_t *crc);
bool FILE_CrcError(FILE_t *file, const CRC_t *crc);

//-------------------------------------------------------------------------------------------------
#endif

/** @file lib/ext/xstring.c */

#include "xstring.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Compute hash of string using djb2 algorithm.
 * @param str Null-terminated string.
 * @return Hash value as `uint32_t`.
 */
uint32_t hash_djb2(const char *str)
{
  uint32_t hash = 5381;
  uint32_t c;
  while((c = *str++)) {
    hash = ((hash << 5) + hash) + c; // hash * 33 + c
  }
  return hash;
}

/**
 * @brief Compute hash of string using djb2 algorithm (case-insensitive).
 * All chars are mapped to lowercase with `LowerCase` table.
 * @param str Null-terminated string.
 * @return Hash value as `uint32_t`.
 */
uint32_t hash_djb2_ci(const char *str)
{
  uint32_t hash = 5381;
  uint32_t c;
  while(*str) {
    c = (uint8_t)LowerCase[(uint8_t)*str++];
    hash = ((hash << 5) + hash) + c; // hash * 33 + c
  }
  return hash;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Helper: encode integer digits into reversed string.
 * @param nbr Number to convert (unsigned).
 * @param str Output buffer (digits reversed).
 * @param base Numeric base (2–36).
 * @param sign Include sign (true = signed number, false = unsigned).
 * @param fill_zero Minimal digit count (filled with zeros).
 * @param fill_space Minimal string length (filled with spaces).
 * @return Number of chars written to `str`.
 */
uint8_t itoa_encode(int64_t nbr, char *str, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space)
{
  static const char Digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if(base < 2 || base > 36) return 0;
  uint8_t n = 0;
  bool is_negative = false;
  uint64_t unbr;
  // normalize padding parameters
  if(fill_zero == 0) fill_zero = 1;
  if(fill_space < fill_zero) fill_space = fill_zero;
  // handle sign
  if(sign && nbr < 0) {
    is_negative = true;
    unbr = (uint64_t)(-nbr);
  }
  else unbr = (uint64_t)nbr;
  // encode digits in reverse order
  do {
    str[n++] = Digits[unbr % base];
    unbr /= base;
  } while(unbr > 0);
  while(n < (fill_zero - (is_negative ? 1 : 0))) str[n++] = '0'; // zero padding (exclude space for '-')
  if(is_negative)  str[n++] = '-'; // add sign if needed
  while(n < fill_space) str[n++] = ' '; // space padding to minimum width
  return n;
}

char StrTempMem[64 + 2]; // Enough for 64-bit binary number (64 bits + sign + '\0')

/**
 * @brief Convert 64-bit integer to string (backend for printf).
 * Supports signed/unsigned, base 2–36, zero-padding, space-padding.
 * @param nbr Number to convert.
 * @param base Numeric base (e.g. 10 for decimal, 16 for hex).
 * @param sign If true, treat as signed and show sign if negative.
 * @param fill_zero Minimum digits (zero padded).
 * @param fill_space Minimum field width (space padded).
 * @return Allocated string (must be freed with heap_clear or manually).
 */
char *str_from_int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space)
{
  uint8_t len = itoa_encode(nbr, StrTempMem, base, sign, fill_zero, fill_space);
  char *string = heap_new(len + 1);
  if(!string) return NULL;
  for(uint8_t i = 0; i < len; i++) {
    string[i] = StrTempMem[len - 1 - i];
  }
  string[len] = '\0';
  return string;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Check if `str` can be parsed as `uint16_t`.
 * Accepts decimal, hexadecimal (`0x` prefix) and binary (`0b` prefix).
 * Validates characters and range (0..65535).
 * @param str Input string.
 * @return `true` if ok, `false` if otherwise.
 */
bool str_is_u16(const char *str)
{
  if(!str || *str == '\0') return false;
  if(str[0] == '0' && str[1] == 'b' && str[2] != '\0') {
    str += 2;
    if(*str == '\0') return false;
    size_t bin_digits = 0;
    while(*str) {
      if(*str != '0' && *str != '1') return false;
      bin_digits++;
      str++;
    }
    if(bin_digits > 16) return false;
  }
  else if(str[0] == '0' && str[1] == 'x' && str[2] != '\0') {
    str += 2;
    size_t hex_digits = 0;
    while(*str) {
      if(!((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'F') || (*str >= 'a' && *str <= 'f'))) return false;
      hex_digits++;
      str++;
    }
    if(hex_digits > 4) return false;
  }
  else {
    size_t dec_digits = 0;
    while(*str) {
      if(*str < '0' || *str > '9') return false;
      dec_digits++;
      str++;
    }
    if(dec_digits > 5) return false;
    if(dec_digits == 5) {
      const char *start = str - dec_digits;
      if(strncmp(start, "65535", 5) > 0) return false;
    }
  }
  return true;
}

/**
 * @brief Check if `str` can be parsed as `int16_t`.
 * Accepts decimal numbers. Validates characters and range (-32768..32767).
 * @param str Input string.
 * @return `true` if ok, `false` if otherwise.
 */
bool str_is_i16(const char *str)
{
  if(!str || *str == '\0') return false;
  bool is_negative = false;
  if(*str == '-') {
    is_negative = true;
    str++;
  }
  size_t dec_digits = 0;
  while(*str) {
    if(*str < '0' || *str > '9') return false;
    dec_digits++;
    str++;
  }
  if(dec_digits > 5) return false;
  if(dec_digits == 5) {
    const char *start = str - dec_digits;
    if(!is_negative && strncmp(start, "32767", 5) > 0) return false;
    else if(is_negative && strncmp(start, "32768", 5) > 0) return false;
  }
  return true;
}

/**
 * @brief Check if `str` can be parsed as `uint32_t`.
 * Accepts decimal, hexadecimal (`0x` prefix) and binary (`0b` prefix).
 * Validates characters and range (0..4294967295).
 * @param str Input string.
 * @return `true` if ok, `false` if otherwise.
 */
bool str_is_u32(const char *str)
{
  if(!str || *str == '\0') return false;
  if(str[0] == '0' && str[1] == 'b' && str[2] != '\0') {
    str += 2;
    if(*str == '\0') return false;
    size_t bin_digits = 0;
    while(*str) {
      if(*str != '0' && *str != '1') return false;
      bin_digits++;
      str++;
    }
    if(bin_digits > 32) return false;
  }
  else if(str[0] == '0' && str[1] == 'x' && str[2] != '\0') {
    str += 2;
    size_t hex_digits = 0;
    while(*str) {
      if(!((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'F') || (*str >= 'a' && *str <= 'f'))) return false;
      hex_digits++;
      str++;
    }
    if(hex_digits > 8) return false;
  }
  else {
    size_t dec_digits = 0;
    while(*str) {
      if(*str < '0' || *str > '9') return false;
      dec_digits++;
      str++;
    }
    if(dec_digits > 10) return false;
    if(dec_digits == 10) {
      const char *start = str - dec_digits;
      if(strncmp(start, "4294967295", 10) > 0) return false;
    }
  }
  return true;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Check if `str` can be parsed as `int32_t`.
 * Accepts decimal numbers. Validates characters and range (-2147483648..2147483647).
 * @param str Input string.
 * @return `true` if ok, `false` if otherwise.
 */
bool str_is_i32(const char *str) 
{
  if(!str || *str == '\0') return false;
  bool is_negative = false;
  if(*str == '-') {
    is_negative = true;
    str++;
  }
  size_t dec_digits = 0;
  while(*str) {
    if(*str < '0' || *str > '9') return false;
    dec_digits++;
    str++;
  }
  if(dec_digits > 10) return false;
  if(dec_digits == 10) {
    const char *start = str - dec_digits;
    if(!is_negative && strncmp(start, "2147483647", 10) > 0) return false;
    else if(is_negative && strncmp(start, "2147483648", 10) > 0) return false;
  }
  return true;
}

/**
 * @brief Convert string to 32-bit number.
 * Supports decimal, hexadecimal (`0x` prefix) and binary (`0b` prefix).
 * Negative values are allowed only in decimal form and are returned
 * as two's complement inside `uint32_t`. Cast to `int32_t` if signed
 * interpretation is required.
 * @note Use validator functions (`str_is_i16`, `str_is_u32`, etc.)
 * before calling to ensure format and range are valid.
 * @param str Input string.
 * @return Number as `uint32_t` (cast as `int32_t`, `uint16_t` or `int16_t` if needed).
 */
uint32_t str_to_int(const char *str) 
{
  int base = 10;
  bool is_negative = false;
  if(*str == '-') {
    is_negative = true;
    str++;
  }
  if(str[0] == '0' && str[1] == 'b') {
    base = 2;
    str += 2;
  } 
  else if(str[0] == '0' && str[1] == 'x') {
    base = 16;
    str += 2;
  }
  char *endptr;
  uint32_t result = strtol(str, &endptr, base);
  return is_negative ? -(int32_t)result : (int32_t)result;
}

/**
 * @brief Check if `str` can be parsed as `uint64_t`.
 * Accepts decimal, hexadecimal (`0x` prefix) and binary (`0b` prefix).
 * Validates characters and range (0..18446744073709551615).
 * @param str Input string.
 * @return `true` if ok, `false` if otherwise.
 */
bool str_is_u64(const char *str)
{
  if(!str || *str == '\0') return false;
  if(str[0] == '0' && str[1] == 'b' && str[2] != '\0') {
    str += 2;
    if(*str == '\0') return false;
    size_t bin_digits = 0;
    while(*str) {
      if(*str != '0' && *str != '1') return false;
      bin_digits++;
      str++;
    }
    if(bin_digits > 64) return false;
  }
  else if(str[0] == '0' && str[1] == 'x' && str[2] != '\0') {
    str += 2;
    size_t hex_digits = 0;
    while(*str) {
      if(!((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'F') || (*str >= 'a' && *str <= 'f'))) return false;
      hex_digits++;
      str++;
    }
    if(hex_digits > 16) return false;
  }
  else {
    size_t dec_digits = 0;
    while(*str) {
      if(*str < '0' || *str > '9') return false;
      dec_digits++;
      str++;
    }
    if(dec_digits > 20) return false;
    if(dec_digits == 20) {
      const char *start = str - dec_digits;
      if(strncmp(start, "18446744073709551615", 20) > 0) return false;
    }
  }
  return true;
}

/**
 * @brief Check if `str` can be parsed as `int64_t`.
 * Accepts decimal numbers. Validates characters and range
 * (-9223372036854775808..9223372036854775807).
 * @param str Input string.
 * @return `true` if ok, `false` if otherwise.
 */
bool str_is_i64(const char *str)
{
  if(!str || *str == '\0') return false;
  bool is_negative = false;
  if(*str == '-') {
    is_negative = true;
    str++;
  }
  size_t dec_digits = 0;
  while(*str) {
    if(*str < '0' || *str > '9') return false;
    dec_digits++;
    str++;
  }
  if(dec_digits > 19) return false;
  if(dec_digits == 19) {
    const char *start = str - dec_digits;
    if(!is_negative && strncmp(start, "9223372036854775807", 19) > 0) return false;
    else if(is_negative && strncmp(start, "9223372036854775808", 19) > 0) return false;
  }
  return true;
}

/**
 * @brief Convert string to 64-bit number.
 * Supports decimal, hexadecimal (`0x` prefix) and binary (`0b` prefix).
 * Negative values are allowed only in decimal form and are returned
 * as two's complement inside `uint64_t`. Cast to `int64_t` if signed
 * interpretation is required.
 * @note Use validator functions (`str_is_i64`, `str_is_u64`, etc.)
 * before calling to ensure format and range are valid.
 * @param str Input string.
 * @return Number as `uint64_t` (cast as `int64_t` if needed).
 */
uint64_t str_to_int64(const char *str) 
{
  int base = 10;
  bool is_negative = false;
  if(*str == '-') {
    is_negative = true;
    str++;
  }
  if(str[0] == '0' && str[1] == 'b') {
    base = 2;
    str += 2;
  } 
  else if(str[0] == '0' && str[1] == 'x') {
    base = 16;
    str += 2;
  }
  char *endptr;
  uint64_t result = strtoull(str, &endptr, base); // use strtoull for uint64_t
  return is_negative ? -(int64_t)result : (int64_t)result;
}

/**
 * @brief Check if `str` can be parsed as `float` (f32).
 * Accepts:
 * - optional sign (`+` or `-`)
 * - decimal numbers with optional decimal point
 * - scientific notation (`e`, `E` with optional sign and digits)
 * - special values (`NaN`, `Inf`, `-Inf`, case-insensitive)
 * @param str Input string.
 * @return `true` if valid float format, `false` otherwise.
 */
bool str_is_f32(const char *str)
{
  if(!str || *str == '\0') return false;
  // handle special values (case-insensitive)
  if(strcasecmp(str, "nan") == 0) return true;
  if(strcasecmp(str, "inf") == 0) return true;
  if(strcasecmp(str, "+inf") == 0) return true;
  if(strcasecmp(str, "-inf") == 0) return true;
  // optional sign
  if(*str == '-' || *str == '+') str++;
  if(*str == '\0') return false;
  bool dot = false;
  bool digit = false;
  // integer / fractional part
  while(*str && *str != 'e' && *str != 'E') {
    if(*str >= '0' && *str <= '9') digit = true;
    else if(*str == '.') {
      if(dot) return false;
      dot = true;
    }
    else return false;
    str++;
  }
  // scientific notation
  if(*str == 'e' || *str == 'E') {
    str++;
    if(*str == '-' || *str == '+') str++;
    if(!(*str >= '0' && *str <= '9')) return false; // at least one digit required
    while(*str) {
      if(*str < '0' || *str > '9') return false;
      str++;
    }
  }
  return digit;
}

/**
 * @brief Convert string to `float` (f32).
 * Supports:
 * - decimal numbers
 * - optional decimal point
 * - scientific notation (`e`, `E`)
 * - special values (`NaN`, `Inf`, `-Inf`, case-insensitive)
 * @note Always use `str_is_f32` before calling to ensure format is valid.
 * @param str Input string.
 * @return Number as `float`.
 */
float str_to_f32(const char *str)
{
  char *endptr;
  float result = strtof(str, &endptr);
  return result;
}

/**
 * @brief Check if string can be parsed as `float` with optional unit suffix.
 * @param str Input string.
 * @return `true` if valid, `false` otherwise.
 */
bool str_is_uf32(const char *str)
{
  if(!str || *str == '\0') return true;
  if(strcasecmp(str, "nan") == 0) return true;
  if(strcasecmp(str, "inf") == 0) return true;
  if(strcasecmp(str, "+inf") == 0) return true;
  if(strcasecmp(str, "-inf") == 0) return true;
  const char *end = str + strlen(str);
  const char *unit = end;
  while(unit > str && 
    !((unit[-1] >= '0' && unit[-1] <= '9') || unit[-1] == '.' ||
    unit[-1] == 'e' || unit[-1] == 'E' ||
    unit[-1] == '+' || unit[-1] == '-')) {
    unit--;
  }
  char nbr[unit - str + 1];
  strncpy(nbr, str, unit - str);
  nbr[unit - str] = '\0';
  return str_is_f32(nbr);
}

/**
 * @brief Convert string to `float` with unit suffix support.
 * Supported units:
 * - 't': tera
 * - 'g': giga
 * - 'meg','e' : mega
 * - 'k': kilo
 * - '%': percent
 * - 'm': milli
 * - 'u': micro
 * - 'n': nano
 * - 'p': pico
 * If the unit is not recognized, the value remains unchanged.
 * Requires prior validation with `str_is_f32_unit`.
 * @param str Input string.
 * @return Value as `float`. Returns 0.0 if invalid.
 */
float str_to_uf32(const char *str)
{
  if(!str || *str == '\0') return 0.0f;
  if(strcasecmp(str, "nan") == 0) return NaN;
  if(strcasecmp(str, "inf") == 0) return Inf;
  if(strcasecmp(str, "+inf") == 0) return Inf;
  if(strcasecmp(str, "-inf") == 0) return -Inf;
  const char *end = str + strlen(str);
  const char *unit = end;
  while(unit > str && 
    !((unit[-1] >= '0' && unit[-1] <= '9') || unit[-1] == '.' ||
    unit[-1] == 'e' || unit[-1] == 'E' ||
    unit[-1] == '+' || unit[-1] == '-')
  ) {
    unit--;
  }
  char nbr[unit - str + 1];
  strncpy(nbr, str, unit - str);
  nbr[unit - str] = '\0';
  float value = str_to_f32(nbr);
  switch(*unit) {
    case 't': value *= 1e12f; break;
    case 'g': value *= 1e9f; break;
    case 'e': value *= 1e6f; break;
    case 'k': value *= 1e3f; break;
    case '%': value /= 100.0f; break;
    case 'm':
      if(unit[1] == 'e' && unit[2] == 'g') value *= 1e6f;
      else value /= 1e3f;
      break;
    case 'u': value /= 1e6f; break;
    case 'n': value /= 1e9f; break;
    case 'p': value /= 1e12f; break;
    default: break;
  }
  return value;
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Copy string into new allocated buffer.
 * @param str Input string (null-terminated).
 * @return New allocated string or `NULL` if fail.
 */
char *str_copy(const char *str)
{
  if(!str) return NULL;
  size_t size = strlen(str) + 1;
  char *cp = (char*)heap_new(size);
  if(!cp) return NULL;
  for(size_t i = 0; i < size; i++) {
    cp[i] = str[i];
  }
  return cp;
}

/**
 * @brief Join multiple strings into new one.
 * @param count Number of strings.
 * @param str First string.
 * @param ... Following strings.
 * @return New allocated string or `NULL` if fail.
 *         Must be freed after use.
 */
char *str_join(uint32_t count, const char *str, ...)
{
  if(!str || count == 0) return NULL;
  va_list args;
  size_t total_size = 1;
  va_start(args, str);
  const char *tmp = str;
  for(uint32_t i = 0; i < count; i++) {
    total_size += strlen(tmp);
    tmp = va_arg(args, const char*);
  }
  va_end(args);
  char *out = (char*)heap_new(total_size);
  if(!out) return NULL;
  char *dst = out;
  va_start(args, str);
  tmp = str;
  for(uint32_t i = 0; i < count; i++) {
    const char *src = tmp;
    while(*src) *dst++ = *src++;
    tmp = va_arg(args, const char*);
  }
  va_end(args);
  *dst = '\0';
  return out;
}

/**
 * @brief Cut substring inside same string.
 * Index can be negative (counting from end).
 * Count <= 0 means substring goes to end.
 * @param str Input string (null-terminated), will be modified.
 * @param start Start index (can be negative).
 * @param len Number of characters to keep, <= 0 means until end.
 * @return Pointer to modified string or `NULL` if invalid.
 */
char *str_sub_this(char *str, int32_t start, int32_t len)
{
  int32_t size = (int32_t)strlen(str);
  if(start < 0) start += size;
  if(len <= 0) len = size - start;
  if(start < 0 || start >= size || len <= 0 || start + len > size) {
    return NULL;
  }
  for(int32_t i = 0; i < len; i++) {
    str[i] = str[start + i];
  }
  str[len] = '\0';
  return str;
}

/**
 * @brief Extract substring from string.
 * Index can be negative (counting from end).
 * Count <= 0 means substring goes to end.
 * @param str Input string (null-terminated).
 * @param start Start index (can be negative).
 * @param len Number of characters to copy, <= 0 means until end.
 * @return New allocated substring or `NULL` if invalid.
 *         Must be freed after use.
 */
char *str_sub(const char *str, int32_t start, int32_t len)
{
  int32_t size = (int32_t)strlen(str);
  if(start < 0) start += size;
  if(len <= 0) len = size - start;
  if(start < 0 || start >= size || len <= 0 || start + len > size) {
    return NULL;
  }
  char *out = (char*)heap_new((size_t)len + 1);
  if(!out) return NULL;
  for(int32_t i = 0; i < len; i++) {
    out[i] = str[start + i];
  }
  out[len] = '\0';
  return out;
}

const char LowerCase[256] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0xAB, 0x8E, 0x86,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x98, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xA2, 0xA3, 0xA5, 0xA5, 0xA6, 0xA7, 0xA9, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBE, 0xBE, 0xBF,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0x88, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xA2, 0xE1, 0xE2, 0xE4, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

const char UpperCase[256] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x8F, 0x87, 0x9D, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x97, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xE0, 0xA3, 0xA4, 0xA4, 0xA6, 0xA7, 0xA8, 0xA8, 0xAA, 0x8D, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBD, 0xBF,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE3, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * @brief Convert all characters in `str` to lowercase (in place).
 * @param str Input string (null-terminated).
 * @return Same pointer to modified string, or `NULL` if input was `NULL`.
 */
char *str_lower_this(char *str)
{
  if(!str) return NULL;
  char *p = str;
  while(*p) {
    *p = LowerCase[(uint8_t)*p];
    p++;
  }
  return str;
}

/**
 * @brief Convert all characters in `str` to uppercase (in place).
 * @param str Input string (null-terminated).
 * @return Same pointer to modified string, or `NULL` if input was `NULL`.
 */
char *str_upper_this(char *str)
{
  if(!str) return NULL;
  char *p = str;
  while(*p) {
    *p = UpperCase[(uint8_t)*p];
    p++;
  }
  return str;
}

/**
 * @brief Create lowercase copy of `str`.
 * @param str Input string (null-terminated).
 * @return Newly allocated lowercase string, or `NULL` if input was `NULL`.
 * Free the result after use.
 */
char *str_lower(char *str)
{
  if(!str) return NULL;
  char *cp = str_copy(str);
  return str_lower_this(cp);
}

/**
 * @brief Create uppercase copy of `str`.
 * @param str Input string (null-terminated).
 * @return Newly allocated uppercase string, or `NULL` if input was `NULL`.
 * Free the result after use.
 */
char *str_upper(char *str)
{
  if(!str) return NULL;
  char *cp = str_copy(str);
  return str_upper_this(cp);
}

/**
 * @brief Reverse string in place.
 * @param str Input string (null-terminated), will be modified.
 * @return Pointer to reversed string or NULL if invalid.
 */
char *str_reverse_this(char *str) 
{
  if(!str) return NULL;
  size_t size = strlen(str);
  if(size <= 1) return str;
  char *begin = str;
  char *end = str + size - 1;
  while(begin < end) {
    char tmp = *begin;
    *begin++ = *end;
    *end-- = tmp;
  }
  return str;
}

/**
 * @brief Create new reversed copy of string.
 * @param str Input string (null-terminated).
 * @return New allocated reversed string or NULL if fail.
 *         Must be freed after use.
 */
char *str_reverse(const char *str)
{
  if(!str) return NULL;
  size_t size = strlen(str);
  char *str_out = (char*)heap_new(size + 1);
  if(!str_out) return NULL;
  for(size_t i = 0; i < size; i++) {
    str_out[i] = str[size - 1 - i];
  }
  str_out[size] = '\0';
  return str_out;
}

/**
 * @brief Check if char is in set.
 * @param c Char to check.
 * @param chars Set of chars.
 * @return true if found, false if not.
 */
static bool str_has_char(char c, const char *chars)
{
  while(*chars) {
    if(c == *chars) return true;
    chars++;
  }
  return false;
}

/**
 * @brief Trim chars from start.
 * @param str Input string (modified).
 * @param chars Set of chars to trim.
 * @return Pointer to trimmed string.
 */
char *str_ltrim_chars(char *str, const char *chars)
{
  while(str_has_char(*str, chars)) str++;
  return str;
}

/**
 * @brief Trim whitespace from start.
 * @param str Input string (modified).
 * @return Pointer to trimmed string.
 */
char *str_ltrim(char *str)
{
  while(isspace((unsigned char)*str)) str++;
  return str;
}

/**
 * @brief Trim chars from end.
 * @param str Input string (modified).
 * @param chars Set of chars to trim.
 * @return Pointer to trimmed string.
 */
char *str_rtrim_chars(char *str, const char *chars)
{
  char *end = str + strlen(str);
  while(end > str && str_has_char(*--end, chars));
  *(end + 1) = '\0';
  return str;
}

/**
 * @brief Trim whitespace from end.
 * @param str Input string (modified).
 * @return Pointer to trimmed string.
 */
char *str_rtrim(char *str)
{
  char *end = str + strlen(str);
  while(end > str && isspace((unsigned char)*--end));
  *(end + 1) = '\0';
  return str;
}

/**
 * @brief Trim chars from both sides.
 * @param str Input string (modified).
 * @param chars Set of chars to trim.
 * @return Pointer to trimmed string.
 */
char *str_trim_chars(char *str, const char *chars)
{
  return str_ltrim_chars(str_rtrim_chars(str, chars), chars);
}

/**
 * @brief Trim whitespace from both sides.
 * @param str Input string (modified).
 * @return Pointer to trimmed string.
 */
char *str_trim(char *str)
{
  return str_ltrim(str_rtrim(str));
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Find first occurrence of pattern in string.
 * @param str Input string.
 * @param pattern Pattern to find.
 * @return Index (0-based) or -1 if not found.
 */
int str_find(const char *str, const char *pattern) 
{
  if(!str || !pattern || !*pattern) return -1;
  int idx = 0;
  size_t plen = strlen(pattern);
  while(str[idx]) {
    if(strncmp(&str[idx], pattern, plen) == 0) {
      return idx;
    }
    idx++;
  }
  return -1;
}

/**
 * @brief Find last occurrence of pattern in string.
 * @param str Input string.
 * @param pattern Pattern to find.
 * @return Index (0-based) or -1 if not found.
 */
int str_find_right(const char *str, const char *pattern)
{
  if(!str || !pattern || !*pattern) return -1;
  int last = -1;
  int idx = 0;
  size_t plen = strlen(pattern);
  while(str[idx]) {
    if(strncmp(&str[idx], pattern, plen) == 0) {
      last = idx;
    }
    idx++;
  }
  return last;
}

/**
 * @brief Cut string before pattern (in place).
 * @param str Input string (modified).
 * @param pattern Pattern to search.
 * @param right If true, use last occurrence, else first.
 * @return Pointer to modified string.
 */
char *str_cut_before_this(char *str, const char *pattern, bool right)
{
  if(!str || !pattern || !*pattern) return NULL;
  int idx = right ? str_find_right(str, pattern) : str_find(str, pattern);
  if(idx >= 0) str[idx] = '\0';
  return str;
}

/**
 * @brief Cut string after pattern (in place).
 * @param str Input string (modified).
 * @param pattern Pattern to search.
 * @param right If true, use last occurrence, else first.
 * @return Pointer to modified string.
 */
char *str_cut_after_this(char *str, const char *pattern, bool right)
{
  if(!str || !pattern || !*pattern) return NULL;
  int idx = right ? str_find_right(str, pattern) : str_find(str, pattern);
  if(idx >= 0) {
    size_t plen = strlen(pattern);
    memmove(str, str + idx + plen, strlen(str + idx + plen) + 1);
  }
  return str;
}

/**
 * @brief Create new string cut before pattern.
 * @param str Input string.
 * @param pattern Pattern to search.
 * @param right If true, use last occurrence, else first.
 * @return New allocated string or NULL if fail.
 */
char *str_cut_before(const char *str, const char *pattern, bool right)
{
  if(!str || !pattern) return NULL;
  size_t size = strlen(str) + 1;
  char *out = (char*)heap_new(size);
  if(!out) return NULL;
  strcpy(out, str);
  return str_cut_before_this(out, pattern, right);
}

/**
 * @brief Create new string cut after pattern.
 * @param str Input string.
 * @param pattern Pattern to search.
 * @param right If true, use last occurrence, else first.
 * @return New allocated string or NULL if fail.
 */
char *str_cut_after(const char *str, const char *pattern, bool right)
{
  if(!str || !pattern) return NULL;
  size_t size = strlen(str) + 1;
  char *out = (char*)heap_new(size);
  if(!out) return NULL;
  strcpy(out, str);
  return str_cut_after_this(out, pattern, right);
}

//-------------------------------------------------------------------------------------------------

/**
 * Replace all chars listed in pattern with replacement (in place).
 * @param str Input string (null-terminated, modified).
 * @param pattern List of chars to replace.
 * @param replacement Char used as replacement.
 * @return Pointer to modified string.
 */
char *str_replace_chars_this(char *str, const char *pattern, char replacement)
{
  if(!str || !pattern) return str;
  char *p = str;
  while(*p) {
    const char *pat = pattern;
    while(*pat) {
      if(*p == *pat) {
        *p = replacement;
        break;
      }
      pat++;
    }
    p++;
  }
  return str;
}

 /**
 * Create copy of string and replace all chars listed in pattern.
 * @param str Input string (null-terminated).
 * @param pattern List of chars to replace.
 * @param replacement Char used as replacement.
 * @return New allocated string with replacements, or NULL on error.
 */
char *str_replace_chars(const char *str, const char *pattern, char replacement)
{
  if(!str || !pattern) return NULL;
  size_t size = strlen(str) + 1;
  char *out = (char*)heap_new(size);
  if(!out) return NULL;
  memcpy(out, str, size);
  return str_replace_chars_this(out, pattern, replacement);
}

/**
 * Create copy of string and replace all occurrences of pattern.
 * @param str Input string (null-terminated, not modified).
 * @param pattern Substring to replace.
 * @param replacement Replacement substring.
 * @return New allocated string with replacements, or NULL on error.
 */
char *str_replace(const char *str, const char *pattern, const char *replacement)
{
  if(!str || !pattern || !replacement || *pattern == '\0') return NULL;
  size_t plen = strlen(pattern);
  size_t rlen = strlen(replacement);
  size_t slen = strlen(str);
  // count occurrences
  size_t count = 0;
  const char *scan = str;
  const char *pos;
  while((pos = strstr(scan, pattern)) != NULL) {
    count++;
    scan = pos + plen;
  }
  // allocate result
  size_t outlen = slen + count * (rlen - plen);
  char *out = (char*)heap_new(outlen + 1);
  if(!out) return NULL;
  // build result
  char *dst = out;
  scan = str;
  while((pos = strstr(scan, pattern)) != NULL) {
    size_t chunk = (size_t)(pos - scan);
    memcpy(dst, scan, chunk);
    dst += chunk;
    memcpy(dst, replacement, rlen);
    dst += rlen;
    scan = pos + plen;
  }
  strcpy(dst, scan);
  return out;
}

/**
 * Split string by delimiter and get part at given index.
 * Example: str_split("a,b,c", ',', 1) → "b"
 * @param str Input string (null-terminated).
 * @param delimiter Single delimiter char.
 * @param index Zero-based index of part to return.
 * @return New allocated substring, or NULL if not found.
 */
char *str_split(const char *str, char delimiter, int index)
{
  if(!str || index < 0) return NULL;
  const char *start = str;
  while(*str) {
    if(*str == delimiter) {
      if(index == 0) break;
      index--;
      start = str + 1;
    }
    str++;
  }
  if(index > 0) return NULL;
  size_t len = (size_t)(str - start);
  char *out = (char*)heap_new(len + 1);
  if(!out) return NULL;
  memcpy(out, start, len);
  out[len] = '\0';
  return out;
}

/**
 * Split string into parts by delimiter.
 * All parts and pointer array are stored in one heap allocation.
 * Must be freed with heap_free().
 * @param arr_ptr Output pointer to array of substrings.
 * @param str Input string (null-terminated, not NULL).
 * @param delimiter Single delimiter char.
 * @return Number of parts, or -1 on error.
 */
int str_explode(char ***arr_ptr, const char *str, char delimiter)
{
  if(!arr_ptr || !str) return -1;
  int count = 1;
  const char *scan = str;
  while((scan = strchr(scan, delimiter)) != NULL) {
    count++;
    scan++;
  }
  size_t str_len = strlen(str);
  char **arr = (char**)heap_new(count * sizeof(char*) + (str_len + 1) * sizeof(char));
  if(!arr) return -1;
  char *dst = (char*)arr + count * sizeof(char*);
  const char *src = str;
  for(int i = 0; i < count; i++) {
    const char *end = strchr(src, delimiter);
    if(!end) end = src + strlen(src);
    size_t len = (size_t)(end - src);
    arr[i] = dst;
    memcpy(dst, src, len);
    dst[len] = '\0';
    dst += len + 1;
    src = (*end) ? end + 1 : end;
  }
  *arr_ptr = arr;
  return count;
}

//-------------------------------------------------------------------------------------------------

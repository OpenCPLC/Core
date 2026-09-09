// lib/ext/xstring.c

#include "xstring.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------------------------------- Globals

char StrTempMem[256];

const char LowerCase[256] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
  0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x5B,0x5C,0x5D,0x5E,0x5F,
  0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
  0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0xAB,0x8E,0x86,
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x98,0x98,0x99,0x9A,0x9B,0x9C,0x88,0x9E,0x9F,
  0xA0,0xA1,0xA2,0xA3,0xA5,0xA5,0xA6,0xA7,0xA9,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBE,0xBE,0xBF,
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
  0xA2,0xE1,0xE2,0xE4,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

const char UpperCase[256] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
  0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
  0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
  0x60,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
  0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x7B,0x7C,0x7D,0x7E,0x7F,
  0x80,0x81,0x82,0x83,0x84,0x85,0x8F,0x87,0x9D,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x97,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
  0xA0,0xA1,0xE0,0xA3,0xA4,0xA4,0xA6,0xA7,0xA8,0xA8,0xAA,0x8D,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBD,0xBF,
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
  0xE0,0xE1,0xE2,0xE3,0xE3,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

//-------------------------------------------------------------------------------------------- Hash

uint32_t hash_djb2(const char *str)
{
  uint32_t hash = 5381;
  uint32_t c;
  while((c = (uint8_t)*str++)) hash = ((hash << 5) + hash) + c; // hash * 33 + c
  return hash;
}

uint32_t hash_djb2_ci(const char *str)
{
  uint32_t hash = 5381;
  uint32_t c;
  while(*str) {
    c = (uint8_t)LowerCase[(uint8_t)*str++];
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

//----------------------------------------------------------------------------------------- Numbers

static bool is_hex_digit(char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// Decimal digits of `str`, or `0` when anything else is present
static size_t dec_digits(const char *str)
{
  size_t count = 0;
  while(*str) {
    if(*str < '0' || *str > '9') return 0;
    count++;
    str++;
  }
  return count;
}

// Unsigned literal that fits `bits`: the digit count bounds a binary or hex value,
// a decimal one of full length is compared against `dec_max` as text
static bool str_is_uint(const char *str, uint8_t bits, const char *dec_max)
{
  if(!str || !*str) return false;
  if(str[0] == '0' && str[1] == 'b' && str[2]) {
    size_t count = 0;
    for(str += 2; *str; str++, count++) {
      if(*str != '0' && *str != '1') return false;
    }
    return count <= bits;
  }
  if(str[0] == '0' && str[1] == 'x' && str[2]) {
    size_t count = 0;
    for(str += 2; *str; str++, count++) {
      if(!is_hex_digit(*str)) return false;
    }
    return count <= bits / 4;
  }
  size_t count = dec_digits(str);
  size_t limit = strlen(dec_max);
  if(!count || count > limit) return false;
  return count < limit || strncmp(str, dec_max, limit) <= 0;
}

// Signed decimal literal, the magnitude bounds differ by one across the sign
static bool str_is_int(const char *str, const char *pos_max, const char *neg_max)
{
  if(!str || !*str) return false;
  const char *max = pos_max;
  if(*str == '-') {
    max = neg_max;
    str++;
  }
  size_t count = dec_digits(str);
  size_t limit = strlen(max);
  if(!count || count > limit) return false;
  return count < limit || strncmp(str, max, limit) <= 0;
}

bool str_is_u16(const char *str) { return str_is_uint(str, 16, "65535"); }
bool str_is_u32(const char *str) { return str_is_uint(str, 32, "4294967295"); }
bool str_is_u64(const char *str) { return str_is_uint(str, 64, "18446744073709551615"); }
bool str_is_i16(const char *str) { return str_is_int(str, "32767", "32768"); }
bool str_is_i32(const char *str) { return str_is_int(str, "2147483647", "2147483648"); }

bool str_is_i64(const char *str)
{
  return str_is_int(str, "9223372036854775807", "9223372036854775808");
}

uint64_t str_to_int64(const char *str)
{
  int base = 10;
  bool negative = false;
  if(*str == '-') {
    negative = true;
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
  uint64_t value = strtoull(str, NULL, base);
  return negative ? (uint64_t)(-(int64_t)value) : value;
}

uint32_t str_to_int(const char *str)
{
  return (uint32_t)str_to_int64(str);
}

uint8_t str_split_int(const char *str, char sep, uint8_t base, uint32_t *parts,
  uint8_t limit)
{
  if(!str || !*str || !limit || base < 2 || base > 36) return 0;
  uint8_t count = 0;
  bool digit = false;
  uint64_t value = 0;
  for(const char *c = str; ; c++) {
    uint8_t weight = 0xFF;
    if(*c >= '0' && *c <= '9') weight = (uint8_t)(*c - '0');
    else if(*c >= 'A' && *c <= 'Z') weight = (uint8_t)(*c - 'A' + 10);
    else if(*c >= 'a' && *c <= 'z') weight = (uint8_t)(*c - 'a' + 10);
    if(weight < base) {
      value = value * base + weight;
      if(value > UINT32_MAX) return 0;
      digit = true;
    }
    else if(*c == sep || *c == 0) {
      if(!digit || count >= limit) return 0;
      parts[count++] = (uint32_t)value;
      value = 0;
      digit = false;
      if(*c == 0) return count;
    }
    else return 0;
  }
}

// `nan` and `inf` in any case, with an optional sign on the infinity
static bool str_is_special(const char *str)
{
  if(strcasecmp(str, "nan") == 0) return true;
  if(*str == '+' || *str == '-') str++;
  return strcasecmp(str, "inf") == 0;
}

bool str_is_f32(const char *str)
{
  if(!str || !*str) return false;
  if(str_is_special(str)) return true;
  if(*str == '-' || *str == '+') str++;
  if(!*str) return false;
  bool dot = false;
  bool digit = false;
  while(*str && *str != 'e' && *str != 'E') {
    if(*str >= '0' && *str <= '9') digit = true;
    else if(*str == '.' && !dot) dot = true;
    else return false;
    str++;
  }
  if(*str == 'e' || *str == 'E') {
    str++;
    if(*str == '-' || *str == '+') str++;
    if(!(*str >= '0' && *str <= '9')) return false; // exponent needs a digit
    while(*str) {
      if(*str < '0' || *str > '9') return false;
      str++;
    }
  }
  return digit;
}

float str_to_f32(const char *str)
{
  return strtof(str, NULL);
}

// Start of the unit suffix. `e` opens an exponent only when digits follow it,
// otherwise it belongs to a suffix such as `meg`
static const char *str_unit(const char *str)
{
  const char *p = str;
  if(*p == '+' || *p == '-') p++;
  while(*p >= '0' && *p <= '9') p++;
  if(*p == '.') { p++; while(*p >= '0' && *p <= '9') p++; }
  if(*p == 'e' || *p == 'E') {
    const char *exp = (p[1] == '+' || p[1] == '-') ? p + 2 : p + 1;
    if(*exp >= '0' && *exp <= '9') { p = exp; while(*p >= '0' && *p <= '9') p++; }
  }
  return p;
}

bool str_is_uf32(const char *str)
{
  if(!str || !*str) return false;
  if(str_is_special(str)) return true;
  const char *unit = str_unit(str);
  char nbr[unit - str + 1];
  memcpy(nbr, str, unit - str);
  nbr[unit - str] = '\0';
  return str_is_f32(nbr);
}

float str_to_uf32(const char *str)
{
  if(!str || !*str) return 0.0f;
  if(str_is_special(str)) return str_to_f32(str);
  const char *unit = str_unit(str);
  char nbr[unit - str + 1];
  memcpy(nbr, str, unit - str);
  nbr[unit - str] = '\0';
  float value = str_to_f32(nbr);
  switch(*unit) {
    case 't': value *= 1e12f; break;
    case 'g': value *= 1e9f; break;
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

uint8_t itoa_encode(int64_t nbr, char *str, uint8_t base, bool sign,
  uint8_t fill_zero, uint8_t fill_space)
{
  static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if(base < 2 || base > 36) return 0;
  if(fill_zero == 0) fill_zero = 1;
  if(fill_space < fill_zero) fill_space = fill_zero;
  bool negative = sign && nbr < 0;
  uint64_t unbr = negative ? (uint64_t)(-(uint64_t)nbr) : (uint64_t)nbr;
  uint8_t n = 0;
  do {
    str[n++] = digits[unbr % base];
    unbr /= base;
  } while(unbr > 0);
  // The sign takes one place of the zero padding, none of the space padding
  while(n < fill_zero - (negative ? 1 : 0)) str[n++] = '0';
  if(negative) str[n++] = '-';
  while(n < fill_space) str[n++] = ' ';
  return n;
}

char *str_from_int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space)
{
  uint8_t len = itoa_encode(nbr, StrTempMem, base, sign, fill_zero, fill_space);
  char *string = heap_new(len + 1);
  if(!string) return NULL;
  for(uint8_t i = 0; i < len; i++) string[i] = StrTempMem[len - 1 - i];
  string[len] = '\0';
  return string;
}

char *str_append_int(char *dst, int64_t nbr, uint8_t base, uint8_t fill_zero)
{
  uint8_t len = itoa_encode(nbr, dst, base, true, fill_zero, 0);
  for(uint8_t i = 0, j = len; i + 1 < j; i++) {
    j--;
    swap(dst[i], dst[j]);
  }
  return dst + len;
}

//------------------------------------------------------------------------------------------ Memory

char *str_copy(const char *str)
{
  if(!str) return NULL;
  size_t size = strlen(str) + 1;
  char *cp = heap_new(size);
  if(!cp) return NULL;
  memcpy(cp, str, size);
  return cp;
}

char *str_join(uint32_t count, const char *str, ...)
{
  if(!str || count == 0) return NULL;
  va_list args;
  size_t total_size = 1;
  va_start(args, str);
  const char *tmp = str;
  for(uint32_t i = 0; i < count; i++) {
    total_size += strlen(tmp);
    if(i + 1 < count) tmp = va_arg(args, const char *);
  }
  va_end(args);
  char *out = heap_new(total_size);
  if(!out) return NULL;
  char *dst = out;
  va_start(args, str);
  tmp = str;
  for(uint32_t i = 0; i < count; i++) {
    size_t len = strlen(tmp);
    memcpy(dst, tmp, len);
    dst += len;
    if(i + 1 < count) tmp = va_arg(args, const char *);
  }
  va_end(args);
  *dst = '\0';
  return out;
}

// Resolve `start` and `len` against the string, `false` when the range leaves it
static bool sub_range(const char *str, int32_t *start, int32_t *len)
{
  int32_t size = (int32_t)strlen(str);
  if(*start < 0) *start += size;
  if(*len <= 0) *len = size - *start;
  return *start >= 0 && *start < size && *len > 0 && *start + *len <= size;
}

char *str_sub_this(char *str, int32_t start, int32_t len)
{
  if(!sub_range(str, &start, &len)) return NULL;
  memmove(str, str + start, len);
  str[len] = '\0';
  return str;
}

char *str_sub(const char *str, int32_t start, int32_t len)
{
  if(!sub_range(str, &start, &len)) return NULL;
  char *out = heap_new((size_t)len + 1);
  if(!out) return NULL;
  memcpy(out, str + start, len);
  out[len] = '\0';
  return out;
}

//-------------------------------------------------------------------------------------------- Case

char *str_lower_this(char *str)
{
  if(!str) return NULL;
  for(char *p = str; *p; p++) *p = LowerCase[(uint8_t)*p];
  return str;
}

char *str_upper_this(char *str)
{
  if(!str) return NULL;
  for(char *p = str; *p; p++) *p = UpperCase[(uint8_t)*p];
  return str;
}

char *str_lower(const char *str) { return str_lower_this(str_copy(str)); }
char *str_upper(const char *str) { return str_upper_this(str_copy(str)); }

//------------------------------------------------------------------------------------------- Shape

char *str_reverse_this(char *str)
{
  if(!str) return NULL;
  size_t size = strlen(str);
  for(size_t i = 0, j = size; i + 1 < j; i++) {
    j--;
    swap(str[i], str[j]);
  }
  return str;
}

char *str_reverse(const char *str)
{
  return str_reverse_this(str_copy(str));
}

// `true` when `c` is listed in `chars`, the terminator itself never is
static bool str_has_char(char c, const char *chars)
{
  while(*chars) {
    if(c == *chars) return true;
    chars++;
  }
  return false;
}

char *str_ltrim_chars(char *str, const char *chars)
{
  while(str_has_char(*str, chars)) str++;
  return str;
}

char *str_ltrim(char *str)
{
  while(isspace((unsigned char)*str)) str++;
  return str;
}

char *str_rtrim_chars(char *str, const char *chars)
{
  char *end = str + strlen(str);
  while(end > str && str_has_char(end[-1], chars)) end--;
  *end = '\0';
  return str;
}

char *str_rtrim(char *str)
{
  char *end = str + strlen(str);
  while(end > str && isspace((unsigned char)end[-1])) end--;
  *end = '\0';
  return str;
}

char *str_trim_chars(char *str, const char *chars)
{
  return str_ltrim_chars(str_rtrim_chars(str, chars), chars);
}

char *str_trim(char *str)
{
  return str_ltrim(str_rtrim(str));
}

//------------------------------------------------------------------------------------------ Search

int str_find(const char *str, const char *pattern)
{
  if(!str || !pattern || !*pattern) return -1;
  const char *pos = strstr(str, pattern);
  return pos ? (int)(pos - str) : -1;
}

int str_find_right(const char *str, const char *pattern)
{
  if(!str || !pattern || !*pattern) return -1;
  int last = -1;
  size_t plen = strlen(pattern);
  for(const char *pos = strstr(str, pattern); pos; pos = strstr(pos + plen, pattern)) {
    last = (int)(pos - str);
  }
  return last;
}

char *str_cut_before_this(char *str, const char *pattern, bool right)
{
  if(!str || !pattern || !*pattern) return NULL;
  int idx = right ? str_find_right(str, pattern) : str_find(str, pattern);
  if(idx >= 0) str[idx] = '\0';
  return str;
}

char *str_cut_after_this(char *str, const char *pattern, bool right)
{
  if(!str || !pattern || !*pattern) return NULL;
  int idx = right ? str_find_right(str, pattern) : str_find(str, pattern);
  if(idx >= 0) {
    char *rest = str + idx + strlen(pattern);
    memmove(str, rest, strlen(rest) + 1);
  }
  return str;
}

char *str_cut_before(const char *str, const char *pattern, bool right)
{
  return str_cut_before_this(str_copy(str), pattern, right);
}

char *str_cut_after(const char *str, const char *pattern, bool right)
{
  return str_cut_after_this(str_copy(str), pattern, right);
}

char *str_replace_chars_this(char *str, const char *pattern, char replacement)
{
  if(!str || !pattern) return str;
  for(char *p = str; *p; p++) {
    if(str_has_char(*p, pattern)) *p = replacement;
  }
  return str;
}

char *str_replace_chars(const char *str, const char *pattern, char replacement)
{
  if(!pattern) return NULL;
  return str_replace_chars_this(str_copy(str), pattern, replacement);
}

char *str_replace(const char *str, const char *pattern, const char *replacement)
{
  if(!str || !pattern || !replacement || !*pattern) return NULL;
  size_t plen = strlen(pattern);
  size_t rlen = strlen(replacement);
  size_t count = 0;
  for(const char *pos = strstr(str, pattern); pos; pos = strstr(pos + plen, pattern)) count++;
  char *out = heap_new(strlen(str) + count * (rlen - plen) + 1);
  if(!out) return NULL;
  char *dst = out;
  const char *scan = str;
  for(const char *pos = strstr(scan, pattern); pos; pos = strstr(scan, pattern)) {
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
  char *out = heap_new(len + 1);
  if(!out) return NULL;
  memcpy(out, start, len);
  out[len] = '\0';
  return out;
}

int str_explode(char ***arr_ptr, const char *str, char delimiter)
{
  if(!arr_ptr || !str) return -1;
  int count = 1;
  for(const char *scan = strchr(str, delimiter); scan; scan = strchr(scan + 1, delimiter)) {
    count++;
  }
  // Pointer array first, the parts follow it in the same block
  char **arr = heap_new(count * sizeof(char *) + strlen(str) + 1);
  if(!arr) return -1;
  char *dst = (char *)(arr + count);
  const char *src = str;
  for(int i = 0; i < count; i++) {
    const char *end = strchr(src, delimiter);
    if(!end) end = src + strlen(src);
    size_t len = (size_t)(end - src);
    arr[i] = dst;
    memcpy(dst, src, len);
    dst[len] = '\0';
    dst += len + 1;
    src = *end ? end + 1 : end;
  }
  *arr_ptr = arr;
  return count;
}

//-------------------------------------------------------------------------------------------------

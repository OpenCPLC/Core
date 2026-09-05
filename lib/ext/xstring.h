// lib/ext/xstring.h

#ifndef XSTRING_H_
#define XSTRING_H_

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include "xdef.h"
#include "heap.h"

//-------------------------------------------------------------------------------------------------

extern char StrTempMem[];
extern const char LowerCase[];
extern const char UpperCase[];

//-------------------------------------------------------------------------------------------------

uint32_t hash_djb2(const char *str);
uint32_t hash_djb2_ci(const char *str);

// Digits land least significant first: `123` fills the buffer as `321`, sign and
// padding behind them. `str_from_int` and `str_append_int` reverse the span.
uint8_t itoa_encode(int64_t nbr, char *str, uint8_t base, bool sign,
  uint8_t fill_zero, uint8_t fill_space);
char *str_from_int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero,
  uint8_t fill_space);
bool str_is_u16(const char *str);
bool str_is_i16(const char *str);
bool str_is_u32(const char *str);
bool str_is_i32(const char *str);
uint32_t str_to_int(const char *str);
bool str_is_u64(const char *str);
bool str_is_i64(const char *str);
uint64_t str_to_int64(const char *str) ;
bool str_is_f32(const char *str);
float str_to_f32(const char *str);
bool str_is_uf32(const char *str);
float str_to_uf32(const char *str);

/**
 * @brief Numbers from `str` split on `sep` (`"2.3.2"` or `"0000-0001"` style).
 *   Letters carry the digits above nine, so a field parses in whatever base it prints in.
 * @param str Input text.
 * @param sep Separator character.
 * @param base Numeric base, 2 to 36.
 * @param parts Output values.
 * @param limit Capacity of `parts`.
 * @return Number of parts; `0` when empty, malformed or past `limit`.
 */
uint8_t str_split_int(const char *str, char sep, uint8_t base, uint32_t *parts,
  uint8_t limit);

/**
 * @brief Write `nbr` at `dst`, forward, without a terminator.
 * @param dst Destination, room for the widest number in `base` or for `fill_zero`
 *   digits, whichever is longer.
 * @param nbr Number to write.
 * @param base Numeric base, 2 to 36.
 * @param fill_zero Least digits to write, padded with leading zeros.
 * @return Position right after the written digits, ready for the next append.
 */
char *str_append_int(char *dst, int64_t nbr, uint8_t base, uint8_t fill_zero);

char *str_copy(const char *str);
char *str_join(uint32_t count, const char *str, ...);
char *str_sub_this(char *str, int32_t start, int32_t len);
char *str_sub(const char *str, int32_t start, int32_t len);
char *str_lower_this(char *str);
char *str_upper_this(char *str);
char *str_lower(char *str);
char *str_upper(char *str);
char *str_reverse_this(char *str);
char *str_reverse(const char *str);
char *str_ltrim_chars(char *str, const char *chars);
char *str_ltrim(char *str);
char *str_rtrim_chars(char *str, const char *chars);
char *str_rtrim(char *str);
char *str_trim_chars(char *str, const char *chars);
char *str_trim(char *str);
int str_find(const char *str, const char *pattern);
int str_find_right(const char *str, const char *pattern);
char *str_cut_before_this(char *str, const char *pattern, bool right);
char *str_cut_after_this(char *str, const char *pattern, bool right);
char *str_cut_before(const char *str, const char *pattern, bool right);
char *str_cut_after(const char *str, const char *pattern, bool right);

char *str_replace_chars_this(char *str, const char *pattern, char replacement);
char *str_replace_chars(const char *str, const char *pattern, char replacement);
char *str_replace(const char *str, const char *pattern, const char *replacement);
char *str_split(const char *str, char delimiter, int index);
int str_explode(char ***arr_ptr, const char *str, char delimiter);

//-------------------------------------------------------------------------------------------------
#endif

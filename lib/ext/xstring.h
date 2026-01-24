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

uint8_t itoa_encode(int64_t nbr, char *str, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space);
char *str_from_int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero, uint8_t fill_space);
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

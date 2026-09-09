// lib/ext/xstring.h

#ifndef XSTRING_H_
#define XSTRING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "xdef.h"
#include "heap.h"

//----------------------------------------------------------------------------------------- Globals

// Scratch for `itoa_encode`: padding parameters are `uint8_t`, so one call writes at most
// 255 bytes. Not terminated, callers take the returned length
extern char StrTempMem[];

// Case mapping tables for the whole byte range, `CP852` above ASCII
extern const char LowerCase[];
extern const char UpperCase[];

//-------------------------------------------------------------------------------------------- Hash

// djb2 hash of a string, `hash_djb2_ci` maps through `LowerCase` first
uint32_t hash_djb2(const char *str);
uint32_t hash_djb2_ci(const char *str);

//----------------------------------------------------------------------------------------- Numbers

// `true` when `str` parses as the type: decimal, `0x` hex or `0b` binary for unsigned,
// decimal with an optional `-` for signed, range included
bool str_is_u16(const char *str);
bool str_is_i16(const char *str);
bool str_is_u32(const char *str);
bool str_is_i32(const char *str);
bool str_is_u64(const char *str);
bool str_is_i64(const char *str);

/**
 * @brief Parse an integer validated by one of the `str_is_...` checks.
 *   A negative decimal comes back as two's complement, cast to the signed type.
 * @param[in] str Decimal, `0x` hex or `0b` binary text
 * @return Value, cast to `int32_t`, `uint16_t` or `int16_t` as needed
 */
uint32_t str_to_int(const char *str);
uint64_t str_to_int64(const char *str);

/**
 * @brief Numbers from `str` split on `sep` (`"2.3.2"` or `"0000-0001"` style).
 *   Letters carry the digits above nine, so a field parses in whatever base it prints in.
 * @param[in] str Input text
 * @param[in] sep Separator character
 * @param[in] base Numeric base, `2` to `36`
 * @param[out] parts Output values
 * @param[in] limit Capacity of `parts`
 * @return Number of parts, `0` when empty, malformed or past `limit`
 */
uint8_t str_split_int(const char *str, char sep, uint8_t base, uint32_t *parts,
  uint8_t limit);

// `true` when `str` is a float: optional sign, decimal point, exponent, `nan`, `inf`
bool str_is_f32(const char *str);
float str_to_f32(const char *str);

// Float with a unit suffix: `t`, `g`, `meg`, `k`, `%`, `m`, `u`, `n`, `p`
bool str_is_uf32(const char *str);
float str_to_uf32(const char *str);

/**
 * @brief Encode an integer as reversed digits, least significant first.
 *   `str_from_int` and `str_append_int` reverse the span.
 * @param[in] nbr Number to encode
 * @param[out] str Output, `fill_space` bytes at most, not terminated
 * @param[in] base Numeric base, `2` to `36`
 * @param[in] sign `true` treats `nbr` as signed and writes `-` when negative
 * @param[in] fill_zero Least digits, padded with leading zeros
 * @param[in] fill_space Least width, padded with leading spaces
 * @return Bytes written
 */
uint8_t itoa_encode(int64_t nbr, char *str, uint8_t base, bool sign,
  uint8_t fill_zero, uint8_t fill_space);

/**
 * @brief Integer as a new heap string, parameters as in `itoa_encode`.
 * @return Terminated string owned by the garbage collector, `NULL` when the heap is full
 */
char *str_from_int(int64_t nbr, uint8_t base, bool sign, uint8_t fill_zero,
  uint8_t fill_space);

/**
 * @brief Write `nbr` at `dst`, forward, without a terminator.
 * @param[out] dst Destination, room for the widest number in `base` or for `fill_zero`
 *   digits, whichever is longer
 * @param[in] nbr Number to write
 * @param[in] base Numeric base, `2` to `36`
 * @param[in] fill_zero Least digits to write, padded with leading zeros
 * @return Position right after the written digits, ready for the next append
 */
char *str_append_int(char *dst, int64_t nbr, uint8_t base, uint8_t fill_zero);

//------------------------------------------------------------------------------------------ Memory

// New strings come from `heap_new`: owned by the garbage collector of the calling thread,
// released by `heap_clear`, never freed by the caller. `NULL` on a full heap.
// `_this` variants work in place and return their argument

char *str_copy(const char *str);

/**
 * @brief Join `count` strings into a new one.
 * @param[in] count Number of strings
 * @param[in] str First string, the rest follow as arguments
 * @return New string, `NULL` on a full heap or `count` of `0`
 */
char *str_join(uint32_t count, const char *str, ...);

/**
 * @brief Substring, in place or as a new string.
 * @param[in] str Source
 * @param[in] start Start index, negative counts from the end
 * @param[in] len Length, `0` or negative runs to the end
 * @return Substring, `NULL` when the range leaves the string
 */
char *str_sub_this(char *str, int32_t start, int32_t len);
char *str_sub(const char *str, int32_t start, int32_t len);

//-------------------------------------------------------------------------------------------- Case

char *str_lower_this(char *str);
char *str_upper_this(char *str);
char *str_lower(const char *str);
char *str_upper(const char *str);

//------------------------------------------------------------------------------------------- Shape

char *str_reverse_this(char *str);
char *str_reverse(const char *str);

// Trim in place: `_chars` variants drop the listed characters, the rest whitespace.
// Left trims return a pointer into the string, the start moves
char *str_ltrim_chars(char *str, const char *chars);
char *str_ltrim(char *str);
char *str_rtrim_chars(char *str, const char *chars);
char *str_rtrim(char *str);
char *str_trim_chars(char *str, const char *chars);
char *str_trim(char *str);

//------------------------------------------------------------------------------------------ Search

// Index of the first or the last occurrence of `pattern`, `-1` when absent
int str_find(const char *str, const char *pattern);
int str_find_right(const char *str, const char *pattern);

/**
 * @brief Cut the string at `pattern`: `before` keeps what precedes it, `after` what
 *   follows. A missing pattern leaves the string whole.
 * @param[in] str Source
 * @param[in] pattern Text to find
 * @param[in] right `true` = last occurrence, `false` = first
 * @return Cut string, `NULL` on a `NULL` or empty argument
 */
char *str_cut_before_this(char *str, const char *pattern, bool right);
char *str_cut_after_this(char *str, const char *pattern, bool right);
char *str_cut_before(const char *str, const char *pattern, bool right);
char *str_cut_after(const char *str, const char *pattern, bool right);

// Replace every character listed in `pattern` with `replacement`
char *str_replace_chars_this(char *str, const char *pattern, char replacement);
char *str_replace_chars(const char *str, const char *pattern, char replacement);

// New string with every occurrence of `pattern` replaced
char *str_replace(const char *str, const char *pattern, const char *replacement);

/**
 * @brief Part `index` of `str` split on `delimiter`, as a new string.
 * @param[in] str Source
 * @param[in] delimiter Separator character
 * @param[in] index Zero-based part number
 * @return New string, `NULL` when the part does not exist
 */
char *str_split(const char *str, char delimiter, int index);

/**
 * @brief Split `str` on `delimiter` into an array of new strings, one allocation.
 * @param[out] arr_ptr Receives the array of parts
 * @param[in] str Source
 * @param[in] delimiter Separator character
 * @return Number of parts, `-1` on a `NULL` argument or a full heap
 */
int str_explode(char ***arr_ptr, const char *str, char delimiter);

//-------------------------------------------------------------------------------------------------
#endif

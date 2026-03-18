/** @file lib/ext/xdef.h */

#define ON  1 // Enabled
#define OFF 0 // Disables

#ifndef XDEF_H_
#define XDEF_H_

#include <limits.h>
#include <math.h>

//-------------------------------------------------------------------------------------------------

typedef enum {
  OK   = 0, // Normal execution
  FREE = 0, // Resource free
  IDLE = 0, // Nothing active
  ERR  = 1, // Error occurred
  BUSY = 2, // Resource busy
} status_t;

#define NaN         NAN
#define Inf         INFINITY
#define isNaN(nbr)  isnan(nbr)
#define isInf(nbr)  isinf(nbr)

/**
 * @brief Set bit at position `bit` in register `reg`.
 * @param reg Register or variable to modify.
 * @param bit Bit index to set (0 = LSB).
 */
#define bit_set(reg, bit)  ((reg) |= ((__typeof__(reg))1 << (bit)))

/**
 * @brief Clear bit at position `bit` in register `reg`.
 * @param reg Register or variable to modify.
 * @param bit Bit index to clear (0 = LSB).
 */
#define bit_rst(reg, bit)  ((reg) &= ~((__typeof__(reg))1 << (bit)))

/**
 * @brief Toggle bit at position `bit` in register `reg`.
 * @param reg Register or variable to modify.
 * @param bit Bit index to toggle (0 = LSB).
 */
#define bit_tgl(reg, bit)  ((reg) ^= ((__typeof__(reg))1 << (bit)))

/**
 * @brief Read bit at position `bit` in register `reg`.
 * @param reg Register or value to read.
 * @param bit Bit index to check (0 = LSB).
 * @return `1` if bit is set, `0` if cleared.
 */
#define bit_get(reg, bit)  (((reg) >> (bit)) & 1U)

//------------------------------------------------------------------------------------------------- ANSI

#ifndef ANSI_MODE
  // Terminal color mode: `0`:disabled, `1`:basic 16-color, `2`:256-color
  #define ANSI_MODE 2  
#endif

#if(ANSI_MODE == 0)
  #define ANSI_RED      ""
  #define ANSI_GREEN    ""
  #define ANSI_YELLOW   ""
  #define ANSI_BLUE     ""
  #define ANSI_VIOLET   ""
  #define ANSI_MAGENTA  ""
  #define ANSI_PINK     ""
  #define ANSI_CYAN     ""
  #define ANSI_GREY     ""
  #define ANSI_DGREY    ""
  #define ANSI_GOLD     ""
  #define ANSI_ORANGE   ""
  #define ANSI_CREAM    ""
  #define ANSI_TEAL     ""
  #define ANSI_TURQS    ""
  #define ANSI_LIME     ""
  #define ANSI_WHITE    ""
  #define ANSI_END      ""
#elif(ANSI_MODE == 1)
  #define ANSI_RED      "\x1B[31m"
  #define ANSI_GREEN    "\x1B[32m"
  #define ANSI_YELLOW   "\x1B[33m"
  #define ANSI_BLUE     "\x1B[94m"
  #define ANSI_VIOLET   "\x1B[35m"
  #define ANSI_MAGENTA  "\x1B[35m"
  #define ANSI_PINK     "\x1B[95m"
  #define ANSI_CYAN     "\x1B[36m"
  #define ANSI_GREY     "\x1B[37m"
  #define ANSI_DGREY    "\x1B[90m"
  #define ANSI_GOLD     "\x1B[33m"
  #define ANSI_ORANGE   "\x1B[91m"
  #define ANSI_CREAM    "\x1B[97m"
  #define ANSI_TEAL     "\x1B[36m"
  #define ANSI_TURQS    "\x1B[96m"
  #define ANSI_LIME     "\x1B[92m"
  #define ANSI_WHITE    "\x1B[97m"
  #define ANSI_END      "\x1B[0m"
#else
  #define ANSI_RED      "\x1B[38;5;167m"  /* #D75F5F */
  #define ANSI_GREEN    "\x1B[38;5;71m"   /* #5FD7A7 */
  #define ANSI_YELLOW   "\x1B[38;5;221m"  /* #FFD75F */
  #define ANSI_BLUE     "\x1B[38;5;69m"   /* #5F87D7 */
  #define ANSI_VIOLET   "\x1B[38;5;99m"   /* #875FD7 */
  #define ANSI_MAGENTA  "\x1B[38;5;135m"  /* #AF87D7 */
  #define ANSI_PINK     "\x1B[38;5;175m"  /* #D787AF */
  #define ANSI_CYAN     "\x1B[38;5;44m"   /* #00D7D7 */
  #define ANSI_GREY     "\x1B[38;5;245m"  /* #8A8A8A */
  #define ANSI_DGREY    "\x1B[38;5;240m"  /* #585858 */
  #define ANSI_GOLD     "\x1B[38;5;184m"  /* #D7D700 */
  #define ANSI_ORANGE   "\x1B[38;5;173m"  /* #D7875F */
  #define ANSI_CREAM    "\x1B[38;5;230m"  /* #FFFFD7 */
  #define ANSI_TEAL     "\x1B[38;5;75m"   /* #5FAFFF */
  #define ANSI_TURQS    "\x1B[38;5;79m"   /* #5FD7AF */
  #define ANSI_LIME     "\x1B[38;5;112m"  /* #87D75F */
  #define ANSI_WHITE    "\x1B[38;5;15m"   /* #FFFFFF */
  #define ANSI_END      "\x1B[0m"
#endif

#define ANSI_OK " " ANSI_GREEN "OK" ANSI_END

//------------------------------------------------------------------------------------------------- macros

#define _args6(a,b,c,d,e,f,name,...) name
#define _args5(a,b,c,d,e,name,...) name
#define _args4(a,b,c,d,name,...) name
#define _args3(a,b,c,name,...) name
#define _args2(a,b,name,...) name

#define _min2(a,b) ({ \
  __auto_type _a = (a); \
  __auto_type _b = (b); \
  _a < _b ? _a : _b; \
})
#define _min3(a,b,c)     _min2(_min2(a,b), (c))
#define _min4(a,b,c,d)   _min2(_min3(a,b,c), (d))
#define _min5(a,b,c,d,e) _min2(_min4(a,b,c,d), (e))
#define _min6(a,b,c,d,e,f) _min2(_min5(a,b,c,d,e), (f))
/**
 * @brief Return minimum of provided values.
 * @note Supports 2–6 arguments. Single evaluation per argument.
 * @return Smallest of given arguments.
 */
#define minv(...) (_args6(__VA_ARGS__, _min6, _min5, _min4, _min3, _min2)(__VA_ARGS__))

#define _max2(a,b) ({ \
  __auto_type _a = (a); \
  __auto_type _b = (b); \
  _a > _b ? _a : _b; \
})
#define _max3(a,b,c)     _max2(_max2(a,b), (c))
#define _max4(a,b,c,d)   _max2(_max3(a,b,c), (d))
#define _max5(a,b,c,d,e) _max2(_max4(a,b,c,d), (e))
#define _max6(a,b,c,d,e,f) _max2(_max5(a,b,c,d,e), (f))
/**
 * @brief Return maximum of provided values.
 * @note Supports 2–6 arguments. Single evaluation per argument.
 * @return Greatest of given arguments.
 */
#define maxv(...) (_args6(__VA_ARGS__, _max6, _max5, _max4, _max3, _max2)(__VA_ARGS__))

/**
 * @brief Return sign of number.
 * @param value Input value.
 * @return `-1` if negative, `0` if zero, `1` if positive.
 */
#define sign_of(value) ({ \
  __typeof__(value) _v = (value); \
  (_v > 0) - (_v < 0); \
})

/**
 * @brief Safe ABS for integer and floating-point types.
 *   Signed integers: |x| with saturation (MIN → MAX to avoid UB).
 *   Unsigned integers: returns x unchanged.
 *   Floating point: uses fabsf/fabs/fabsl.
 */
#define absv(value) ({ \
  __auto_type _v = (value); \
  _Generic((_v), \
    signed char: (signed char)(_v == SCHAR_MIN ? SCHAR_MAX : (_v < 0 ? -_v : _v)), \
    short: (short)(_v == SHRT_MIN ? SHRT_MAX : (_v < 0 ? -_v : _v)), \
    int: (int)(_v == INT_MIN ? INT_MAX : (_v < 0 ? -_v : _v)), \
    long: (long)(_v == LONG_MIN ? LONG_MAX : (_v < 0 ? -_v : _v)), \
    long long: (long long)(_v == LLONG_MIN ? LLONG_MAX : (_v < 0 ? -_v : _v)), \
    unsigned char: _v, \
    unsigned short: _v, \
    unsigned int: _v, \
    unsigned long: _v, \
    unsigned long long: _v, \
    float: fabsf(_v), \
    double: fabs(_v), \
    long double: fabsl(_v), \
    default: (_v < 0 ? -_v : _v) \
  ); \
})

#ifndef __ZEPHYR__
/**
 * @brief Clamp value to `[min, max]` range.
 * @param value Value to clamp.
 * @param min Minimum allowed value.
 * @param max Maximum allowed value.
 * @return Clamped value in range `[min, max]`.
 */

#define clamp(value, min, max) ({ \
  __typeof__(value) _v   = (value); \
  __typeof__(value) _min = (min); \
  __typeof__(value) _max = (max); \
  _v < _min ? _min : (_v > _max ? _max : _v); \
})
#endif

/**
 * @brief Check if value is inside `[min, max]` range (inclusive).
 * @param value Value to check.
 * @param min Minimum allowed value.
 * @param max Maximum allowed value.
 * @return `true` if value ∈ [min, max], else `false`.
 */
#define in_range(value, min, max) ({ \
  __typeof__(value) _v  = (value); \
  __typeof__(value) _min = (min); \
  __typeof__(value) _max = (max); \
  (_v >= _min) && (_v <= _max); \
})

/**
 * @brief Swap two variables of the same type.
 * @param a First variable.
 * @param b Second variable.
 */
#define swap(a, b) do { \
  __typeof__(a) _tmp = (a); \
  (a) = (b); \
  (b) = _tmp; \
} while(0)

/**
 * @brief Get number of elements in static array.
 * @param x Static array (not a pointer).
 * @return Element count (e.g. 4 for `int x[4]`).
 */
#define array_len(x) (sizeof(x) / sizeof((x)[0]) + \
  0 * sizeof(struct { _Static_assert(!__builtin_types_compatible_p(__typeof__(x), __typeof__(&(x)[0])), "not array"); }))

/**
 * @brief Suppress unused variable/parameter warning.
 * @param x Variable or parameter to mark as intentionally unused.
 */
#define unused(x) ((void)(x))

/**
 * @brief Mark intentional switch-case fallthrough.
 * @note Use as standalone statement: `fallthrough;` right before next `case`.
 *   Silences `-Wimplicit-fallthrough` on GCC/Clang.
 *   On other compilers becomes no-op.
 */
#if defined(__GNUC__) || defined(__clang__)
  #define fallthrough __attribute__((fallthrough))
#else
  #define fallthrough ((void)0)
#endif

//------------------------------------------------------------------------------------------------- TRY

#define _try2(err, fn) { (err) = (fn); if(err) break; }
#define _try3(err, fn, code) { (err) = (fn); if(err) { (err) = (code); break; } }
#define _try5(err, fn, code, log_fn, msg) { (err) = (fn); if(err) { log_fn(msg); (err) = (code); break; } }

/**
 * @brief Break on error (variadic macro).
 * @example try_break(err, fn): Break on error
 * @example try_break(err, fn, code): Break on error, override with code
 * @example try_break(err, fn, code, log_fn, msg): Break on error, override with code and log
 */
#define try_break(...) _args5(__VA_ARGS__, _try5, _try3, _try3, _try2)(__VA_ARGS__)


//-------------------------------------------------------------------------------------------------
#endif

// lib/ext/xdef.h

#define ON  1 // Enabled
#define OFF 0 // Disabled

#ifndef XDEF_H_
#define XDEF_H_

#include <limits.h>
#include <math.h>

//------------------------------------------------------------------------------------------ Status

// `OK`, `FREE` and `IDLE` share value `0`: a function reports the one that fits
typedef enum {
  OK   = 0, // Operation finished
  FREE = 0, // Resource available
  IDLE = 0, // Nothing to do
  ERR  = 1, // Error occurred
  BUSY = 2  // Resource busy
} status_t;

//------------------------------------------------------------------------------------------- Float

#define NaN         NAN
#define Inf         INFINITY
#define isNaN(nbr)  isnan(nbr)
#define isInf(nbr)  isinf(nbr)

//-------------------------------------------------------------------------------------------- Bits

/**
 * @brief Set bit `bit` of `reg`, width follows `__typeof__(reg)`.
 * @param reg Register or variable to modify
 * @param bit Bit index, `0` = LSB
 */
#define bit_set(reg, bit)  ((reg) |= ((__typeof__(reg))1 << (bit)))

/**
 * @brief Clear bit `bit` of `reg`, width follows `__typeof__(reg)`.
 * @param reg Register or variable to modify
 * @param bit Bit index, `0` = LSB
 */
#define bit_rst(reg, bit)  ((reg) &= ~((__typeof__(reg))1 << (bit)))

/**
 * @brief Toggle bit `bit` of `reg`, width follows `__typeof__(reg)`.
 * @param reg Register or variable to modify
 * @param bit Bit index, `0` = LSB
 */
#define bit_tgl(reg, bit)  ((reg) ^= ((__typeof__(reg))1 << (bit)))

/**
 * @brief Read bit `bit` of `reg`.
 * @param reg Register or value to read
 * @param bit Bit index, `0` = LSB
 * @return `1` if set, `0` if cleared
 */
#define bit_get(reg, bit)  (((reg) >> (bit)) & 1u)

//-------------------------------------------------------------------------------------------- ANSI

#ifndef ANSI_MODE
  // Terminal color mode: `0` disabled, `1` basic 16-color, `2` 256-color
  #define ANSI_MODE 2
#endif

#if(ANSI_MODE == 0)
  #define ANSI_MAROON   ""
  #define ANSI_RED      ""
  #define ANSI_SALMON   ""
  #define ANSI_ORANGE   ""
  #define ANSI_GOLD     ""
  #define ANSI_YELLOW   ""
  #define ANSI_CREAM    ""
  #define ANSI_LIME     ""
  #define ANSI_GREEN    ""
  #define ANSI_TURQUS   ""
  #define ANSI_TEAL     ""
  #define ANSI_CYAN     ""
  #define ANSI_SKY      ""
  #define ANSI_BLUE     ""
  #define ANSI_VIOLET   ""
  #define ANSI_PURPLE   ""
  #define ANSI_MAGNTA   ""
  #define ANSI_PINK     ""
  #define ANSI_GREY     ""
  #define ANSI_SILVER   ""
  #define ANSI_WHITE    ""
  #define ANSI_END      ""
#elif(ANSI_MODE == 1)
  #define ANSI_MAROON   "\x1B[31m"
  #define ANSI_RED      "\x1B[31m"
  #define ANSI_SALMON   "\x1B[91m"
  #define ANSI_ORANGE   "\x1B[91m"
  #define ANSI_GOLD     "\x1B[33m"
  #define ANSI_YELLOW   "\x1B[93m"
  #define ANSI_CREAM    "\x1B[97m"
  #define ANSI_LIME     "\x1B[92m"
  #define ANSI_GREEN    "\x1B[32m"
  #define ANSI_TURQUS   "\x1B[96m"
  #define ANSI_TEAL     "\x1B[36m"
  #define ANSI_CYAN     "\x1B[36m"
  #define ANSI_SKY      "\x1B[94m"
  #define ANSI_BLUE     "\x1B[94m"
  #define ANSI_VIOLET   "\x1B[35m"
  #define ANSI_PURPLE   "\x1B[35m"
  #define ANSI_MAGNTA   "\x1B[95m"
  #define ANSI_PINK     "\x1B[95m"
  #define ANSI_GREY     "\x1B[90m"
  #define ANSI_SILVER   "\x1B[37m"
  #define ANSI_WHITE    "\x1B[97m"
  #define ANSI_END      "\x1B[0m"
#else
  #define ANSI_MAROON   "\x1B[38;5;88m"   // #870000
  #define ANSI_RED      "\x1B[38;5;167m"  // #D75F5F
  #define ANSI_SALMON   "\x1B[38;5;181m"  // #D7AFAF
  #define ANSI_ORANGE   "\x1B[38;5;173m"  // #D7875F
  #define ANSI_GOLD     "\x1B[38;5;178m"  // #D7AF00
  #define ANSI_YELLOW   "\x1B[38;5;227m"  // #FFFF5F
  #define ANSI_CREAM    "\x1B[38;5;187m"  // #D7D7AF
  #define ANSI_LIME     "\x1B[38;5;112m"  // #87D700
  #define ANSI_GREEN    "\x1B[38;5;71m"   // #5FAF5F
  #define ANSI_TURQUS   "\x1B[38;5;79m"   // #5FD7AF
  #define ANSI_TEAL     "\x1B[38;5;37m"   // #00AFAF
  #define ANSI_CYAN     "\x1B[38;5;44m"   // #00D7D7
  #define ANSI_SKY      "\x1B[38;5;75m"   // #5FAFFF
  #define ANSI_BLUE     "\x1B[38;5;69m"   // #5F87FF
  #define ANSI_VIOLET   "\x1B[38;5;99m"   // #875FFF
  #define ANSI_PURPLE   "\x1B[38;5;134m"  // #AF5FD7
  #define ANSI_MAGNTA   "\x1B[38;5;170m"  // #D75FD7
  #define ANSI_PINK     "\x1B[38;5;168m"  // #D75F87
  #define ANSI_GREY     "\x1B[38;5;240m"  // #585858
  #define ANSI_SILVER   "\x1B[38;5;248m"  // #A8A8A8
  #define ANSI_WHITE    "\x1B[97m"
  #define ANSI_END      "\x1B[0m"
#endif

#define ANSI_OK " " ANSI_GREEN "OK" ANSI_END

//---------------------------------------------------------------------------------------- Dispatch

// Argument-count dispatch: pick the implementation named after the count
#define _args6(a,b,c,d,e,f,name,...) name
#define _args5(a,b,c,d,e,name,...) name
#define _args4(a,b,c,d,name,...) name
#define _args3(a,b,c,name,...) name
#define _args2(a,b,name,...) name

//-------------------------------------------------------------------------------------------- Math

#define _min2(a,b) ({ \
  __auto_type _a = (a); \
  __auto_type _b = (b); \
  _a < _b ? _a : _b; \
})
#define _min3(a,b,c)     _min2(_min2(a,b), (c))
#define _min4(a,b,c,d)   _min2(_min3(a,b,c), (d))
#define _min5(a,b,c,d,e) _min2(_min4(a,b,c,d), (e))
#define _min6(a,b,c,d,e,f) _min2(_min5(a,b,c,d,e), (f))

// Smallest of 2 to 6 arguments, each evaluated once
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

// Greatest of 2 to 6 arguments, each evaluated once
#define maxv(...) (_args6(__VA_ARGS__, _max6, _max5, _max4, _max3, _max2)(__VA_ARGS__))

// Sign of `value`: `-1`, `0` or `1`
#define sign_of(value) ({ \
  __typeof__(value) _v = (value); \
  (_v > 0) - (_v < 0); \
})

// Absolute value for any arithmetic type. Signed integers saturate at the type minimum
// (`INT_MIN` gives `INT_MAX`), so the negation never overflows.
// Typed helpers behind the dispatch: the compiler checks the unselected branches too
static inline signed char _absv_sc(signed char v)
{
  return v == SCHAR_MIN ? SCHAR_MAX : (v < 0 ? -v : v);
}
static inline short _absv_s(short v) { return v == SHRT_MIN ? SHRT_MAX : (v < 0 ? -v : v); }
static inline int _absv_i(int v) { return v == INT_MIN ? INT_MAX : (v < 0 ? -v : v); }
static inline long _absv_l(long v) { return v == LONG_MIN ? LONG_MAX : (v < 0 ? -v : v); }
static inline long long _absv_ll(long long v)
{
  return v == LLONG_MIN ? LLONG_MAX : (v < 0 ? -v : v);
}
static inline unsigned char _absv_uc(unsigned char v) { return v; }
static inline unsigned short _absv_us(unsigned short v) { return v; }
static inline unsigned int _absv_u(unsigned int v) { return v; }
static inline unsigned long _absv_ul(unsigned long v) { return v; }
static inline unsigned long long _absv_ull(unsigned long long v) { return v; }
#define absv(value) ({ \
  __auto_type _v = (value); \
  _Generic((_v), \
    signed char: _absv_sc, \
    short: _absv_s, \
    int: _absv_i, \
    long: _absv_l, \
    long long: _absv_ll, \
    unsigned char: _absv_uc, \
    unsigned short: _absv_us, \
    unsigned int: _absv_u, \
    unsigned long: _absv_ul, \
    unsigned long long: _absv_ull, \
    float: fabsf, \
    double: fabs, \
    long double: fabsl \
  )(_v); \
})

/**
 * @brief Clamp value to `[min, max]`.
 * @param value Value to clamp
 * @param min Lower bound
 * @param max Upper bound
 * @return Value inside `[min, max]`
 */
#define clamp(value, min, max) ({ \
  __typeof__(value) _v = (value); \
  __typeof__(value) _min = (min); \
  __typeof__(value) _max = (max); \
  _v < _min ? _min : (_v > _max ? _max : _v); \
})

/**
 * @brief Check if value lies inside `[min, max]`, bounds included.
 * @param value Value to check
 * @param min Lower bound
 * @param max Upper bound
 * @return `true` when inside
 */
#define in_range(value, min, max) ({ \
  __typeof__(value) _v = (value); \
  __typeof__(value) _min = (min); \
  __typeof__(value) _max = (max); \
  (_v >= _min) && (_v <= _max); \
})

/**
 * @brief Swap two variables of one type.
 * @param a First variable
 * @param b Second variable
 */
#define swap(a, b) do { \
  __typeof__(a) _tmp = (a); \
  (a) = (b); \
  (b) = _tmp; \
} while(0)

//----------------------------------------------------------------------------------------- Helpers

// Element count of a static array, a decayed pointer fails to compile
#define array_len(x) (sizeof(x) / sizeof((x)[0]) + \
  0 * sizeof(struct { _Static_assert(!__builtin_types_compatible_p( \
    __typeof__(x), __typeof__(&(x)[0])), "not array"); }))

// Silence an unused variable or parameter warning
#define unused(x) ((void)(x))

// Intentional `switch` fallthrough, written as a statement right before the next `case`
#if defined(__GNUC__) || defined(__clang__)
  #define fallthrough __attribute__((fallthrough))
#else
  #define fallthrough ((void)0)
#endif

//------------------------------------------------------------------------------------------- Chain

#define _try2(err, fn) { (err) = (fn); if(err) break; }
#define _try3(err, fn, code) { (err) = (fn); if(err) { (err) = (code); break; } }
#define _try5(err, fn, code, log_fn, msg) \
  { (err) = (fn); if(err) { log_fn(msg); (err) = (code); break; } }

// Break out of a `do { } while(0)` chain on failure: `(err, fn)` keeps the status,
// `(err, fn, code)` overrides it with `code`, `(err, fn, code, log_fn, msg)` logs first
#define try_break(...) _args5(__VA_ARGS__, _try5, _try3, _try3, _try2)(__VA_ARGS__)

//-------------------------------------------------------------------------------------------------
#endif

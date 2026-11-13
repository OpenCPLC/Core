# C Style Guide

C on top of [`style.md`](style.md), which holds the rules every language shares.
Read both before editing.

`MODULE`, `dev`, `REG` in examples are toys, never a real API to copy from.
The one exception is [`xdef.h`](#standard-utilities-xdefh), where the rule *is* which helper to reach for.
The OpenCPLC framework has its own rules in `plcstyle.md` of the Core repo.

**Layout** · [Line endings](#line-endings) · [Tables](#tables) · [Braces](#braces) · [Directives](#directives) · [Flag words and formats](#flag-words-and-formats) · [Separators](#separators) · [File header](#file-header)

**Comments** · [Backticks](#backticks)

**Naming** · [Table](#table) · [Types and enums](#types-and-enums) · [Acronyms](#acronyms) ·
[Macros](#macros) · [Internal prefix](#internal-prefix) · [Static functions](#static-functions) ·
[Units](#units) · [Parameters by role](#parameters-by-role) · [Variants](#variants)

**[Library and application](#library-and-application)**

**Header** · [Include guard](#include-guard) · [Includes](#includes) ·
[Configurable defaults](#configurable-defaults) · [Struct](#struct) ·
[Function Doxygen](#function-doxygen) · [Macro Doxygen](#macro-doxygen) ·
[Inline definitions](#inline-definitions) · [Section order](#section-order)

**Source** · [Includes](#includes-1) · [Config](#config) · [Internal structs](#internal-structs) ·
[Singleton](#singleton) · [Static helpers](#static-helpers)

**Patterns** · [Init](#init) · [Returns](#returns) · [Error flow](#error-flow) ·
[Variadic lists](#variadic-lists) · [Conditional compilation](#conditional-compilation) ·
[Macros](#macros-1) · [Literals](#literals) · [Volatile](#volatile) · [String dispatch](#string-dispatch)

**[Example](#example)** · **[`xdef.h`](#standard-utilities-xdefh)**

---

## Layout

### Line endings

Line endings follow the repository and never mix inside a file: a project tree keeps whatever it was created with.

### Tables

```c
static const PIN_t PINS[] = {
  { GPIOA, 5,  GPIO_Mode_Output, GPIO_Pull_None }, // LD2, the board LED
  { GPIOC, 13, GPIO_Mode_Input,  GPIO_Pull_Up   }, // B1, the user button, active low
  { GPIOA, 2,  GPIO_Mode_Alt,    GPIO_Pull_None }, // USART2 TX, the ST-LINK bridge
};
```

An enum that is a lookup table, hash values, register codes, is a table too:
name, value and note as columns.
An ordinary enum or define run is not, and takes one space around `=`.

```c
typedef enum {
  HASH_List = 2090473057,
  HASH_Off  = 193501344,
  HASH_Addr = 2090071808,
} HASH_t;
```

### Braces

Control structures: same-line brace, no space before `(`.
Function definitions: new-line brace.
A `static` helper with a single statement may collapse, see [Static helpers](#static-helpers).

```c
if(x) {
  // ...
}
else {
  // ...
}

for(uint16_t i = 0; i < len; i++) {
  // ...
}

switch(state) {
  case STATE_A:
    // ...
    break;
  default:
    break;
}

void MODULE_Init(MODULE_t *dev)
{
  // ...
}
```

Guard clauses are one-liners:

```c
if(dev->_busy) return BUSY;
if(!count) return 0;
```

### Directives

A directive sits at the indent of the code around it, never forced to column zero.
A directive nested in another directive indents **2**.
At file level, code inside a directive keeps the column it would have without it.
Inside a function, the code under a directive indents **2** past it.
The include guard is the one directive that indents nothing: the file inside it stays at column zero.

```c
#ifndef MODULE_BUFFER_SIZE
  #define MODULE_BUFFER_SIZE 32
#endif

#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif

#if(MODULE_FEATURE_ENABLE)
void MODULE_Feature(void)
{
  step_measure();
  #if(FOC_ENABLED)
    step_vector();
  #endif
}
#endif
```

### Flag words and formats

A flag word that wraps gives every term a line of its own, the operator first:

```c
uart->reg->CR3 |= USART_CR3_DMAT
  | USART_CR3_OVRDIS
  | USART_CR3_EIE;
```

A call with a format string, a log macro like `LOG_Info` or `printf`, keeps the format on its first line and moves every argument to the next:

```c
LOG_Warning("Transfer aborted after %.1fs, %u bytes sent",
  (double)elapsed_ms / 1000, sent);
```

### Separators

Besides the named form of [`style.md`](style.md#separators), C has a bare one: `//` and dashes alone.
It closes a file, or stands between unnamed blocks.
It can take its name on the next line as a plain comment: when the name wants more than one word, or when the block opens with a sentence.
A blank line under the separator leaves the block unnamed.

```c
//--------------------------------------------------------------------------------------------- API
//---------------------------------------------------------------------------------------- Internal
//-------------------------------------------------------------------------------------------------
// ISR context
```

```c
  switch(function_code) {
    //---------------------------------------------------------------------------------------------
    case MODBUS_Fnc_ReadBits:
```

A blank line follows a separator.
A `#if` guarding the whole section hugs it instead:
the directive right under the separator, its `#endif` right above the next.

```c
//-------------------------------------------------------------------------------------------- Heap
#if(QUEUE_USE_HEAP)

static void heap_sift_up(QUEUE_t *queue, uint16_t i)
{
  // ...
}

#endif
//--------------------------------------------------------------------------------------------- API
```

Separates API blocks, internal helpers, feature groups, header sections.
The usual names: `Config`, `Types`, `Constants`, `Presets`, `Structure`, `Globals`, `Init`, `API`, `Internal`.

### File header

The [path line](style.md#file-header) is a logical address in C, not a `__FILE__` mirror.
It usually matches the disk.

It may diverge from the disk when the logical role matters more than the location:
- A generated or pre-processed file whose location the build system sets
- A family-shared abstraction with one logical name backed by several physical files picked at build time
- A module of one layer that lives elsewhere for packaging, named by its layer

That divergence is a deliberate choice, and the path stays unique and stable across moves.
A forgotten rename is a bug, not a virtual path.

---

## Comments

### Backticks

In C a code symbol is any of these, and takes backticks in a comment or a Doxygen line:

- Functions: `MODULE_Init`, `TIM_Enable`
- Types: `UART_t`, `status_t`
- Fields: `_busy`, `dev->reg`
- Enum values and constants: `OK`, `BUSY`, `STATE_IDLE`
- Macros: `NULL`, `M_PI`, `BUFFER_SIZE`
- Literals: `true`, `false`, `0x42`, `0`
- Registers and bits: `TIM_CR1_CEN`, `USART_ISR_TC`
- Names that are not C identifiers, such as a register map's `Group:Register`: `Ctrl:Mode`, `Limit:BusMax`.
  The grep test does not reach them on its own

```c
// Wait for `_busy` to clear, then drain `RDR`
if(uart->_busy) return BUSY;

/**
 * @brief Send buffer.
 * @param[in] data Bytes to send, never `NULL`
 * @return `OK` on success, `BUSY` while a previous `UART_Write` runs
 */
```

---

## Naming

### Table

| Category                 | Convention                       | Example                                |
| ------------------------ | -------------------------------- | -------------------------------------- |
| All types                | `PascalCase_t` or `snake_case_t` | `UART_t`, `EEPROM_Mode_t`, `status_t`  |
| Module types             | `MODULE_t` / `MODULE_Name_t`     | `EEPROM_t`, `EEPROM_Mode_t`            |
| Utility types            | `snake_case_t`                   | `ary_t`, `status_t`                    |
| Enum values _(module)_   | `MODULE_Name_Value`              | `EEPROM_Mode_Idle`, `UART_Parity_None` |
| Enum values _(utility)_  | `UPPER_CASE`                     | `OK`, `ERR`, `BUSY`, `NaN`             |
| Defines _(module)_       | `MODULE_NAME`                    | `UART_BUFFER_SIZE`, `I2C_MAX_RETRY`    |
| Functions _(module)_     | `MODULE_PascalCase`              | `MODULE_Init`, `UART_Write`            |
| Functions _(utility)_    | `snake_case`                     | `div_round`, `hash_djb2`               |
| Variables                | `snake_case`                     | `rx_len`, `page_count`                 |
| Globals _(extern state)_ | `PascalCase`                     | `RtcReady`, `TaskTicker`               |
| Globals _(extern table)_ | `MODULE_NAME_TAB`, `_MAP`        | `ADC_PRESCALER_TAB`, `UART_TX_MAP`     |
| Macros _(with args)_     | like a function of its scope     | `bit_set`, `clamp`, `QUEUE_New`        |
| Macros _(constant)_      | `UPPER_CASE`                     | `M_PI`, `NaN`                          |
| Internal                 | `_prefix`                        | `_head`, `_busy`, `_MODBUS_WriteBits`  |
| Unit suffix              | `_Hz`, `_V`, `_ms`, `_us`, `_C`  | `freq_Hz`, `timeout_ms`, `temp_C`      |

### Types and enums

All types end in `_t`, enums and function-pointer typedefs included.
Never `_e`, `_s`, `_enum`, `_fn`.

Enum values follow the casing of their type:
`MODULE_Name_t` gives `MODULE_Name_Value`, `snake_case_t` gives plain `UPPER_CASE` without prefix.

Callback fields are PascalCase whatever their direction: `OnReceive`, `OnTransmit`, `FallHandler`.
Direction goes in `@param[in]` / `@param[out]`, not in the name.

### Acronyms

An acronym keeps its capitals inside a PascalCase name:
`IRQ_EnableTIM`, `RCC_EnableUART`, `AIN_IsOK`, `DAC_SetCH2`, `MODULE_ISR`.
`EnableTim` reads as a word, and it is not one.
In `snake_case` it is lowercase like every other word: `rx_dma`, `image_crc`, `adc_isr`.

### Macros

Casing follows the parameter list, not taste.
A macro taking arguments is called like a function and named like one of its scope:
`snake_case` for a utility, `MODULE_Pascal` for a module.
A macro standing for a fixed value is named like a constant.

```c
#define clamp(value, min, max) ...    // utility with arguments → snake_case
#define QUEUE_New(name, type, n) ...  // module with arguments → MODULE_Pascal
#define M_PI 3.14159265358979f        // fixed value → UPPER_CASE
#define UART_BUFFER_SIZE 32           // fixed value → UPPER_CASE
```

A module-scoped define carries the module prefix.
Only `xdef.h` primitives stand without one, `NaN`, `OK`, `M_PI`,
because they are present in every translation unit.

### Internal prefix

`_prefix` marks what the API user never touches.
After the underscore the name follows the table.

```c
uint16_t _head;                                // struct field, internal state
static MODBUS_Error_t _MODBUS_WriteBits(...);  // worker behind `MODBUS_WriteBits`
```

On a struct field it is the rule.
On a `static` function it is needed once, where the worker and its public wrapper carry one name:
the prefix makes the pair legal and tells the call site which of the two it reached.
Anywhere else it is optional, `static` already says private.

Two underscores open the toolchain's own names: `__attribute__`, `__builtin_clz`, `__WFI`.
A name written here opens with one at most.

### Static functions

A `static` function is named by what it is, not by being `static`:

- `MODULE_Pascal` when it is an operation of the module, a step that could be public tomorrow:
  `SENSOR_Sample`, `LINK_Teardown`, `FRAME_Encode`
- `snake_case` when it is a local helper of the file:
  `frame_fits`, `ring_wrap`, `clock_running`, `limit_set`
- `_MODULE_Pascal` when it is the internal half of a public function and shares its stem:
  `_MODBUS_WriteBits` behind `MODBUS_WriteBits`

A library file is mostly helpers, an application module mostly operations,
and one file can hold both.

### Units

A variable or field holding a physical quantity carries the unit suffix, every time.
`timeout` alone is a defect; `timeout_ms` is the name.
The suffix keeps the unit's own casing whatever the name around it:
`MODULE_WDG_ms`, `SENSOR_VREF_mV`, `BATTERY_LOW_pct`, `offset_mV`.
A quantity stands between the module prefix and the unit: `MODULE_ENTRY_Hz`, not `MODULE_Hz`.

Where the name cannot carry it, the comment does, in square brackets at the end.
That covers fixed-point formats, hardware counter domains and tick cadences.

```c
#define MODULE_LOCK_N 40  // gate before a lock holds [detector ticks]
int32_t phase_err_q15;    // PLL unlock floor [Q15 ~ sin]
uint16_t angle_out;       // applied angle [2^16/turn]
int32_t speed_acc;        // speed PI integrator [counts << 10]
uint16_t margin;          // headroom under `Limit:BusMax` [V ×10]
```

A debounce or timeout constant names its domain:
`ticks` alone is ambiguous between ISR steps and detector cadence, which can differ by two orders.

### Parameters by role

No canonical name for buffers, lengths, or counters.
The name says what this is *in this function*, so the reader feels the role without the signature.

Pointers and buffers, by role not by type:
- `tx_data` / `rx_data` when direction matters
- `dst` / `src` for copy or move
- `record` for one structured item
- `var` for a variable saved to or loaded from storage
- `data` for a byte payload with no narrower role
- `buffer` for the holding area itself, not a payload
- `buf` / `buff` only where the API is buffer-centric and `BUFF_t` does not collide

Lengths, by what is counted:
- `len` for bytes of a raw buffer
- `count` for elements of a typed array
- `size` for a byte size that is itself the quantity: allocation, region, struct footprint

Loop counters, by loop weight:
- `i`, `j`, `k`, `n` for a short self-contained loop where the index means nothing past the iteration
- `idx`, `pos`, `cnt`, or full names, for nested loops, several counters in scope,
  or an index that carries meaning

One file can mix all of these. Local clarity beats global uniformity.

### Variants

A type-specific variant takes a type suffix:
`_u8`, `_i16`, `_u16`, `_i32`, `_u32`, `_f32`, `_i64`.

```c
void sort_asc_u16(uint16_t *array, uint16_t len);
int16_t median3_i16(int16_t a, int16_t b, int16_t c);
float filter_f32(float input, float prev, float alpha);
```

`_this` marks in-place mutation; no suffix returns a new copy:

```c
char *str_lower_this(char *str);  // mutates, returns same pointer
char *str_lower(char *str);       // allocates new string
```

---

## Library and application

The same style, three differences, each explained where it applies:

| | Library | Application |
| --- | --- | --- |
| Contract | Doxygen block in the `.h`, none in the `.c` | At the definition, most functions have no header |
| `<>` includes | stdlib, vendor | stdlib, vendor, and the framework |
| Header line | `// mod/module.h` | `// app/control.c` |

See [Function Doxygen](#function-doxygen), [Static helpers](#static-helpers), [Includes](#includes).

A library splits into topics, and a helper sits next to the API it serves.

---

## Header `.h`

A header is the module's contract:
types, configurable defaults, declarations, and definitions that must inline at call sites.

Doxygen there drives IDE hover hints.
Short `@brief`, one-line params, no prose, the [voice](style.md#voice) of any other comment.

### Include guard

```c
#ifndef MODULE_H_
#define MODULE_H_
// ...
#endif
```

Trailing underscore after `_H`. Never `#pragma once`.

### Includes

`<>` for anything outside the tree being edited: stdlib, vendor SDK, RTOS,
and the framework itself when the tree is an application.
`""` for the tree's own files.
Order: stdlib, vendor, project, a blank line between groups.
A family-conditional vendor header goes inside its conditional, see [Directives](#directives).

```c
#include <stdbool.h> // system/stdlib
#include <stdint.h>

#include <vendor.h> // vendor/SDK/RTOS

#include "common.h" // project-local
#include "utils.h"
```

### Configurable defaults

Module knobs live in the header as `#define` inside `#ifndef`,
so a `-D` flag or a wrapping config header can override them.
The comment goes between the two.

```c
#ifndef MODULE_BUFFER_SIZE
  // Maximum receive buffer size in bytes
  #define MODULE_BUFFER_SIZE 32
#endif
```

### Struct

Public config fields first, an `// internal` separator, then `_prefix` fields.
The Doxygen mirrors it: `@param[in]` on config fields, an `Internal:` label,
then plain `@param` on the rest.

```c
/**
 * @brief Short description.
 * @param[in] address Device address
 * @param[in] OnEvent Called on event, `NULL` disables
 * Internal:
 * @param _busy Operation in progress
 * @param _buffer Data buffer
 */
typedef struct {
  uint8_t address;
  void (*OnEvent)(const uint8_t *data, uint8_t len);
  // internal
  volatile bool _busy;
  uint8_t _buffer[MODULE_BUFFER_SIZE];
} MODULE_t;
```

The two separators are always paired:
`// internal` in code, lowercase, no colon; `Internal:` in Doxygen, capitalized, with colon, no `@`.

### Function Doxygen

Three forms, by how much the name leaves unsaid:

- A Doxygen block when the contract goes past the signature:
  what a parameter accepts, what the return means, a side effect
- A `//` line when one sentence says it
- Nothing when the name says it: `MODULE_IsBusy`, `QUEUE_Count`.
  Hover then shows the signature alone, and that is the point:
  a line restating the name would cost a line and say nothing

```c
// Initialize module, call once before use
void MODULE_Init(void);

bool MODULE_IsBusy(const MODULE_t *dev);

/**
 * @brief Send buffer.
 * @param[in,out] dev Module instance
 * @param[in] data Bytes to send
 * @param[in] len Byte count
 * @return `OK` on success, `BUSY` while a previous write runs
 */
status_t MODULE_Write(MODULE_t *dev, uint8_t *data, uint16_t len);
```

- `@brief` only in a block, never alone
- `@brief` ends with a period; `@param` and `@return` are fragments and do not
- `@param[in]` read, `@param[out]` written, `@param[in,out]` modified
- `@return` names its values in backticks: `OK` when found, `ERR` when absent
- Units in square brackets where the name has none: `@return Frequency [Hz]`
- `@note` for a fact past the contract, `@details` for a paragraph the brief cannot hold.
  Nothing else, never `@retval`

The block lives in the `.h`.
A `.c` carries one only where no header holds the contract, which is application code, see [Static helpers](#static-helpers).
A library `.c` never repeats it.

### Macro Doxygen

A macro takes the block when its arguments have an order or a meaning the name does not give.
`seconds(s)` says everything with a `//` line; `clamp(value, min, max)` has an order to get wrong.

```c
// Convert seconds to milliseconds
#define seconds(s) (1000 * (s))

/**
 * @brief Clamp value to `[min, max]`.
 * @param[in] value Value to clamp
 * @param[in] min Lower bound
 * @param[in] max Upper bound
 * @return Clamped value
 */
#define clamp(value, min, max) ({ ... })
```

### Inline definitions

A trivial public function that must inline at every call site is `static inline` in the `.h`.
The compiler must see the body to inline it; `inline` in a `.c` still costs callers from other files a `bl`.
`static inline` in a header is the C idiom for it, the one Linux, Zephyr, ESP-IDF and glibc use.

```c
/**
 * @brief Start timer.
 * @param[in,out] tim Timer instance
 */
static inline void TIM_Enable(TIM_t *tim)
{
  tim->reg->CR1 |= TIM_CR1_CEN;
  tim->reg->SR &= ~TIM_SR_UIF;
  tim->enable = true;
}
```

It qualifies when it is a setter or a flag flip: a few register or field writes,
no branch, no loop, no call into a `.c`. Anything more stays in the `.c`.

The definition is also the declaration; no separate `void TIM_Enable(TIM_t *tim);` elsewhere.

### Section order

Skipping what a file has nothing for:

1. File header `// path`
2. Include guard
3. Includes
4. Configurable defaults
5. Presets, constants
6. Types
7. Pin maps, extern tables
8. Structs
9. API declarations
10. Inline definitions

[Separators](#separators) between sections.

---

## Source `.c`

Implementation, internal state, statics, ISRs.

### Includes

Own header first, then the groups of the header, a blank line between them.

```c
#include "module.h" // own header first

#include <string.h> // stdlib next

#include "helper.h" // project-local last
```

### Config

Constants private to the file sit in a `Config` section at the top,
`#define` with the unit in the suffix, related ones grouped by a blank line.
A comment above one that says something the name does not, none above one that does.
A value used once, whose name would only repeat where it is used, takes no `#define` and stays a literal there.

```c
//------------------------------------------------------------------------------------------ Config

// Hysteresis: `SET` trips, `RST` clears
#define MODULE_TEMP_TRIP_SET_C 45.0f
#define MODULE_TEMP_TRIP_RST_C 42.0f

// Boot grace before staleness bits judge readings still warming up
#define MODULE_SETTLE_ms 5000

#define MODULE_WDG_ms 10000
```

### Internal structs

A struct that never leaves the `.c` takes `//` notes, not Doxygen:

```c
typedef struct {
  SCHED_Task_t threads[SCHED_THREAD_LIMIT];
  uint32_t i;             // active thread index
  uint32_t count;         // thread count
  volatile bool enabled;  // switching enabled flag
  volatile bool init;     // initialization flag
} SCHED_t;
```

### Singleton

When an ISR or callback needs the user's struct:

```c
static MODULE_t *self;

void MODULE_Init(MODULE_t *dev)
{
  self = dev;
  // ...
}
```

### Static helpers

Brace form follows the body:

1. One statement that fits the line: the whole helper on one line
2. One statement that does not fit: same-line brace, body on its own line
3. Two or more statements, or any control flow: new-line brace

```c
static inline void enable(void) { REG->CR |= ENABLE_MASK; }

static inline bool is_ready(MODULE_t *dev) {
  return !dev->_busy && dev->_cursor < dev->limit;
}

static void process(MODULE_t *dev)
{
  if(dev->_busy) return;
  dev->_cursor++;
  notify(dev);
}
```

`inline` in a `.c` is only for `static` helpers of that file.
A public function is never `inline` in a `.c`: it has no effect across translation units.
For that, move it to the `.h`, see [Inline definitions](#inline-definitions).

Names follow [Static functions](#static-functions).

**Library code.** A `//` line above the helper where the name leaves something unsaid,
nothing where it does not.
A Doxygen block only when the contract cannot be read off the signature:
atomicity, ordering against an ISR, unit of the return value, ownership of a returned pointer.

```c
/**
 * @brief Atomically read 64-bit ticker on 32-bit core.
 * @return Current tick value
 */
static inline uint64_t sched_ticker_get(void)
{
  // ...
}
```

**Application code.** Most of what it defines has no header,
so a function with parameters or a return value carries at its definition the block a header would give it, `static` or not.
That is the better form there: the contract sits where the code is read.
Which of the two is a judgement, not a count of parameters:
`static void LINK_Teardown(bool sleep)` under `// Counters flushed, peripherals released, then out` says all it needs;
`LINK_Encode` with four parameters and a return earns the block.

---

## Patterns

### Init

The user fills the struct at declaration and passes a pointer to init. No setters.
An initializer gives each field a line of its own, never packed by force: a row per line is only for a table of uniform, short rows, see [Tables](#tables).

```c
static MODULE_t dev = {
  .address = 0x12,
  .OnReceive = handler,
};

MODULE_Init(&dev);
```

### Returns

```c
status_t MODULE_Write(...);  // `OK`, `ERR`, `BUSY` for a failable operation
bool MODULE_IsBusy(...);     // `true` / `false` for a query
void *get_item(...);         // pointer, `NULL` on lookup miss
uint32_t MODULE_Read(...);   // direct value for a read
// Value with fallback
uint32_t MODULE_Get(..., uint32_t default_value);
```

### Error flow

Early return on error, no nesting:

```c
status_t MODULE_Init(MODULE_t *dev)
{
  if(check_config(dev)) return ERR;
  if(MODULE_Reset(dev)) return ERR;
  if(OTHER_Init()) return ERR;
  return OK;
}
```

The code travels where the caller can act on it, `if(err) return err;`,
and flattens to `ERR` where every failure means the same to it.
A chain with shared cleanup uses `try_break`, see [Error chaining](#error-chaining).

### Variadic lists

`NULL` closes the list:

```c
status_t MODULE_SaveList(MODULE_t *dev, uint32_t *var, ...)
{
  va_list args; va_start(args, var);
  status_t status = OK;
  while(var) {
    if(MODULE_Save(dev, var)) status = ERR;
    var = va_arg(args, uint32_t *);
  }
  va_end(args);
  return status;
}

MODULE_SaveList(&dev, &a, &b, &c, NULL);
```

### Conditional compilation

A feature toggle defaults in the header, as [Configurable defaults](#configurable-defaults) shows, and guards its code with `#if(...)`.
A platform or a family picks its header with `#if defined(...)`.
Both look as in [Directives](#directives).

### Macros

Every argument wrapped in `(...)` at every use:

```c
#define is_error(s) ((s) >= ERR_FIRST)
```

Single evaluation via `__auto_type` / `__typeof__`:

```c
#define minv(a, b) ({ \
  __auto_type _a = (a); \
  __auto_type _b = (b); \
  _a < _b ? _a : _b; \
})
```

A statement macro is a `do { ... } while(0)`:

```c
#define swap(a, b) do { \
  __typeof__(a) _tmp = (a); \
  (a) = (b); \
  (b) = _tmp; \
} while(0)
```

A declaration macro may bundle a struct with its backing storage:

```c
#define collection_new(name, type, capacity) \
  type name##_data[capacity]; \
  collection_t name = { .value = name##_data, .limit = (capacity), \
    .element_size = sizeof(type), ._head = 0, ._tail = 0, .count = 0 }
```

### Literals

Integer suffixes are lowercase: `1u`, `0xFFu`, `1ull`.
The suffix is a property of the number, not a word, and stays out of the way.

Suffix and cast answer different questions.
A suffix says what the number *is*, and belongs on a value that stands on its own:
a mask, a bound, a sentinel.
A cast says how wide the *arithmetic* must be, and belongs on an operand that would otherwise be computed in its natural, too-narrow type.

```c
#define EEPROM_ERASED_WORD 0xFFFFFFFFFFFFFFFFull // the value is the point
if(v > 0xFFFFFFFFull) v = 0xFFFFFFFFull; // a bound, likewise
reg |= (1u << pin); // 32-bit shift, no widening
mask = (uint64_t)1 << bit; // the shift needs 64 bits
total = (uint64_t)bits * len; // the product needs 64 bits
```

`uint64_t` over `unsigned long long`: the fixed-width types are the vocabulary the rest of the code speaks,
and `ull` only promises *at least* 64 bits.

### Volatile

`volatile` on a field written or read across an ISR or DMA-callback boundary, nowhere else.
Not on a field touched from one context, not on a polled value that never enters ISR scope.

```c
volatile bool _busy;      // set in ISR, polled from main
volatile uint8_t *_head;  // moved by DMA, read by main
```

### String dispatch

A string matched against several words goes through `hash_djb2`, or `hash_djb2_ci` for any case, into a `switch` over a `HASH_t` table like the one in [Tables](#tables), never through a ladder of `strcmp`.

---

## Example

One invented module, the rules above playing together.

```c
// mod/module.h

#ifndef MODULE_H_
#define MODULE_H_

#include <stdbool.h>
#include <stdint.h>

#include "bus.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef MODULE_BUFFER_SIZE
  // Receive buffer [B], one frame at most
  #define MODULE_BUFFER_SIZE 32
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  MODULE_Mode_Idle,
  MODULE_Mode_Poll,
  MODULE_Mode_Event,
} MODULE_Mode_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief One device on the bus.
 * @param[in] address Device address
 * @param[in] mode Polled from the main loop, or driven by the bus interrupt
 * @param[in] OnEvent Called when a frame arrives, `NULL` disables
 * Internal:
 * @param _busy Transfer in progress
 * @param _buffer Frame under assembly
 */
typedef struct {
  uint8_t address;
  MODULE_Mode_t mode;
  void (*OnEvent)(const uint8_t *data, uint8_t len);
  // internal
  volatile bool _busy;
  uint8_t _buffer[MODULE_BUFFER_SIZE];
} MODULE_t;

//--------------------------------------------------------------------------------------------- API

// Bind the bus and reset state, call once before use
void MODULE_Init(MODULE_t *dev);

/**
 * @brief Send buffer.
 * @param[in,out] dev Module instance
 * @param[in] data Bytes to send
 * @param[in] len Byte count
 * @return `OK` when queued, `BUSY` while a previous write runs, `ERR` when the frame does not fit
 */
status_t MODULE_Write(MODULE_t *dev, const uint8_t *data, uint16_t len);

bool MODULE_IsBusy(const MODULE_t *dev);

//------------------------------------------------------------------------------------------ Inline

/**
 * @brief Drop the frame under assembly.
 * @param[in,out] dev Module instance
 */
static inline void MODULE_Clear(MODULE_t *dev)
{
  dev->_busy = false;
}

//-------------------------------------------------------------------------------------------------
#endif
```

```c
// mod/module.c

#include "module.h"

#include <string.h>

//------------------------------------------------------------------------------------------ Config

// Gap the bus needs after the previous frame, not before this one
#define MODULE_GAP_us 50

//---------------------------------------------------------------------------------------- Internal

static MODULE_t *self; // ISR target, one device per build

static bool frame_fits(const MODULE_t *dev, uint16_t len) { return len <= sizeof(dev->_buffer); }

// ISR context, `self` is set by `MODULE_Init`
static void on_frame(uint8_t len)
{
  self->_busy = false;
  if(self->OnEvent) self->OnEvent(self->_buffer, len);
}

//--------------------------------------------------------------------------------------------- API

void MODULE_Init(MODULE_t *dev)
{
  self = dev;
  dev->_busy = false;
  BUS_Init(dev->address, dev->_buffer, dev->mode == MODULE_Mode_Event ? &on_frame : NULL);
}

status_t MODULE_Write(MODULE_t *dev, const uint8_t *data, uint16_t len)
{
  if(dev->_busy) return BUSY;
  if(!frame_fits(dev, len)) return ERR;
  memcpy(dev->_buffer, data, len);
  dev->_busy = true;
  delay_us(MODULE_GAP_us);
  BUS_Send(dev->address, dev->_buffer, len);
  return OK;
}

bool MODULE_IsBusy(const MODULE_t *dev)
{
  return dev->_busy;
}
```

---

## Standard Utilities `xdef.h`

`xdef.h` is the project's `stdlib.h`, present in every build.
Before a manual bit shift, a hand-rolled `min`/`max`, `sizeof(x)/sizeof(x[0])`,
or an `if(err) return err;` chain, check here.

### Status

`status_t` is the return type of any failable operation:

```c
status_t err = MODULE_Write(&dev, data, len);
if(err) return err;
```

- `OK`, `FREE`, `IDLE`: three names for `0`, chosen by what the function reports.
  `OK` when an operation finished, `FREE` when a resource is available, `IDLE` when there was nothing to do.
  A function reporting one of them never returns another in the same position
- `ERR` is `1`, `BUSY` is `2`
- Any non-zero is failure: `if(err)`, never `== OK`

### Bits

Width follows `__typeof__(reg)`, no cast needed:

```c
bit_set(reg, 3);  // reg |= (1 << 3)
bit_rst(reg, 3);  // reg &= ~(1 << 3)
bit_tgl(reg, 3);  // reg ^= (1 << 3)
bit_get(reg, 3);  // ((reg >> 3) & 1)
```

### Math

Single evaluation, type-generic:

- `minv(a, b)` through `minv(a, b, c, d, e, f)`, 2 to 6 arguments; `maxv(...)` the same
- `clamp(value, min, max)`
- `in_range(value, min, max)`, inclusive
- `sign_of(value)`: `-1`, `0` or `1`
- `absv(value)`: `_Generic` dispatch, saturates `INT_MIN` and kin instead of overflowing
- `swap(a, b)`

### Float specials

- `NaN`, `Inf` instead of `NAN`, `INFINITY`
- `isNaN(x)`, `isInf(x)` instead of `isnan(x)`, `isinf(x)`

### Helpers

- `array_len(x)`: element count of a static array, compile-asserts `x` is an array and not a decayed pointer
- `unused(x)`: silences an unused warning
- `fallthrough;`: intentional `switch` fallthrough, replaces the `// fallthrough` comment,
  `__attribute__((fallthrough))` on GCC and Clang, no-op elsewhere

### Error chaining

`try_break(...)` collapses an `if(err) break;` chain inside `do { ... } while(0)`:

```c
status_t err = OK;
do {
  try_break(err, MODULE_Open(&dev));
  try_break(err, MODULE_Configure(&dev), CONFIG_ERR); // override status
  try_break(err, MODULE_Start(&dev), START_ERR, LOG, "start"); // log and override
} while(0);
if(err) cleanup(&dev);
return err;
```

By argument count:
- `(err, fn)`: break on failure
- `(err, fn, code)`: break and override `err` with `code`
- `(err, fn, code, log_fn, msg)`: log, then break with `code`

For a chain long enough that shared cleanup would otherwise repeat.
A short one reads better as plain guard clauses.

### Variadic dispatch

`_args5` and `_args6` are the dispatch primitives behind `minv`, `maxv`, `try_break`.
Same pattern for an arg-count overload of your own:

```c
#define _foo2(a, b)     // 2-arg impl
#define _foo3(a, b, c)  // 3-arg impl
#define foo(...) _args3(__VA_ARGS__, _foo3, _foo2)(__VA_ARGS__)
```

### Terminal colors

`ANSI_RED`, `ANSI_GREEN`, `ANSI_CYAN`, ..., `ANSI_END` for log and shell output.
`ANSI_MODE` picks at compile time: `0` strips them, `1` basic 16-color, `2` 256-color, the default.

```c
LOG("status: " ANSI_GREEN "OK" ANSI_END);
```

### On / Off

`ON` / `OFF` sit outside the `xdef.h` include guard, so they survive a repeat include.
Readability aliases for feature toggles:

```c
#define MODULE_LOG_ENABLE ON
```

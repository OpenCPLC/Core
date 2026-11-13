# OpenCPLC Style

The framework's own rules, on top of `cstyle.md` for C and `style.md` for what every language shares.
The three together are the style, read in full before editing the framework or a project built on it.

**[Layers](#layers)** · **[Files](#files)** · **[Project `main.h`](#project-mainh)**

---

## Layers

- `hal/`: hardware abstraction, STM32 families and a host build. Strictest adherence to the style
- `lib/`: portable standard utilities, collections, shell with the USB device, system. Same rules as `hal/`
- `plc/`: PLC inputs, outputs and comms, `io/` and `com/`. Optional in a project
- `drv/`: device drivers, sensors, displays, GNSS. Work without `plc/`
- `brd/`: boards, the glue of one controller

Some code predates the style and diverges from it.
A function you edit comes to the style in the same commit.
A file you did not edit stays as it is: divergence alone is never a reason to open a file.

### Dependency direction

Includes go downward, never up:

```text
brd/  →  plc/  drv/  →  lib/  →  hal/
```

`plc/` and `drv/` are siblings and never include each other.
Within a layer, sibling includes are fine.

One file goes against the arrow: the project's `main.h`.
It is the configuration point, every layer includes it, and it is what `-D` flags and the configurable defaults of `cstyle.md` meet in.

### Vendor subtrees

Exempt from the style. Never reformatted, renamed, or migrated:

- `lib/ext/`: external libraries, one folder each: nanopb, Monocypher
- `hal/arm/`: CMSIS and newlib stubs
- `hal/stm32g0/inc/`, `hal/stm32wb/inc/`: CubeMX device headers
- `hal/svd/`: SVD-derived register maps

### Boards

One directory per controller under `brd/`, its `.ini` the manifest the tool reads:

```text
brd/opencplc.h
brd/uno/opencplc_uno.{c,h,ini}
```

A board file glues the abstract `plc/` API, `RO`, `TO`, `DI`, `AI`, `RS`,
to concrete pins, peripherals and memory.
One board per project, included via `brd/opencplc.h`.

Board objects carry the label printed on the enclosure, short and uppercase:
`RO1`, `DI4`, `RS2`, `RTD`, `BTN`, `POT`.

---

## Files

The framework tree is CRLF, and no file mixes line endings.
A file includes another by name alone, `#include "uart.h"`, never by a path: the build puts every layer on the include path.

---

## Project `main.h`

The project's `main.h` opens with a block naming it: `@name` as `Project:` and its path, `@brief` with what it does, and `@date`.
It sets every knob directly, and `#ifndef` around a knob belongs to the module header that declares it.

```c
// demo/blink/main.h

/**
 * @name  Project: demo/blink
 * @brief Blinks the board LED at `BLINK_ms`.
 * @date  2026-11-11
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <xdef.h>

#define MODULE_BUFFER_SIZE 64
#define BLINK_ms 500

#endif
```


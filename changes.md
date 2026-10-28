# Changes

Release history of **OpenCPLC Core**, newest first.

### `0.6.2` One bootloader

- **Bootloader** built by Forge with each image, from its `boot.h`
- Keys, power hold and board variant in `boot.h`
- **BLD** driver in place of `boot`, settings stay `BOOT_`
- Folders renamed to `drv/`, `bin/` and `plc/io/`
- Projects need Forge 0.5.1

### `0.6.1` PDB + M24x

- **PDB** format `1` with page headers, older bases read as none
- EEPROM only by key, values saved by address read as none
- **JRN** record with event data
- **M24x** driver, I2C master on host

### `0.6.0` Tests

- **Tests** on computer with `make test`, chip flash modeled in `hal/host`
- **EEPROM** and **PDB** survive power cuts, moved to `lib/sys`
- `FLASH_Save` records of older Core read as none

### `0.5.6` Bootloader

- **Bootloader** `key` variant starts only Ed25519-signed images
- Projects need Forge 0.4.8, standard library moved to `lib/std`

### `0.5.5` ADC

- ADC errata in driver, buffers sized by `adc_record_scan_max`
- LSE starts at medium-high drive, `RTC_LSE_DRIVE`
- Image header with chip and epoch, room for signature

### `0.5.4` Log

- Log levels gate at compile time, `LOG_LEVEL_NIL` silences the log
- UART presets for every standard baud

### `0.5.3` Flash

- Fixed `FLASH_WritePage` on a blank page, bootloaders rebuilt
- `FLASH_WritePage` in double words on STM32WB, bootloaders rebuilt

### `0.5.2` RTC

- Fixed RTC time set, `RTC_SetDatetime` reports a refused write

### `0.5.1` BLE

- **BLE** advertises the service UUID
- Fixed Modbus slave coil writes
- CPU2 stack reinstalled on every run
- `TASK_Add` reports a refused task

### `0.5.0` USB

- **USB** device stack with a CDC virtual COM port on STM32G0 and STM32WB
- **BLE** and WPAN radio on STM32WB, with CPU2 firmware and a flashing script
- **Bootloader** with a staging slot and CRC-checked images
- Board layer `brd` and drivers `dvr` moved to the top level
- New drivers: ISM330, GNSS, SHT4x and RGB

### `0.4.3` Host

- Minimal HAL wrappers for the host backend
- Device drivers moved to `plc/dvr`

### `0.4.2` Flash

- Fixed STM32 flash verification

### `0.4.1` Fixes

- Fixed host CRC, cooperative VRTS and RTC alarms
- Fixed heap alignment, PDB iteration and buffer overflow recovery
- Fixed Modbus writes, TWI validation, UART parity and PWM frequency

### `0.4.0` I/O

- New GPIF/DIN input filtering, better AIN/ADC sampling, runtime PWM control
- EEPROM and PDB on emulated NOR flash in the host
- Fixes across RTC, I2C, Modbus, LPUART, ADC, flash, heap and 1-Wire

### `0.3.2` BOR

- Added brown-out reset _(BOR)_ support
- RTC keeps time across resets; backup domain resets only on power-on

### `0.3.1` Refactor

- Project-wide refactor of style and paths

### `0.3.0` Shell

- Refactored shell and `cmd` layer
- Added picoDB `pdb` as lightweight local storage
- Added RTC-based CRON scheduler

### `0.2.1` IWDG

- Fixed **IWDG** register and function naming
- Improved readme readability

### `0.2.0` Rewrite

- Full rewrite around a multiplatform, multi-family directory structure
- Targets **STM32G0** and **STM32WB**, building on Windows and Linux

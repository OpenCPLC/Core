# Changes

Release history of **OpenCPLC Core**, newest first.

### `0.6.0` 2026-10-12

- **Tests** on computer with `make test`, chip flash modeled in `hal/host`
- **EEPROM** and **PDB** survive power cuts, moved to `lib/sys`
- `FLASH_Save` records of older Core read as none

### `0.5.6` 2026-10-06

- **Bootloader** `key` variant starts only Ed25519-signed images
- Projects need Forge 0.4.8, standard library moved to `lib/std`

### `0.5.5` 2026-10-01

- ADC errata in driver, buffers sized by `adc_record_scan_max`
- LSE starts at medium-high drive, `RTC_LSE_DRIVE`
- Image header with chip and epoch, room for signature

### `0.5.4` 2026-09-22

- Log levels gate at compile time, `LOG_LEVEL_NIL` silences the log
- UART presets for every standard baud

### `0.5.3` 2026-09-14

- Fixed `FLASH_WritePage` on a blank page, bootloaders rebuilt
- `FLASH_WritePage` in double words on STM32WB, bootloaders rebuilt

### `0.5.2` 2026-09-14

- Fixed RTC time set, `RTC_SetDatetime` reports a refused write

### `0.5.1` 2026-09-11

- **BLE** advertises the service UUID
- Fixed Modbus slave coil writes
- CPU2 stack reinstalled on every run
- `TASK_Add` reports a refused task

### `0.5.0` 2026-09-09

- **USB** device stack with a CDC virtual COM port on STM32G0 and STM32WB
- **BLE** and WPAN radio on STM32WB, with CPU2 firmware and a flashing script
- **Bootloader** with a staging slot and CRC-checked images
- Board layer `brd` and drivers `dvr` moved to the top level
- New drivers: ISM330, GNSS, SHT4x and RGB

### `0.4.3` 2026-08-30

- Minimal HAL wrappers for the host backend
- Device drivers moved to `plc/dvr`

### `0.4.2` 2026-08-29

- Fixed STM32 flash verification

### `0.4.1` 2026-08-25

- Fixed host CRC, cooperative VRTS and RTC alarms
- Fixed heap alignment, PDB iteration and buffer overflow recovery
- Fixed Modbus writes, TWI validation, UART parity and PWM frequency

### `0.4.0` 2026-08-24

- New GPIF/DIN input filtering, better AIN/ADC sampling, runtime PWM control
- EEPROM and PDB on emulated NOR flash in the host
- Fixes across RTC, I2C, Modbus, LPUART, ADC, flash, heap and 1-Wire

### `0.3.2` 2026-06-22

- Added brown-out reset _(BOR)_ support
- RTC keeps time across resets; backup domain resets only on power-on

### `0.3.1` 2026-05-18

- Project-wide refactor of style and paths

### `0.3.0` 2026-04-18

- Refactored shell and `cmd` layer
- Added picoDB `pdb` as lightweight local storage
- Added RTC-based CRON scheduler

### `0.2.1` 2026-03-22

- Fixed **IWDG** register and function naming
- Improved readme readability

### `0.2.0` 2026-03-19

- Full rewrite around a multiplatform, multi-family directory structure
- Targets **STM32G0** and **STM32WB**, building on Windows and Linux

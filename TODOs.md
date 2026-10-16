# TODOs

Snapshot of the codebase and docs review on branch `feat/mcu/usb` (2026-10-14).
Host tests `gamegirl-input-tests` and `gamegirl-timer-tests` pass.

## Firmware bugs

- [x] **`mcu/src/drivers/usb.c` does not compile.** `init_usb()` calls `configi_FIFO_regs()`, which is undefined (and misspelled), and the static `clear_rx_fifo_reg()` is unused, which `-Werror` rejects. This goes unnoticed because `usb.c` is not listed in `mcu/CMakeLists.txt`; add it there.
- [x] ~~**`mcu/include/timer.h` declares both `timer_init()` and `init_timer()`**, but only `init_timer()` is defined. Keep one name; `docs/mcu/timer-layers.md` uses `timer_init()`.~~
- [x] ~~**The `timer_t` name collides with POSIX/newlib.** Both define `timer_t` in `<sys/types.h>`, so any translation unit including it alongside `timer_api.h` will fail. Rename it (e.g. `deadline_t`).~~
- [x] ~~**`configure_tim2()` in `mcu/src/drivers/timer.c` is non-static** with no prototype; make it `static`.~~

## USB bring-up

- [ ] Set `PWR_SVMCR.USV` (and enable the PWR clock first). On the U5, the USB supply stays isolated until this bit is set.
- [ ] Configure PA11/PA12 (USB D-/D+) for the alternate function.
- [ ] Do the core soft reset (`CSRST`) before forcing device mode (`FDMOD`), matching ST's usual init order.
- [ ] Replace the 25 ms spin in `setup_OTG_FS_mode()` with a non-blocking USB state machine, as `docs/mcu/timer-layers.md` requires, or document it as an accepted boot-time exception.
- [ ] Implement FIFO sizing (`GRXFSIZ`, TX FIFOs), device configuration, enumeration and descriptors.
- [ ] Use `hid_report_t` (`mcu/include/hid.h`): convert `input_state_t.current` into a report, add the HID report descriptor, and unit-test report packing for every button.

## Clock tree

- [ ] Add a clock module (`clock_init()`). USB full-speed needs HCLK of roughly 14 MHz or more, and the chip boots at 4 MHz from MSIS.
- [ ] When the system clock changes, update `TIM2_INPUT_CLOCK_HZ` in `timer.c`, or derive it from the clock module.

## Build infrastructure

- [ ] Add an ARM cross-compilation toolchain file (`cmake/arm-none-eabi-toolchain.cmake`).
- [ ] Add startup code (vector table, `Reset_Handler`) and a linker script.
- [ ] Produce ELF, BIN and map files instead of the current object-only `gamegirl-firmware-objects` target. `GAMEGIRL_BUILD_MCU` is `OFF` by default.
- [ ] Add OpenOCD `flash`, `reset` and `erase` targets.
- [ ] Add `.cache/` (clangd) to `.gitignore`.

## Docs out of date

- [ ] `docs/mcu/drivers/timer.md` still describes the old once-per-millisecond update-flag design (`PSC=3`, `ARR=999`, `timer_event_elapsed()`) and a clock-enable bug that is already fixed. Rewrite it or replace it with `docs/mcu/timer-layers.md`.
- [ ] `docs/mcu/drivers/gpio.md` tells callers to use `enable_GPIO_clocks()` and `set_GPIO_pins()`, which are now static. The public entry point is `init_GPIO()`.
- [ ] `docs/mcu/drivers/boardless-tests.md` does not mention `gamegirl-timer-tests`.
- [ ] The spec (`docs/Game_GIRL_Pi4_STM32_Cahier_des_charges(1).md`) and `docs/sprints/non_hardware_firmware.md` include a **Menu** button and use a different bit order from `btn_t` in `mcu/include/inputs.h` (12 buttons, no Menu). Decide which is authoritative, then add Menu or update the docs.
- [ ] `timer_architecture(1).md` at the repo root looks like a draft of `docs/mcu/timer-layers.md`. Merge it or delete it.
- [ ] `docs/file_tree.md` and the spec's layout show `mcu/drivers/` and `mcu/services/` at the top level; the code uses `mcu/src/drivers/`, `mcu/src/services/` and `mcu/src/common/`. Align one with the other.

## Remaining sprint work (`docs/sprints/non_hardware_firmware.md`)

- [ ] USB status protocol: command IDs, request/response and `gamegirl_status_t` packets, parsing and serialization, plus tests for valid, malformed, truncated and unknown commands.
- [ ] Battery telemetry: MAX17043 register conversions (VCELL to mV, SOC to %), low and critical battery states, and tests.
- [ ] Power/shutdown state machine: power-button hold detection and normal / shutdown-requested / waiting-for-host / complete states, with tests.
- [ ] LED and rumble policies: abstract states and requests, with tests.

## Hardware checks (when the board is available)

- [ ] Each button pulls its documented pin low; the pull-ups read high when released.
- [ ] TIM2 really runs at the assumed input clock, giving a 1 ms tick.
- [ ] USB enumeration and HID recognition by Linux.
- [ ] MAX17043: I²C pull-ups at 3.3 V and battery JST polarity checked before connecting (see `docs/Game_GIRL_MAX17043_wiring_notes.md`).

## Not yet started

- [ ] Emulator (`emulator/`) and Pi launcher (`pi/launcher/`) are empty stubs.

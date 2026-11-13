# TODOs

Snapshot of the codebase and docs review on branch `feat/mcu/usb` (2026-10-14).
Host tests `gamegirl-input-tests` and `gamegirl-timer-tests` pass.

## Firmware bugs

- [x] **`mcu/src/drivers/usb.c` does not compile.** `init_usb()` calls `configi_FIFO_regs()`, which is undefined (and misspelled), and the static `clear_rx_fifo_reg()` is unused, which `-Werror` rejects. This goes unnoticed because `usb.c` is not listed in `mcu/CMakeLists.txt`; add it there.
- [x] ~~**`mcu/include/timer.h` declares both `timer_init()` and `init_timer()`**, but only `init_timer()` is defined. Keep one name; `docs/mcu/timer-layers.md` uses `timer_init()`.~~
- [x] ~~**The `timer_t` name collides with POSIX/newlib.** Both define `timer_t` in `<sys/types.h>`, so any translation unit including it alongside `timer_api.h` will fail. Rename it (e.g. `deadline_t`).~~
- [x] ~~**`configure_tim2()` in `mcu/src/drivers/timer.c` is non-static** with no prototype; make it `static`.~~
- [x] ~~**`GPIOE_BASE_NS` was `0x04202100`**; RM0456 puts GPIOE at `0x42021000`. Port-E button reads would have hit the wrong address on the board. Every other base address in `stm32u575_regs.h` was checked against the memory map.~~
- [x] ~~**`timer.c` had a stray `RCC_ICSCR1 |= 0;`** in `enable_hardware_timer_clock()`.~~

## USB bring-up

- [x] Set `PWR_SVMCR.USV` (and enable the PWR clock first). On the U5, the USB supply stays isolated until this bit is set.
- [x] Configure PA11/PA12 (USB D-/D+) for the alternate function.
- [x] Do the core soft reset (`CSRST`) before forcing device mode (`FDMOD`), matching ST's usual init order.
- [x] The 25 ms mode-switch wait (and the 5 ms VDDUSB monitor wait) in `init_usb()` is an **accepted start-up exception**, documented in `docs/mcu/timer-layers.md` step 6. One `usb_task()` call in `STARTING` blocks about 30 ms, at most about 40 ms (a failing step adds one 10 ms timeout), at most 3 times. Revisit with a split `init_usb()` if background retry or a task needing < 30 ms response is added.
- [x] VBUS override (`GCCFG.VBDEN = 0`, `GOTGCTL.BVALOEN | BVALOVAL`), `SDIS`, `PCGCCTL` and `DCFG` in `init_usb_device()`. On the Nucleo PA9 is the ST-LINK UART, not VBUS (UM2861 Tables 10, 13).
- [x] Implement FIFO sizing (`GRXFSIZ`, TX FIFOs), the rest of the device configuration, enumeration and descriptors. (All of `usb.c` §4 is written and builds; untested until the board, see `docs/todos/usb_TODO.md` §5–§6.)
- [x] Use `hid_report_t` (`mcu/include/hid.h`): convert `input_state_t.current` into a report, add the HID report descriptor, and unit-test report packing for every button. (Done in `hid.c` and `test_hid.c`; sending the report from `usb.c` is still open in `docs/todos/usb_TODO.md` §4.)

## Clock tree

- [x] Add a clock module. `init_hardware_clock()` (`mcu/src/drivers/hardware_clock.c`) raises VOS to range 3 (USB doesn't work in range 4) and MSIS to 24 MHz, with 0 flash/SRAM wait states. Details in `docs/mcu/drivers/usb.md` §2.
- [x] Derive the TIM2 prescaler from the clock module: `timer.c` uses `hardware_clock_hz()`, so the 1 ms tick holds at 24 MHz and at 4 MHz if the clock init fails.

## Build infrastructure

- [ ] Add an ARM cross-compilation toolchain file (`cmake/arm-none-eabi-toolchain.cmake`).
- [ ] Add startup code (vector table, `Reset_Handler`) and a linker script.
- [ ] Produce ELF, BIN and map files instead of the current object-only `gamegirl-firmware-objects` target. `GAMEGIRL_BUILD_MCU` is `OFF` by default.
- [ ] Add OpenOCD `flash`, `reset` and `erase` targets.
- [ ] Add `.cache/` (clangd) to `.gitignore`.

## Docs out of date

- [ ] `docs/mcu/drivers/timer.md` still describes the old once-per-millisecond update-flag design (`PSC=3`, `ARR=999`, `timer_event_elapsed()`) and a clock-enable bug that is already fixed. It also assumes a 4 MHz TIM2 clock; the prescaler now comes from `hardware_clock_hz()` (24 MHz). Rewrite it or replace it with `docs/mcu/timer-layers.md`.
- [ ] No doc for the clock module yet beyond `docs/mcu/drivers/usb.md` §2. Add `docs/mcu/drivers/hardware_clock.md` or point to that section.
- [ ] `docs/mcu/drivers/gpio.md` tells callers to use `enable_GPIO_clocks()` and `set_GPIO_pins()`, which are now static. The public entry point is `init_GPIO()`.
- [ ] `docs/mcu/drivers/boardless-tests.md` does not mention `gamegirl-timer-tests`.
- [ ] The spec (`docs/Game_GIRL_Pi4_STM32_Cahier_des_charges(1).md`) and `docs/sprints/non_hardware_firmware.md` include a **Menu** button and use a different bit order from `btn_t` in `mcu/include/inputs.h` (12 buttons, no Menu). Decision for the USB gamepad (`docs/todos/usb_TODO.md`): no Menu button, DS-style set as in `btn_t`. Update the spec and sprint doc to match.
- [ ] `timer_architecture(1).md` at the repo root looks like a draft of `docs/mcu/timer-layers.md`. Merge it or delete it.
- [ ] `docs/file_tree.md` and the spec's layout show `mcu/drivers/` and `mcu/services/` at the top level; the code uses `mcu/src/drivers/`, `mcu/src/services/` and `mcu/src/common/`. Align one with the other.

## Remaining sprint work (`docs/sprints/non_hardware_firmware.md`)

- [ ] USB status protocol: command IDs, request/response and `gamegirl_status_t` packets, parsing and serialization, plus tests for valid, malformed, truncated and unknown commands.
- [ ] Battery telemetry: MAX17043 register conversions (VCELL to mV, SOC to %), low and critical battery states, and tests.
- [ ] Power/shutdown state machine: power-button hold detection and normal / shutdown-requested / waiting-for-host / complete states, with tests.
- [ ] LED and rumble policies: abstract states and requests, with tests.

## Hardware checks (when the board is available)

- [ ] Each button pulls its documented pin low; the pull-ups read high when released.
- [ ] `init_hardware_clock()` returns `FAULT_NONE` (VOSRDY and MSISRDY set), and TIM2 really ticks every 1 ms at 24 MHz.
- [ ] Port-E buttons read correctly now that `GPIOE_BASE_NS` is fixed.
- [ ] USB enumeration and HID recognition by Linux.
- [ ] Nucleo USB power and VBUS: `init_usb()` passes `VDDUSBRDY` (no `0x27`) and `BSVLD` (no `0x26`), and D+ goes high when `SDIS` clears (the Pi logs a new full-speed device in `dmesg`). If nothing appears, check `OTG_GOTGCTL.BSVLD` and `OTG_GCCFG` in the debugger first.
- [ ] `usb_task()` failure path: force an `init_usb()` failure (e.g. skip `RCC_CR.HSI48ON`, which times out as `FAULT_USB_HSI48`), check 3 attempts about 100 ms apart, the USB fault count reaches 3, the task ends in `FAILED`, and the buttons are still sampled.
- [ ] MAX17043: I²C pull-ups at 3.3 V and battery JST polarity checked before connecting (see `docs/Game_GIRL_MAX17043_wiring_notes.md`).

## Not yet started

- [ ] Emulator (`emulator/`) and Pi launcher (`pi/launcher/`) are empty stubs.

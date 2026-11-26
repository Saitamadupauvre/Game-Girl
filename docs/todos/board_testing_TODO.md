# Board testing TODO

Everything that host tests can't prove, to check once the boards arrive: first the NUCLEO-U575ZI-Q, then the Game Girl PCB. Each section assumes the ones above it pass, so work top to bottom.

Related lists: [`usb_TODO.md`](usb_TODO.md) §6, [`error_handling_TODO.md`](error_handling_TODO.md) §7, [`TODOs.md`](../../TODOs.md) "Hardware checks", [`boardless-tests.md`](../mcu/drivers/boardless-tests.md) "Hardware Checks Still Required".

**Forcing a fault without changing the code:** in GDB, break on the function and make it return a fault code, e.g. `break init_usb` then `return FAULT_USB_VDDUSB` and `finish`. The rest of the firmware then runs its real fallback path.

**Tools:** ST-LINK + OpenOCD/GDB, a multimeter, a logic analyser or scope if available, and on the Pi: `dmesg -w`, `lsusb -v`, `usbhid-dump`, `hid-decode`, `evtest`.

## 0. Before the board: build and flash

These are not tests, but nothing below can run without them (`TODOs.md` "Build infrastructure").

- [ ] ARM toolchain file, so `GAMEGIRL_BUILD_MCU=ON` uses `arm-none-eabi-gcc`. Then the full firmware build with `-Werror` (last open criterion on #38 and #39).
- [ ] Startup code (vector table, `Reset_Handler`) and a linker script.
- [ ] ELF/BIN output and OpenOCD `flash`, `reset`, `erase` targets.

## 1. First boot and debugger

- [ ] Flash, then halt in GDB at `main()`. The vector table and `Reset_Handler` work.
- [ ] Step through `init_system()` without a HardFault.
- [ ] Read `last_fault` and `active_fault[]` in `fault.c` with the debugger: all `FAULT_NONE` after a normal boot.

## 2. Clock (`hardware_clock.c`)

- [ ] `init_hardware_clock()` returns `FAULT_NONE`: `PWR_VOSR.VOSRDY` and `RCC_CR.MSISRDY` are set, `RCC_ICSCR1.MSISRANGE = 1`.
- [ ] `hardware_clock_hz()` returns 24 MHz.
- [ ] Forced fault (`return FAULT_CLOCK_VOS`): the chip stays at 4 MHz, `active_fault[CLOCK]` holds the code, `usb_task()` never calls `init_usb()`, buttons still work.

## 3. Timer (`timer.c`, TIM2)

- [ ] TIM2 prescaler is 23 999 at 24 MHz, so the tick is 1 ms: toggle a pin every 500 ms and measure it with the scope, or compare `time_now_ms()` against a stopwatch over 60 s.
- [ ] Same at 4 MHz (forced clock fault): prescaler 3 999, tick still 1 ms.

## 4. Buttons (`gpio.c`, `inputs.c`)

- [ ] GPIOE and GPIOF clocks are on (`RCC_AHB2ENR1`).
- [ ] Every button is released → its pin reads high (internal pull-up). Pressed → low. Check all 12 against the table in `docs/mcu/drivers/gpio.md`: PF3 L, PF4 R, PF5 Start, PF10 Select, PF11 Y, PF13 A, PF14 B, PF15 X, PE7 Up, PE8 Down, PE9 Left, PE11 Right.
- [ ] Port E buttons read correctly (the `GPIOE_BASE_NS` address fix).
- [ ] Each press and release gives exactly one `pressed` / `released` event: no repeats while held, none from contact bounce (logic analyser on a pin to see the real bounce time against the 5-sample debounce).
- [ ] Several buttons held at once all read correctly (D-pad diagonals, A+B).

## 5. USB start-up (`usb.c`, `usb_task.c`)

On the Nucleo, use the user USB connector (check UM2861 for its number), not the ST-LINK one.

- [ ] `init_usb()` returns `FAULT_NONE` on the first attempt.
- [ ] VDDUSB: `PWR_SVMSR.VDDUSBRDY` sets within the 5 ms wait (VDDUSB is tied to 3V3 through SB30).
- [ ] HSI48 starts (`RCC_CR.HSI48RDY`).
- [ ] VBUS override: `GOTGCTL.BSVLD` reads 1 after `BVALOEN | BVALOVAL`.
- [ ] The D+ pull-up is on once `DCTL.SDIS` is cleared: about 3.3 V on D+ with the cable unplugged from the host.
- [ ] Forced faults, one at a time (`return FAULT_USB_HSI48`, `FAULT_USB_VDDUSB`, `FAULT_USB_BSESSION`): 3 attempts 100 ms apart, `hard_reset_usb()` after each, then `FAILED`, `active_fault[USB]` = the code, buttons still read.
- [ ] Forced fault that succeeds on the 2nd attempt: enumerates normally, `active_fault[USB]` = `FAULT_NONE`. Checks that `hard_reset_usb()` (`OTGRST`) really leaves the core usable.

## 6. USB enumeration and HID (Pi)

- [ ] `dmesg -w` shows a new full-speed device and `hid-generic … Gamepad`.
- [ ] `lsusb -v`: interface class 3 (HID), endpoint `0x81` interrupt IN.
- [ ] `usbhid-dump` + `hid-decode`: the report descriptor matches `hid.c`.
- [ ] `evtest`: every button gives the expected code, press and release, and the D-pad gives the right hat values including diagonals.
- [ ] Idle rate: with no button change, reports follow `SET_IDLE` (0 = only on change).
- [ ] Holding buttons and pressing quickly: no missed or stuck reports.
- [ ] Timing: the turnaround (`TRDT = 0x8` at 24 MHz) holds; no transaction errors in `dmesg`.
- [ ] If enumeration is flaky: try CRS to trim HSI48 from SOF (`usb_TODO.md` §6).

## 7. USB runtime recovery

- [ ] Unplug and replug the cable: the device enumerates again (bus reset path, `usb_on_reset()`).
- [ ] Host suspend and resume (`echo auto > /sys/bus/usb/devices/<dev>/power/control` with no program reading the gamepad): `USBSUSP` then `WKUINT`, **no soft replug** during suspend.
- [ ] Soft replug: unbind the HID driver on the Pi while a report is pending (`echo <intf> > /sys/bus/usb/drivers/usbhid/unbind`, then press a button). After about 2 s the device disconnects for 100 ms and the Pi enumerates it again.
- [ ] Soft replug limit: keep the host from recovering (unbind again after each re-enumeration). After 3 replugs: `FAILED`, `active_fault[USB]` = `FAULT_USB_HOST_LOST` (`0x28`), buttons still read.
- [ ] A slow but working host (Pi busy at boot) never triggers a replug.
- [ ] Pi powered off or still booting with the cable in: bus idle, no replug.

## 8. Watchdog and reset cause (once #41 and #42 are done)

- [ ] IWDG runs with a ~500 ms timeout: block the main loop on purpose (GDB, or a test build with a `for (;;)`) and the chip resets.
- [ ] Paused at a breakpoint for longer than the timeout: no reset (DBGMCU freeze bit, or the release-only start).
- [ ] The slowest main-loop pass (a USB start-up attempt, about 30 ms) stays far under the timeout.
- [ ] After a watchdog reset, `RCC_CSR.IWDGRSTF` is read and then cleared with `RMVF`: the next pin reset no longer shows it.

### Reset-loop guard (`backup.c`, `reset_guard.c`)

**Forcing a hang:** in GDB, break on the task, then `jump` to a `b .` loop, or set `$pc` to the current instruction and `continue`. The IWDG fires 500 ms later with the breadcrumb still naming that task.

- [ ] `TAMP_BKPR` writes stick: write a value in GDB, press reset, read it back. If not, check `PWR_DBPR.DBP`, `RTCAPBEN` and `TAMP_SECCFGR`.
- [ ] After a power-on, `TAMP_BKPR(0)` (hang counts) reads 0. Unplugging the Nucleo clears it (VBAT tied to VDD).
- [ ] During normal running, `TAMP_BKPR(2)` (breadcrumb) changes between INPUT (4), USB (2) and NONE (0).
- [ ] Hang in `usb_task()` once: after the reset, the USB nibble of `TAMP_BKPR(0)` is 1 and USB still starts.
- [ ] Hang in `usb_task()` 3 times in a row: USB stays off (no enumeration on the Pi), `active_fault[USB]` = `FAULT_USB_HANG` (`0x29`), buttons still read, and the chip stops rebooting.
- [ ] Hang inside `OTG_FS_IRQHandler`: the breadcrumb says USB, not INPUT, and the USB count goes up.
- [ ] Hang in `input_task()` 3 times: the INPUT count goes up, nothing is disabled, the chip keeps rebooting.
- [ ] After a hang, 10 s of clean running clears the counts of the enabled subsystems. A disabled USB keeps its count and stays off.
- [ ] Pressing the reset button (pin reset) or a power cycle with USB disabled: counts back to 0, USB starts again.

## 9. LEDs and fault display (once #43 and the LED wiring are done)

- [ ] Green LED follows the charger, red LED follows low/critical battery, as in `docs/mcu/services/led.md`.
- [ ] Critical blink: 250 ms period, 125 ms on.
- [ ] Each forced fault (sections 2, 5, 7, 8) shows its blink code, and none clash with the low-battery patterns.
- [ ] The last `fault_t` is readable with the debugger after a reset (backup register).

## 10. Game Girl PCB only

These depend on the real board, not the Nucleo.

- [ ] VBUS: decide whether it reaches PA9. If yes, VBUS sensing can replace the override; if VDDUSB is fed from VBUS, revisit the background retry (dropped in `error_handling_TODO.md` §2).
- [ ] Battery and fuel gauge (MAX17043): polarity and SDA/SCL levels with a multimeter before connecting, as in `docs/Game_GIRL_MAX17043_wiring_notes.md`. Then VCELL and SOC readings against a multimeter.
- [ ] Power latch (`docs/mcu/power-and-usb-fallback.md`): `PWR_LATCH` holds the rail on, and its pull resistor keeps it on while the STM32 is in reset (watchdog or USB-triggered reset must not cut the Pi's power).
- [ ] Clean shutdown: holding the button 2 s pulls `PI_SHUTDOWN_REQ` (Pi GPIO3) low, the Pi raises `PI_HALTED` (GPIO26), then the STM32 releases the latch. Check the 30 s timeout when the Pi doesn't answer.
- [ ] Forced power-off: holding the button 10 s releases the latch at once.
- [ ] Tune the *(v0.1)* values in `power-and-usb-fallback.md` from what the board actually does.

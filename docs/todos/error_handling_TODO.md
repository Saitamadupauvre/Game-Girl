# Error handling and fallbacks TODO

Goal: no single failure stops the Game Girl. A driver that fails is marked as down and retried, the main loop keeps running (buttons, battery, LEDs, power button), and every failure is visible.

Register references are RM0456 Rev 7. Related lists: [`usb_TODO.md`](usb_TODO.md), [`TODOs.md`](../../TODOs.md). Power control and the USB failure policy (escalation ladder, degraded mode): [`power-and-usb-fallback.md`](../mcu/power-and-usb-fallback.md).

## Principles

1. **Detected errors are handled in code.** A timeout returns `false`, and the caller decides.
2. **Retry with a change.** Reset the peripheral between attempts, and wait longer each time. Running the same steps again usually fails the same way.
3. **Degrade, don't stop.** When retries run out, mark the subsystem as failed and keep the main loop running. Never stall in a `for (;;)`.
4. **Hangs are the watchdog's job.** The IWDG resets the chip only when the firmware is stuck and can no longer handle anything itself.
5. **No reset loops.** Count watchdog resets, and boot into degraded mode after too many.
6. **Make it visible.** Use an error code, a blink pattern, and later a status message to the Pi.

## Done

- [x] Every hardware wait in `usb.c` has a timeout (`wait_comp_setup()`, `USB_CORE_TIMEOUT_MS`).
- [x] `init_hardware_clock()` returns `fault_t`. On failure the chip stays at 4 MHz, TIM2 still ticks at 1 ms (prescaler from `hardware_clock_hz()`), and `usb_task()` goes straight to `FAILED` without calling `init_usb()`.
- [x] USB runtime recovery from the host side: a bus reset runs `usb_on_reset()`, which returns to address 0 with EP1 off.
- [x] Superloop of non-blocking tasks. `main()` runs `init_system()` once (clock, backup, GPIO, timer, reset guard, watchdog), then loops `kick_watchdog()` → `reset_guard_task()` → `handle_tasks()` (`input_task()`, then `usb_task()` unless the reset guard disabled USB). Nothing in the loop blocks or retries inline.

## 1. Error reporting (shared by everything below)

- [x] One error-code type, `fault_t` in `mcu/include/fault.h`: `0xSN`, high nibble = subsystem (clock, USB, system), low nibble = step. One code per wait that can time out, so a failure names its step.
- [x] `fault_log()` (`mcu/src/services/fault.c`) keeps the last fault and a saturating count per subsystem. Callers only pass real faults (they check `!= FAULT_NONE` first).
- [x] Active fault per subsystem in `fault.c`: `fault_set_failure(code)` when a module degrades (USB entering `FAILED`, the clock falling back to 4 MHz), `fault_clear_failure(subsystem)` when it recovers (USB entering `RUNNING`), `fault_read_active(subsystem)` to read it (`FAULT_NONE` = healthy). `fault_log()` stays as history (last fault, counts); the active flags are current state. Reporters (blink code, status protocol, backup register) read only `fault.c`, never a module's own state: each module handles its own recovery and its state machine stays private (e.g. `usb_task.c`).
- [x] The `init_*()` functions return `fault_t` instead of `bool`, so the caller learns *which* step failed.

## 2. USB init: retry, then degrade

Closes the `[?]` "how `init_usb()` reports failure" in `usb_TODO.md` §3.

- [x] `init_usb()` returns the `fault_t` of the first step that failed, including `init_usb_device()`.
- [x] Start-up runs from `usb_task()` (`mcu/src/services/usb_task.c`), a state machine: `STARTING` → `RUNNING`, or `BACKOFF` → `STARTING` again, or `FAILED`. Every failed attempt is passed to `fault_log()`.
- [x] Add `RCC_AHB2RSTR1` (RCC + `0x064`, §11.8.22), `RCC_AHB2RSTR1_OTGRST` (bit 14) and `NVIC_ICPR2` (`0xE000E288`, Cortex-M33 core register) to `stm32u575_regs.h`.
- [x] `hard_reset_usb()` in `usb.c`, called by `usb_task()` after every failed attempt, including the last before `FAILED`. It masks the OTG IRQ, sets `SDIS`, pulses `OTGRST` (resets every OTG register, more than `CSRST`), clears the pending OTG IRQ in `ICPR2`, and resets the driver RAM (`ep1_busy`, `report_pending`, `hid_reset()`). The IRQ stays masked until the next successful `init_usb()`; the backoff keeps the pull-up off for at least 100 ms, far longer than the host needs to see the disconnect.
- [x] Up to 3 attempts (`USB_START_ATTEMPTS`), 100 ms apart (`USB_RETRY_BACKOFF_MS`), waiting without blocking.
- [x] ~~Growing wait between attempts~~ **Won't do.** Growing backoff is for many clients competing for one resource; here there is one device, and a failed attempt never turns the pull-up on, so the host sees nothing. Almost every USB fault is deterministic. The one transient cause, a VDDUSB rail still ramping at boot, now has its own check (below), and the fixed 100 ms covers it.
- [x] VDDUSB check before `USV` (RM0456 §10.6.3): `UVMEN`, 5 ms wake-up, `VDDUSBRDY`, monitor off → `FAULT_USB_VDDUSB` (`0x27`). On the Nucleo VDDUSB is tied to 3V3 (UM2861 SB30), so it is a wiring check.
- [x] VBUS override for the Nucleo: `VBDEN = 0`, `GOTGCTL.BVALOEN | BVALOVAL`, wait `BSVLD` → `FAULT_USB_BSESSION` (`0x26`). Without it the core never connects the pull-up, because PA9 is the ST-LINK UART, not VBUS (UM2861 Tables 10, 13). Before this, that failure was silent: `init_usb()` returned `FAULT_NONE` and the host never saw the device.
- [x] After the last attempt: `usb_task()` stays in `FAILED`, never calls `usb_hid_send_if_changed()`, and the main loop carries on.
- [x] ~~Background retry after `FAILED`~~ **Won't do.** Nothing transient can cause `FAILED` on this hardware: VDDUSB is tied to 3V3 (the boot ramp is covered by the monitor wait and the backoff), and VBUS is overridden, so a missing cable never fails `init_usb()`. A retry would repeat a permanent failure and stall the main loop ~30 ms each time. Revisit only if the final PCB powers VDDUSB from VBUS.
- [x] Runtime last resort, if the host stops talking to an enumerated device (#53): `usb_task()` watches host activity (bus reset, SETUP, EP1 report collected) while `usb_waiting_for_host()` says the bus is active and something is pending. After 2 s without progress (`USB_HOST_STALL_MS`) it logs `FAULT_USB_HOST_LOST` and soft replugs: `DCTL.SDIS = 1` for 100 ms (`USB_REPLUG_OFF_MS`), then clear it. After 3 replugs (`USB_REPLUG_LIMIT`) without a clean run, `hard_reset_usb()` and `FAILED`. A suspended bus never counts as a stall.

## 3. Clock init

- [x] No retry: a failed `init_hardware_clock()` leaves the chip at 4 MHz until the next reset. A VOS or MSIS timeout is almost certainly a hardware or supply fault, so a second attempt would fail the same way. `init_system()` logs the fault and marks it active (`fault_set_failure()`). Rationale in `docs/mcu/drivers/usb.md` §2.
- [x] Record which step failed (`FAULT_CLOCK_VOS` / `FAULT_CLOCK_MSIS_NOT_READY` / `FAULT_CLOCK_MSIS_RANGE`), so the blink code can tell "no USB because of the clock" from a USB fault. `usb_task()` logs nothing extra in that case.

## 4. Watchdog (IWDG, RM0456 chapter 61)

Tracked in #41.

- [x] Add the IWDG registers to `stm32u575_regs.h` (base `0x40003000`, `IWDG_KR`, `IWDG_PR`, `IWDG_RLR`, `IWDG_SR`, keys and status bits, see §61.7).
- [x] `init_watchdog()` (`watchdog.c`) is the last call in `init_system()`, after every other `init_*()`. It follows the documented sequence (§61.4, window option disabled): start, unlock, `PR`/`RLR`, wait for `IWDG_SR` to clear with a 10 ms timeout (`FAULT_SYS_WATCHDOG_CONFIG` on timeout), refresh.
- [x] Timeout 500 ms: LSI 32 kHz ÷ 32 (`PR = 3`) = 1 ms per tick, `RLR = 499`. LSI is independent of MSIS and HSI48, so a clock fault can't stop it.
- [x] Kicked in **one place only**: `kick_watchdog()` at the top of the main loop. Never inside an ISR or a wait loop.
- [ ] Check that no main-loop pass can legitimately take longer than the timeout once retries and later drivers are added. On the board: `board_testing_TODO.md` §8.
- [x] Frozen while the core is halted by the debugger: `DBGMCU_APB1LFZR.DBG_IWDG_STOP`.

## 5. Reset cause and reset-loop guard

Tracked in #42. Degraded-mode table in `docs/mcu/power-and-usb-fallback.md`.

- [x] Registers in `stm32u575_regs.h`: `RCC_CSR` reset flags + `RMVF`, `RCC_APB3ENR.RTCAPBEN`, `PWR_DBPR.DBP`, `TAMP_BKPR(x)`.
- [x] Backup driver `backup.c`: `init_backup()` (`PWREN`, `DBP`, `RTCAPBEN`), `reset_flags_read_clear()` (returns `RCC_CSR`, then `RMVF`), bounds-checked `backup_read()`/`backup_write()`, `breadcrumb_set()`/`breadcrumb_fetch()`. Slot map in `backup.h`: `BKP_HANG_COUNTS` 0, `BKP_LAST_FAULT` 1, `BKP_BREADCRUMB` 2.
- [x] Append `FAULT_SUBSYS_INPUT` (0x4), `FAULT_SUBSYS_I2C` (0x5), `FAULT_SUBSYS_POWER` (0x6) to `fault_subsystem_t`, and add `FAULT_USB_HANG` (0x29).
- [x] Breadcrumb in `TAMP_BKPR(2)`: the main loop writes the subsystem before each task and `FAULT_SUBSYS_NONE` at the end. The USB ISR saves it, writes `USB`, and restores it on exit.
- [x] Hang counters in `TAMP_BKPR(0)`: one 4-bit counter per subsystem. Backup registers survive a system reset and clear on power-off, so no `.noinit` section is needed.
- [x] Pure `reset_guard_decide(csr_flags, breadcrumb, counts)` in `reset_guard.c`: an IWDG reset increments the breadcrumb's counter, any other reset clears all counters. A subsystem is disabled at count >= 3 if it is disableable (USB, later I2C). INPUT and POWER never are.
- [x] `init_reset_guard()` at boot, after `init_backup()` and `init_timer()`, before `init_watchdog()`: `reset_flags_read_clear()`, read the counts and the breadcrumb, decide, write the counts back, set the breadcrumb to `NONE`.
- [x] Gate `usb_task()` on `reset_guard_enabled()`. A disabled USB is never initialised, so the core stays in its post-reset state, and `FAULT_USB_HANG` is active.
- [x] After 10 s of clean running (`reset_guard_task()`), clear the counters of the subsystems still enabled. A disabled one stays off until the next non-watchdog reset.

## 6. Making failures visible

Tracked in #43.

- [x] LED registers in `stm32u575_regs.h`: GPIOB/C/G bases, `MODER`/`BSRR`, `GPIO_BSRR_SET`/`RESET`, `PWR_SVMCR.IO2SV`, LED pins (UM2861 §7.5: LD1 green PC7, LD2 blue PB7, LD3 red PG2, active high).
- [x] `fault_log()` also writes `TAMP_BKPR(BKP_LAST_FAULT)`, so the last fault can be read with the debugger after a reset (`x/wx 0x46007D04`). Move `init_backup()` to the top of `init_system()` so a clock fault is saved too. Add a `backup_write()` fake to `test_fault.c` and `test_usb_task.c`.
- [x] `fault_first_active()` in `fault.c`: the first non-`NONE` active fault, in subsystem order (a clock fault also stops USB, so it is the real cause).
- [x] Fault codes on their own LED: `fault_led` (blue, LD2 on the Nucleo) in `led_output_t`, `active_fault` in `led_input_t`. Red and green keep the battery/charger policy, so no clash and no priority rules. Pattern: N flashes (500 ms slots, 200 ms lit), then a 1.5 s pause, stateless from `now_ms`. Codes: 2 = clock fallback (`0x10`–`0x12`), 3 = USB start-up (`0x20`–`0x27`), 4 = host lost (`0x28`), 5 = USB disabled by the reset guard (`0x29`). No active fault or an unknown code = off.
- [x] Host tests in `test_led.c` (N flashes per period per code, off when healthy, red/green unchanged) and `test_fault.c` (`fault_first_active()`, last fault written to the backup slot).
- [x] `led.md`: third LED, blink table, timing.
- [ ] PCB: add a third (fault) LED. Port G pins 2–15 need `IO2SV` and a powered VDDIO2 pin, so prefer other ports for the LEDs.
- [x] LED GPIO driver: `set_GPIO_led_pins()` and `write_GPIO_leds()` in `gpio.c` (PC7/PB7/PG2, `IO2SV` set first), driven by `led_task()` in `led_task.c`, which the main loop runs last.
- [ ] ~~Error counters for the status protocol~~ Moved to #17: `fault_count[]`, the reset guard's hang counts and the replug count already exist; nothing reads them until the protocol does.

## 7. Tests

- [x] Host-test the retry and degrade logic with fake init steps that fail N times (`tests/test_usb_task.c`): the subsystem ends `RUNNING` after a late success and `FAILED` after 3 failures, and the main-loop path keeps running. Also covers the soft replug (stall, suspend, limit, counter reset).
- [x] Host-test the reset guard (`tests/test_reset_guard.c`): the breadcrumb's counter increments on IWDG resets, other resets clear all counters, USB is disabled at 3, INPUT/POWER/NONE never disable anything, counters saturate, and the clean-run clear skips disabled subsystems and runs once.
- [ ] On the board: force each failure (e.g. skip `PWR_SVMCR.USV`, or block in the main loop to trigger the IWDG) and check the blink code and the recovery.

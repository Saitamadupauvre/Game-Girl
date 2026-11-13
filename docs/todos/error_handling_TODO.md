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
- [x] Superloop of non-blocking tasks. `main()` runs `init_system()` once (clock, GPIO, timer), then loops `input_task()` → `usb_task()`. Nothing in the loop blocks or retries inline.

## 1. Error reporting (shared by everything below)

- [x] One error-code type, `fault_t` in `mcu/include/fault.h`: `0xSN`, high nibble = subsystem (clock, USB, system), low nibble = step. One code per wait that can time out, so a failure names its step.
- [x] `fault_log()` (`mcu/src/services/fault.c`) keeps the last fault and a saturating count per subsystem. Callers only pass real faults (they check `!= FAULT_NONE` first).
- [ ] Per-subsystem state (`OK` / `RETRYING` / `FAILED`) readable by the status protocol. USB's state exists but is private to `usb_task.c`; expose a getter when the status protocol needs it.
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
- [ ] Decide on a background retry, e.g. every few seconds, or when VBUS appears (see §7 of `usb_TODO.md`). `usb_task()` is already non-blocking between attempts, but each `init_usb()` call still contains the 25 ms busy-wait (open `TODOs.md` item).
- [ ] Runtime last resort, if the host stops talking to an enumerated device: soft replug, `DCTL.SDIS = 1` for about 10 ms, then clear it. The host sees an unplug and enumerates again, without touching the rest of the firmware.

## 3. Clock init

- [ ] Decide whether a failed `init_hardware_clock()` is retried once, or the 4 MHz fallback is final until the next reset. A VOS or MSIS timeout is almost certainly a hardware or supply fault, so one retry at most.
- [x] Record which step failed (`FAULT_CLOCK_VOS` / `FAULT_CLOCK_MSIS_NOT_READY` / `FAULT_CLOCK_MSIS_RANGE`), so the blink code can tell "no USB because of the clock" from a USB fault. `usb_task()` logs nothing extra in that case.

## 4. Watchdog (IWDG, RM0456 chapter 61)

- [ ] Add the IWDG registers to `stm32u575_regs.h` (base `0x40003000`, `IWDG_KR` at offset 0 and the prescaler/reload registers, see §61.7).
- [ ] `init_watchdog()`: start it after all `init_*()` calls, or give it a timeout longer than the slowest init (25 ms device-mode wait + the timeouts + USB retries).
- [ ] Timeout around 500 ms. It runs from LSI, which is independent of MSIS and HSI48, so a clock fault can't stop it.
- [ ] Kick it in **one place only**: once per main-loop pass. Never inside an ISR or a wait loop, or a stuck loop could keep it happy.
- [ ] Check that no main-loop pass can legitimately take longer than the timeout once retries and later drivers are added.
- [ ] Once started, the IWDG can't be stopped. Check it doesn't fire during debugging: the DBGMCU freeze bit for IWDG, or start it only in release builds.

## 5. Reset cause and reset-loop guard

- [ ] Add `RCC_CSR` (RCC + `0x0F4`, §11.8.50) with its reset flags: `IWDGRSTF` (29), `WWDGRSTF` (30), `SFTRSTF` (28), `BORRSTF` (27), `PINRSTF` (26), and `RMVF` (23) to clear them.
- [ ] At boot, read the reset cause first, then clear the flags with `RMVF`.
- [ ] Keep a watchdog-reset counter in memory that survives a reset but not a power-off: a `TAMP` backup register or a `.noinit` RAM section. This depends on the linker script (`usb_TODO.md` §5).
- [ ] After 3 watchdog resets in a row: boot into degraded mode (USB off), stop rebooting, and report it. Reset the counter after a clean run of, say, 10 s.

## 6. Making failures visible

- [ ] Blink code on the red status LED (`docs/mcu/services/led.md`), e.g. 2 blinks = clock, 3 = USB init, 4 = repeated watchdog resets. Must not clash with the low-battery patterns.
- [ ] Keep the last `fault_t` in a backup register, so it can be read with the debugger after a reset.
- [ ] Error counters (init retries, watchdog resets, soft replugs) for the status protocol to send to the Pi later (spec §7.2, `TODOs.md` "USB status protocol").

## 7. Tests

- [ ] Host-test the retry and degrade logic with fake init steps that fail N times: the subsystem ends `OK` after a late success and `FAILED` after 3 failures, and the main-loop path keeps running.
- [ ] Host-test the reset-loop guard: counter increments on watchdog resets, clears after a clean run, and degraded mode starts at the limit.
- [ ] On the board: force each failure (e.g. skip `PWR_SVMCR.USV`, or block in the main loop to trigger the IWDG) and check the blink code and the recovery.

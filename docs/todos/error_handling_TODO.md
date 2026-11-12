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
- [x] `init_hardware_clock()` returns `bool`. On failure the chip stays at 4 MHz, TIM2 still ticks at 1 ms (prescaler from `hardware_clock_hz()`), and `main()` skips `init_usb()`.
- [x] USB runtime recovery from the host side: a bus reset runs `usb_on_reset()`, which returns to address 0 with EP1 off.

## 1. Error reporting (shared by everything below)

- [ ] Define one error-code type, e.g. `fault_t` in `mcu/include/fault.h`: `FAULT_NONE`, `FAULT_CLOCK_VOS`, `FAULT_CLOCK_MSIS`, `FAULT_USB_HSI48`, `FAULT_USB_CORE_RESET`, `FAULT_USB_DEVICE_MODE`, `FAULT_USB_FIFO_FLUSH`, … One code per wait that can time out, so a failure names its step.
- [ ] Keep the last fault and a per-subsystem state (`OK` / `RETRYING` / `FAILED`) in one place that the main loop and the status protocol can read.
- [ ] Decide whether the `init_*()` functions return `fault_t` instead of `bool`, so the caller learns *which* step failed. `bool` is enough for the retry decision, `fault_t` is better for debugging.

## 2. USB init: retry, then degrade

Closes the `[?]` "how `init_usb()` reports failure" in `usb_TODO.md` §3.

- [ ] `init_usb()` returns the result of its steps, including `init_usb_device()` (it ignores that today).
- [ ] Add `RCC_AHB2RSTR1` (RCC + `0x064`, §11.8.22) and `RCC_AHB2RSTR1_OTGRST` (bit 14) to `stm32u575_regs.h`.
- [ ] Between attempts, pulse `OTGRST` (set, then clear). It resets the whole OTG block, which clears more than the core soft reset (`CSRST`).
- [ ] Up to 3 attempts, with a growing wait between them (e.g. 10, 50, 100 ms).
- [ ] After the last attempt: USB state = `FAILED`, `usb_hid_send_if_changed()` returns straight away, and the main loop carries on.
- [ ] Decide on a background retry, e.g. every few seconds, or when VBUS appears (see §7 of `usb_TODO.md`). This needs `init_usb()` as a non-blocking state machine, which also covers the open `TODOs.md` item about the 25 ms busy-wait.
- [ ] Runtime last resort, if the host stops talking to an enumerated device: soft replug, `DCTL.SDIS = 1` for about 10 ms, then clear it. The host sees an unplug and enumerates again, without touching the rest of the firmware.

## 3. Clock init

- [ ] Decide whether a failed `init_hardware_clock()` is retried once, or the 4 MHz fallback is final until the next reset. A VOS or MSIS timeout is almost certainly a hardware or supply fault, so one retry at most.
- [ ] Record which step failed (`FAULT_CLOCK_VOS` / `FAULT_CLOCK_MSIS`), so the blink code can tell "no USB because of the clock" from a USB fault.

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

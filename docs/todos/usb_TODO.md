# USB TODO

Goal: the STM32U575 enumerates as a **USB HID gamepad** on the Raspberry Pi (spec §7.1), so Linux exposes it as `/dev/input/eventX`. Branch: `feat/mcu/usb`.

Implementation guide: [`docs/mcu/drivers/usb.md`](../mcu/drivers/usb.md). Section numbers below (§n) refer to it.
Reference implementation (compare, don't copy): `mcu/src/drivers/usb_mock.c`.

This replaces `usb_TODOS.md` at the repository root.

## Done

- [x] `stm32u575_regs.h` defines every OTG_FS, PWR, GPIOA, RCC and `NVIC_ISER2` register the guide uses in §3–§9.
- [x] `usb.h` declares `init_usb()`, `usb_hid_send_if_changed()`, `OTG_FS_IRQHandler()` and the `usb_reply_t` reply type.
- [x] `usb.c` is in `mcu/CMakeLists.txt`, and `main.c` calls `init_usb()`.
- [x] `usb.c` turns on HSI48 and the OTG clock, soft-resets the core, then forces device mode with a 25 ms wait (correct order).
- [x] Code layout decided: `drivers/usb.c` (registers) + `services/hid.c` (pure logic) + `tests/test_hid.c`. No `usb/` folder, no `fifo.c`.
- [x] Guide updated for the layout, the reply type and the bugs found in review.

## 0. Housekeeping

- [x] Delete the root `usb_TODOS.md` now that this file replaces it, and tick the reset-order item in `TODOs.md` (it's already correct in `usb.c`).
- [x] `hid.h`: `hid_report_t` was renamed `usb_report_t`. The guide and `usb_mock.c` still say `hid_report_t`, and `usb_mock.c` no longer compiles. Pick one name, and keep the `hid_` prefix if the type lives in `hid.h`.
- [x] `hid.h`: `hid_reply_t` is an empty struct, which isn't valid ISO C (GCC only accepts it as an extension). `usb_reply_t` in `usb.h` already covers the reply, so remove `hid_reply_t` or give it a purpose.
- [x] Remove the commented-out `clear_rx_fifo_reg()` in `usb.c`. The RX flush gets rewritten in §5 with an `AHBIDL` wait.
- [x] Add `NVIC_ICER2` (`0xE000E188`) to `stm32u575_regs.h`, next to `NVIC_ISER2`. It's needed for §9.

## 1. Clock (§2): blocks USB on the board

- [ ] Add a clock module with `clock_init()`: MSIS range 1 → 24 MHz, flash wait states raised **before** the clock goes up. (Nothing exists yet: `clock_init()` is only referenced in `usb_mock.c`.)
- [ ] Add the `FLASH_ACR`, `RCC_ICSCR1` and `PWR_VOSR` definitions to `stm32u575_regs.h`.
- [ ] Check in RM0456 whether USB works in VOS range 4. If not, raise VOS first.
- [ ] Change `TIM2_INPUT_CLOCK_HZ` to 24 MHz **in the same change** (prescaler 23 999). Otherwise every USB timeout expires 6× too early.
- [ ] Call `clock_init()` before `init_timer()` in `main.c`.

## 2. `hid.c`: pure logic, testable now without the board

- [ ] Create `mcu/src/services/hid.c` and add it to `mcu/CMakeLists.txt`. (`hid.c` exists as an untracked 3-line stub with only includes, and isn't in `mcu/CMakeLists.txt` yet.)
- [ ] Define `hid_state_t` (address, configuration, idle rate, last report) and `hid_reset()` (§7.1).
- [ ] Descriptors (§8): device, 34-byte configuration block, strings, 29-byte HID report descriptor. Choose the VID/PID (`0x1209` pid.codes test PID suggested).
- [ ] `hid_pack()` (§9).
- [ ] `hid_handle_setup()` returning `usb_reply_t`, already clipped to `wLength` (§7.1).
- [ ] Create `mcu/tests/test_hid.c` and a `gamegirl-hid-tests` target in `mcu/tests/CMakeLists.txt`, modelled on `gamegirl-input-tests`. Cover the test list in §9.
- [ ] Settle the button questions before freezing the report descriptor (§8.5): the Menu button (spec has it, `btn_t` doesn't) and the button order / D-pad as buttons vs hat.

## 3. `usb.c`: start-up (§3–§5), compile-check only

- [x] Add timeouts (`deadline_t`) to the `HSI48RDY`, `AHBIDL` and `CSRST` waits.
- [x] Check `GINTSTS.CMOD == 0` after the 25 ms device-mode wait (§4).
- [x] Power: `RCC_AHB3ENR.PWREN`, then `PWR_SVMCR.USV` (§3).
- [x] Pins: PA11/PA12 alternate function, AF10, very-high speed (§3).
- [x] Kernel clock: `RCC_CCIPR1.ICLKSEL = HSI48` (§3).
- [x] `GUSBCFG.PHYSEL` before the soft reset, and `GCCFG.PWRDWN` after it (§4).
- [x] Device setup (§5), first part in `init_usb_device()`: `GCCFG.VBDEN` (VBUS sensing on PA9, so no `BVALOEN`/`BVALOVAL` override), `SDIS`, `PCGCCTL`, `DCFG` (full speed, address 0).
- [x] Device setup (§5), rest: FIFO sizes (`GRXFSIZ`, `DIEPTXF0`, `DIEPTXF1`), TX and RX flushes with `AHBIDL` waits, endpoint clear loop, interrupt masks, `GAHBCFG`, NVIC, clear `SDIS` last.
- [x] FIFO layout decided (§5): RX 64, TX0 32, TX1 16 words, at RX 0–63, TX0 64–95, TX1 96–111 (112 of 320 words). Use named constants and compute each start address from the previous start + depth. `usb_mock.c` still uses 128 / 64 / 64.
- [?] Check in RM0456 before relying on those sizes: the minimum depth for `DIEPTXFx` (TX1 = 16 assumes it is ≤ 16). RX must stay ≥ 28 words (16 + 2 + 10), because SETUP packets can't be NAKed. (Confirmed in RM0456: the 320-word total, from the dedicated 1.25 KB RAM in the features list, and 6 device endpoints including EP0 in Table 749. Use 6 for the endpoint loop count.)
- [?] Decide how `init_usb()` reports failure (it returns `void` today, so a timeout is silent). `init_usb_device()` returns `bool` but `init_usb()` ignores it; check it once the flushes can time out, or make it `void`.

## 4. `usb.c`: interrupts and transfers (§6, §7.2, §9)

- [ ] `txfifo_write()`, `rxfifo_read()`, and the TX/RX flush helpers as `static` functions (§6.6).
- [ ] `OTG_FS_IRQHandler` dispatching to the event handlers (§6).
- [ ] USB reset handler, with `DAINTMSK =` (not `|=`) and `hid_reset()` (§6.1).
- [ ] Speed-negotiated handler: EP0 64 bytes, `CGINAK`, `TRDT = 0x8` for 24 MHz (§6.2).
- [ ] RX FIFO handler: SETUP and OUT data (§6.3).
- [ ] OUT endpoint handler: re-arm EP0 **before** `apply_setup()` (§6.4).
- [ ] IN endpoint handler: clear flags, `ep1_busy = false` on EP1 `XFRC` (§6.5).
- [ ] EP0 helpers: `ep0_send`, `ep0_send_zlp`, `ep0_stall`, `ep0_arm_setup`, `ep0_arm_status_out` (§6.6).
- [ ] `apply_setup()`, `ep1_activate()` (with the TX FIFO 1 flush) and `ep1_deactivate()` (§7.2).
- [ ] `usb_hid_send_if_changed()` with the idle-rate, `report_pending` and IRQ-masking fixes (§9).
- [ ] Call `usb_hid_send_if_changed(&inputs)` in the main loop in `main.c`.

## 5. Build and boot infrastructure: blocks flashing

- [ ] Startup code (vector table with `OTG_FS_IRQHandler` at IRQ 73, `Reset_Handler`) and a linker script.
- [ ] ELF and BIN outputs, plus OpenOCD flash, reset and erase targets.
- [x] Build with `GAMEGIRL_BUILD_MCU=ON` and `-Werror`, and fix whatever shows up. (`cmake --build build-mcu` is clean for the current sources; re-run it as `hid.c`, the clock module and the ISR land.)

## 6. Bring-up on the board (§10)

- [ ] The connector's VBUS net must reach PA9 (`OTG_FS_VBUS`). `usb.c` relies on `VBDEN` sensing with no software Bvalid override, so without that wiring the core never connects. Check the schematic, or fall back to the `BVALOEN`/`BVALOVAL` override as in `usb_mock.c`.
- [x] Leave PA9 unconfigured in `enable_usb_pins()` so the VBUS function stays active. (True today: it only touches PA11 and PA12. Keep it that way.)
- [ ] `dmesg -w` on the Pi shows a new full-speed device and `hid-generic … Gamepad`.
- [ ] `lsusb -v` shows interface class 3 and EP `0x81` interrupt.
- [ ] `usbhid-dump` + `hid-decode` match the report descriptor.
- [ ] `evtest`: every button gives the expected key code.
- [ ] Unplug/replug, and host suspend/resume.
- [ ] Confirm the 1 ms tick still holds at 24 MHz.
- [ ] If enumeration is flaky: enable CRS to trim HSI48 from USB SOF (§3).

## 7. Later (optional)

- [ ] Handle VBUS plug/unplug: unmask `SRQINT` and `OTGINT` in `GINTMSK`, and on unplug reset `hid_reset()` state and disconnect (`SDIS`). Not needed for first enumeration, since `VBDEN` already gates the pull-up on Bvalid.
- [ ] Vendor control channel for the status protocol (spec §7.2: battery, charger, rumble), probably as `SET_REPORT` / vendor requests on EP0 rather than a new endpoint.

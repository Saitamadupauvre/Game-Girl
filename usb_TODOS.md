# USB TODOs

Goal: the STM32U575 enumerates as a **USB HID gamepad** on the Raspberry Pi (spec §7.1), so Linux exposes it as `/dev/input/eventX`. Branch: `feat/mcu/usb`.

## Done so far

- [x] OTG_FS register definitions started in `mcu/include/stm32u575_regs.h`: base address, `GUSBCFG` (`FDMOD`, `FHMOD`), `GRSTCTL` (`AHBIDL`, `CSRST`, `TXFFLSH`, `RXFFLSH`, `TXFNUM`), `GRXFSIZ`, `RCC_AHB2ENR1_OTGEN`, `RCC_CR_HSI48ON/RDY`.
- [x] `mcu/include/usb.h` exposes `init_usb()`. `main.c` calls it.
- [x] `mcu/src/drivers/usb.c` enables the HSI48 clock and the OTG_FS clock.
- [x] `usb.c` forces device mode (clears `FHMOD`, sets `FDMOD`) and waits 25 ms using the `deadline_t` timer (`USB_WAIT_PERIOD_MS`).
- [x] `usb.c` does the core soft reset (idle wait on `AHBIDL`, set `CSRST`, wait for it to clear).
- [x] `mcu/include/hid.h` has a `hid_report_t` placeholder (`btn_low`, `btn_high`) matching the 12 buttons in `btn_t`.
- [x] Timer layer (`timer_api.h`, `timer.c`) exists to support USB timeouts, and its host tests pass.

## 1. Make it build (blocking)

- [x] Add `src/drivers/usb.c` to `mcu/CMakeLists.txt`. `main.c` already calls `init_usb()`, so the firmware currently has an unresolved symbol.
- [ ] Re-enable or delete the commented-out `clear_rx_fifo_reg()` and `config_FIFO_regs()` calls. With `-Werror`, an unused static function fails the build, so either call it or remove it.
- [ ] Build with `GAMEGIRL_BUILD_MCU=ON` and fix whatever else shows up, for example the missing `#include "timer_api.h"` / `USB_WAIT_PERIOD_MS` use in `usb.c`.

## 2. Power, pins and clocks

- [ ] Enable the PWR clock, then set `PWR_SVMCR.USV`. The USB supply stays isolated until this bit is set (RM0456, PWR chapter).
- [ ] Configure PA11 (USB D-) and PA12 (USB D+) for the alternate function (AF10). Add the `GPIOA_MODER`/`AFRH` definitions and put the setup in `gpio.c` or `usb.c`.
- [ ] Select HSI48 as the 48 MHz USB kernel clock (`RCC_CCIPR1.ICLKSEL`). Enabling HSI48 alone is not enough.
- [ ] Add a clock module (`clock_init()`). The chip boots at 4 MHz from MSIS, and USB full-speed needs HCLK of roughly 14 MHz or more. Update `TIM2_INPUT_CLOCK_HZ` in `timer.c` when the system clock changes.

## 3. Core initialisation order

- [ ] Do the core soft reset (`CSRST`) **before** forcing device mode (`FDMOD`). Today `init_usb()` forces the mode first and resets afterwards, and the reset can undo the mode bits. The order should be: clocks, `CSRST`, set `FDMOD`, wait 25 ms.
- [ ] Add a timeout to the `AHBIDL` and `CSRST` wait loops so a dead core cannot hang boot.
- [ ] Decide what to do about the 25 ms blocking wait in `setup_OTG_FS_mode()`. Either replace it with a non-blocking USB state machine (as `docs/mcu/timer-layers.md` requires) or document it as an accepted boot-time exception.
- [ ] Select the full-speed embedded PHY (`GUSBCFG.PHYSEL`) and set the turnaround time (`TRDT`) for the chosen HCLK.
- [ ] Disable VBUS sensing, or enable it if VBUS is wired to a sense pin (`GCCFG`: `VBDEN`, `NOVBUSSENS`). Decide this together with the board wiring.

## 4. Device mode setup

- [ ] Set the device configuration (`DCFG`): full-speed device speed, address 0, periodic frame interval.
- [ ] Size the FIFOs: `GRXFSIZ` (RX), `DIEPTXF0` (EP0 TX) and `DIEPTXF1` (EP1 TX for the HID IN endpoint). The total must fit the 1.25 KB FIFO RAM.
- [ ] Flush the TX and RX FIFOs after sizing (`TXFFLSH` with `TXFNUM`, `RXFFLSH`).
- [ ] Unmask the core and device interrupts (`GINTMSK`: `USBRST`, `ENUMDNE`, `RXFLVL`, `IEPINT`, `OEPINT`, `USBSUSP`) and enable the global interrupt (`GAHBCFG.GINTMSK`).
- [ ] Add the `OTG_FS` handler to the vector table. This depends on the startup code (see §8).
- [ ] Clear soft-disconnect (`DCTL.SDIS`) to connect to the host once everything is ready.

## 5. Enumeration (EP0)

- [ ] Handle the USB reset and enumeration-done interrupts, and read the negotiated speed.
- [ ] Set up EP0 IN and OUT (max packet size 64) and receive SETUP packets from the RX FIFO.
- [ ] Implement the standard requests: `GET_DESCRIPTOR`, `SET_ADDRESS`, `SET_CONFIGURATION`, `GET_STATUS`, `SET_INTERFACE`.
- [ ] Add the descriptors: device (choose VID/PID), configuration, interface (HID class), HID descriptor, endpoint (interrupt IN, EP1), plus string descriptors.
- [ ] Implement the HID class requests: `GET_DESCRIPTOR(HID report)`, `SET_IDLE`, `GET_REPORT`, `SET_PROTOCOL` as needed.

## 6. HID gamepad report

- [ ] Write the HID report descriptor: 12 buttons (matching `btn_t` in `mcu/include/inputs.h`), 4 padding bits, no axes unless added later. Settle the layout in `docs/sprints/non_hardware_firmware.md` ("Define the USB HID gamepad report layout").
- [ ] Decide the Menu button question (the spec has one, `btn_t` has none) before freezing the report layout.
- [ ] Implement `input_state_t.current` to `hid_report_t` conversion in a pure function with no hardware access.
- [ ] Unit-test report packing for every button, plus none pressed and all pressed. Add the test to `mcu/tests/CMakeLists.txt`.
- [ ] Send the report on the interrupt IN endpoint from the main loop only when the state changes (or at the polling interval). Use a `deadline_t` for the poll rate rather than blocking.

## 7. Bring-up and verification (needs the board)

- [ ] Flash and confirm `lsusb` on the Pi shows the device (`dmesg` for enumeration errors).
- [ ] Confirm Linux recognises it as a gamepad (`/dev/input/eventX`, `evtest`, `jstest`).
- [ ] Check each button press shows up on the right bit.
- [ ] Check unplug and replug, and host suspend and resume.
- [ ] Confirm the 1 ms tick still holds after the clock change (shared with the hardware checks in `TODOs.md`).

## 8. Dependencies outside USB

These come from the build infrastructure section of `TODOs.md` and block flashing:

- [ ] ARM toolchain file (`cmake/arm-none-eabi-toolchain.cmake`).
- [ ] Startup code (vector table, `Reset_Handler`) and a linker script.
- [ ] ELF and BIN outputs, plus OpenOCD flash, reset and erase targets.

## 9. Later (optional)

- [ ] Vendor-specific control channel for the status protocol (spec §7.2: battery, charger state, rumble). See "USB status protocol" in `docs/sprints/non_hardware_firmware.md`.
- [ ] Docs: add `docs/mcu/drivers/usb.md` describing the init sequence and the endpoint layout.

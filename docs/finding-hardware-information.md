# Finding Hardware Information Without an LLM

## Purpose

Bringing up a peripheral such as USB means knowing more than which bits exist. You also need the *order* to set them in and *why* each one matters, and the reference manual's register pages rarely say. This document lists where that information lives and the workflow engineers use to find it.

It was written alongside `docs/mcu/drivers/usb.md`. The USB examples refer to that file.

---

## 1. Read the reference manual beyond its register pages

RM0456 contains much of the "why", but in prose sections, not in the register descriptions. Start from a chapter's introduction and its programming model, not the register tables.

| Look in | What it tells you |
|---|---|
| OTG_FS chapter, **programming model** (core initialization, device initialization) | The required order: PHYSEL, reset, mode, FIFOs, connect |
| PWR chapter, **independent supply / VDDUSB** section | Why `USV` exists and what the isolation protects |
| RCC chapter, **clock tree diagram** | Following USB back through the multiplexers shows why you need both `ICLKSEL` and `OTGEN` |
| GPIO chapter, **register reset values** | A `MODER` reset value of `0xFFFF…` means pins start in analog mode |

> These section names were not checked against the PDF (ST blocks scripted downloads), so search the manual for the terms rather than expecting exact headings.

---

## 2. Vendor example code

This is the most common real-world method: get a working example running, then study its sequence.

- **STM32CubeU5**: look for the USB Device HID example for the NUCLEO-U575ZI-Q board.
- **U5 HAL / LL source**, e.g. `stm32u5xx_ll_usb.c` and `stm32u5xx_hal_pcd.c`. The HAL is effectively ST's executable documentation. Some details, such as the USB turnaround-time table in `USB_SetTurnaroundTime`, exist only there.

Links are in the References section of `docs/mcu/drivers/usb.md`.

---

## 3. Application notes and errata

ST publishes separate documents for what the reference manual leaves out:

- **AN4879**, *USB hardware and PCB guidelines using STM32 MCUs*. It covers VBUS sensing, VDDUSB, and the pull-ups.
- **The errata sheet for the STM32U575**. This lists silicon bugs and places where the manual is wrong. Check it before blaming your own code.

Search st.com for "STM32U5 USB" filtered by document type *Application note* to see what else exists.

---

## 4. The USB specification and the classic references

The reasons behind VBUS and pull-ups, STALL, zero-length packets, `SET_ADDRESS` timing and polled interrupt endpoints aren't STM32-specific. They come from USB itself.

- **USB 2.0 specification**, chapter 7 (electrical: pull-ups, VBUS, connect detection) and chapter 9 (device framework, enumeration).
- **HID 1.11** and the **HID Usage Tables** for report descriptors.
- **Jan Axelson, *USB Complete***: the standard book for writing USB firmware.
- **USB in a NutShell** (beyondlogic.org): free, readable coverage of the same material.

---

## 5. Other drivers for the same IP core

The STM32 USB core is not an ST design. It is the Synopsys **DWC2** core, also used in Raspberry Pi SoCs, the ESP32-S2/S3 and others. The Synopsys databook is not public, so people learned the core from open drivers:

- **Linux kernel**, `drivers/usb/dwc2/`. `hw.h` documents every register.
- **TinyUSB**, `src/portable/synopsys/dwc2/dcd_dwc2.c`.
- **libopencm3**, `lib/usb/usb_dwc_common.c`.

Comments in these drivers often record undocumented behaviour that someone found the hard way.

The lesson applies generally: find out who designed the IP block. A vendor's peripheral is often licensed IP, documented far better elsewhere.

---

## 6. Community knowledge

- ST Community forum, EEVblog forum, Stack Overflow.
- Blog posts from people who already hit the problem.

Traps such as `GCCFG.PWRDWN` (writing 1 powers the transceiver *up*) and the VBUS-override bits are passed on this way far more than through official documentation.

---

## 7. Measurement and experiment

When the documents run out, measure:

| Tool | What it shows |
|---|---|
| Multimeter / oscilloscope on D+ | Whether the 1.5 kΩ pull-up is present, i.e. whether the device connected |
| Logic analyzer | Pin activity and timing |
| USB protocol analyzer (Total Phase Beagle, Ellisys) | Every packet on the wire |
| `usbmon` + Wireshark on the host | The same, free, on the Raspberry Pi |
| `dmesg`, `lsusb -v`, `evtest` | How far enumeration got, from Linux's side |

---

## Typical workflow

1. **Find a vendor example** for the peripheral and get it working on a dev board.
2. **Read the programming-model section** of the reference manual to understand its order.
3. **Rewrite it yourself**, keeping the example as a known-good reference.
4. **When something breaks**, compare register by register against the example, then check the errata and search the forums.
5. **Measure** when comparison doesn't explain it.

For a report, cite the primary sources (RM0456 sections, USB 2.0 chapters, specific HAL functions) rather than secondary summaries.

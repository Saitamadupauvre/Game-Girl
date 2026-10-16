# STM32U575 USB HID Gamepad — Implementation Flow

## Purpose

This is the full bring-up flow for making the STM32U575 enumerate on the Raspberry Pi as a USB HID gamepad (spec §7.1), written as pseudocode. It is a guide for writing `mcu/src/drivers/usb.c`, not code to paste.

It follows the section order of `usb_TODOS.md`.

### How much to trust each value

Every register, offset and bit below carries one of these marks:

| Mark | Meaning |
|---|---|
| ✔ CMSIS | Checked against ST's `stm32u575xx.h` (see [References](#references)). |
| ✔ HAL | Checked against ST's U5 HAL / LL source, which is known to work on this chip. |
| ⚠ RM | Not found in the header or HAL. Confirm it in RM0456 before relying on it. |

The pseudocode uses CMSIS-style names (`OTG.GUSBCFG`, `DIEPCTL(n)`) for readability. Map them onto the `MMIO32(...)` style in `stm32u575_regs.h`.

---

## 0. Big picture

```text
boot
 ├─ clock_init()            raise HCLK above 14.2 MHz (USB FS minimum)
 ├─ timer init              (TIM2 rate depends on the new HCLK)
 ├─ init_usb()
 │   ├─ power   : PWR clock on, VDDUSB valid (USV)
 │   ├─ pins    : PA11/PA12 → AF10
 │   ├─ clocks  : HSI48 on, ICLKSEL = HSI48, OTG clock on
 │   ├─ core    : PHYSEL → soft reset → PWRDWN → force device mode
 │   ├─ device  : VBUS config, DCFG, FIFOs, flush, masks, NVIC
 │   └─ connect : clear DCTL.SDIS
 └─ main loop
     ├─ update_input_state()
     └─ usb_hid_send_if_changed()      (non-blocking)

OTG_FS_IRQHandler  (all enumeration happens here)
 ├─ USBRST   → reset endpoints, arm EP0 OUT for SETUP
 ├─ ENUMDNE  → EP0 = 64 bytes, set TRDT
 ├─ RXFLVL   → pop RX FIFO (SETUP bytes / OUT data)
 ├─ OEPINT   → STUP: handle the SETUP request
 ├─ IEPINT   → XFRC: IN transfer done (EP0 / EP1)
 └─ USBSUSP / WKUINT → set/clear a suspended flag
```

Roles: **Pi = USB host**, **STM32 = USB device**.

---

## Why each step is needed

The pseudocode in the sections below says *what* to write. This section explains *why*. The idea running through all of it: every block on the STM32 starts out **off, unclocked, or disconnected**, to save power. USB depends on several of those blocks at once, so each one has to be switched on, and in the right order.

### Two kinds of clock

Most STM32 peripherals use two separate clocks:

| Clock | What it does | For USB |
|---|---|---|
| **Bus clock** (register clock) | Lets the CPU read and write the peripheral's registers. Without it, writes are ignored and reads return 0. | `RCC_AHB2ENR1.OTGEN`, and HCLK speed |
| **Kernel clock** | Drives the peripheral's actual function, independently of the CPU. | 48 MHz from HSI48, selected by `ICLKSEL` |

This is why turning HSI48 on (`HSI48ON`) is not enough. That only starts the oscillator. `ICLKSEL` is the multiplexer that connects it to the USB block, and `OTGEN` is what lets you program the USB block at all.

### Clocks and power (§2, §3)

| Step | Why |
|---|---|
| **Raise HCLK to ≥ 14.2 MHz** | The USB core moves every packet between the wire and its FIFO RAM over the AHB bus, which runs at HCLK. When the host sends an IN token, the device has only a few bit-times to start answering. At 4 MHz the core cannot read the FIFO fast enough. ST's turnaround table starts at 14.2 MHz, so that is the lowest speed it supports. |
| **Flash wait states first** | Flash memory has a fixed access time. At a higher CPU clock, a read finishes before the flash has its data ready, unless the CPU is told to wait extra cycles. Raise the clock first and the CPU fetches garbage instructions and crashes. So set the wait states *before* increasing the frequency, and lower them only *after* reducing it. |
| **Update `TIM2_INPUT_CLOCK_HZ`** | TIM2 counts ticks of its bus clock. If HCLK goes from 4 to 24 MHz and the prescaler stays the same, the "1 ms" tick becomes 1/6 ms and every `deadline_t` expires six times too early, including the USB timeouts. |
| **PWR clock (`PWREN`)** | The PWR block is a peripheral like any other. Its registers ignore writes until its bus clock is on. |
| **`PWR_SVMCR.USV`** | The USB pins are powered by a separate supply rail, **VDDUSB** (3.3 V). The U5 keeps that domain electrically isolated until software sets `USV` to say "VDDUSB is present and valid". The isolation protects the chip on boards where VDDUSB might be absent. Until the bit is set, the USB transceiver and the PA11/PA12 pads are disconnected from the core, so nothing reaches the wire. |
| **HSI48** | Full-speed USB runs at 12 Mbit/s, and the core samples it with a 48 MHz clock (4× oversampling). The rate must be within **±0.25 %** (USB 2.0 spec). HSI48 is an internal RC oscillator designed for this. |
| **CRS (optional)** | An RC oscillator drifts with temperature. The host sends a Start-of-Frame packet exactly every 1 ms, and CRS uses those as a reference to trim HSI48 continuously. Without CRS it usually works, but it can be the cause of flaky enumeration. |

### Pins (§3)

| Step | Why |
|---|---|
| **GPIOA clock** | Same rule as above: no bus clock, no register writes. |
| **`MODER` = alternate function** | On the U5, most pins come out of reset in *analog* mode: disconnected from everything, to save power. The pin has to be told that a peripheral, not the GPIO block, drives it. |
| **`AFRH` = AF10** | Each pin is wired to an internal multiplexer with up to 16 peripheral options. On PA11/PA12, option 10 is OTG_FS D−/D+. The datasheet's alternate-function table lists these assignments. |
| **High speed (`OSPEEDR`)** | This sets the strength of the output driver. USB edges at 12 Mbit/s need the fast setting, or the signal edges get too slow and the host sees bit errors. |

### Core initialisation (§4)

| Step | Why |
|---|---|
| **`PHYSEL`** | The Synopsys core is designed to work with either an external high-speed PHY chip (ULPI) or the built-in full-speed transceiver. This bit picks the built-in one. ST sets it **before** the reset, so the core comes out of reset already wired to the correct PHY. |
| **Wait `AHBIDL`, then `CSRST`** | The soft reset puts all the core's state machines and FIFO pointers into a known state. Resetting while the core is in the middle of a bus transfer can lock up the bus, so first wait until it reports idle. |
| **Timeouts on those waits** | If the clock or power setup is wrong, these bits never change. Without a timeout, the board silently hangs at boot. With one, you get an error you can report. |
| **`GCCFG.PWRDWN = 1`** | The transceiver (the analog circuit that drives D+/D−) is powered down after reset to save power. Despite the name, writing **1** powers it **up**. Without it, the wire is dead even if everything else is right. |
| **Force device mode** | It's an *OTG* core, so it can act as host or device. Normally it decides from the cable's ID pin. The Game GIRL is always a device and the ID pin isn't used, so force it. The switch-over takes several milliseconds inside the core, which is why you wait or poll `CMOD`. |

### Device setup (§5)

| Step | Why |
|---|---|
| **VBUS sensing / `BVALOEN`+`BVALOVAL`** | The USB spec forbids a device from switching on its D+ pull-up unless the host is supplying VBUS. A self-powered device must never push current into an unpowered host. So the core won't connect until it sees a valid VBUS, normally on a sense pin (PA9). If VBUS isn't wired to that pin, the core thinks no cable is plugged in and **never connects**. The override bits tell it "VBUS is valid". The trade-off: the core can no longer detect an unplug, so the battery-powered Game GIRL would keep its pull-up on with no cable attached. That's harmless in practice, but it's a reason to wire VBUS sensing if the board allows it. |
| **`DCTL.SDIS` during setup** | A full-speed device announces itself by pulling **D+ high through 1.5 kΩ**. The host detects that and starts enumerating straight away. Keep the device "soft-disconnected" until FIFOs, masks and the IRQ are ready, or the host sends requests nobody is ready to answer. |
| **`DCFG.DSPD = full speed`** | The core can also run at high speed with an external PHY. This tells it to use 12 Mbit/s full speed. |
| **FIFO sizes** | The core has one shared block of packet RAM (about 1.25 KB), and software decides how it is split. **RX** is a single shared FIFO, because every OUT and SETUP packet arrives in one queue, tagged with a status word saying which endpoint it belongs to. **Each IN endpoint gets its own TX FIFO**, so a gamepad report waiting on EP1 can't block an EP0 reply during enumeration. If two regions overlap, packets corrupt each other. |
| **Flush after sizing** | This clears any stale read/write pointers left over from before the new sizes. |
| **Three interrupt switches** | `GINTMSK` picks *which* core events raise the USB interrupt. `GAHBCFG.GINTMSK` is the core's master on/off switch. The NVIC decides whether the *CPU* takes that interrupt. All three must be on. Miss one and nothing happens, with no error. |
| **Vector table entry** | When the NVIC accepts IRQ 73, the CPU jumps to the address stored in slot 73 of the vector table. If that slot holds the default handler, the CPU hangs in an infinite loop. |
| **Clear `SDIS` last** | This is the moment the host first sees the device. Everything after it is driven by host requests. |

### Interrupts and enumeration (§6, §7)

| Event / step | Why |
|---|---|
| **USB reset** | After detecting the pull-up, the host holds the bus in reset (~10 ms). Afterwards the device must be at **address 0** with only EP0 active. The host can also reset the device at any time, for example when the driver reloads, so the handler must fully return to that default state. |
| **Enumeration done** | Only now is the speed known, so this is where EP0's packet size is set (64 bytes for full speed) and `TRDT` is programmed. `TRDT` tells the core how many PHY clocks it has to fetch data from the FIFO. It depends on HCLK, which is why the value comes from a table. |
| **`STUPCNT = 3`** | The host is allowed to send up to three SETUP packets back to back, for example if one is lost. The core needs room to accept them without NAKing. Devices are not allowed to NAK a SETUP packet. |
| **Pop RX FIFO, but act on `STUP`** | `RXFLVL` only means "bytes are waiting". `DOEPINT.STUP` means the SETUP stage is complete and the 8 bytes are final. If you act too early, you can process a SETUP packet that a retry is about to overwrite. |
| **Re-arm EP0 after each SETUP** | The endpoint's transfer counters run down as packets arrive. If you don't reload them, the core stops accepting the next request. |
| **Status stage (ZLP)** | Every control transfer ends with a zero-length packet in the opposite direction. It's the "done, OK" handshake, and the host waits for it. A missing ZLP shows up as a timeout in `dmesg` (error −110 or −71). |
| **STALL** | This is the USB way of saying "I don't support that request". It's a normal, expected answer. Linux probes for things the device may not have, such as the device qualifier. |
| **Device qualifier → STALL** | That descriptor only exists for high-speed-capable devices. A full-speed-only device must STALL the request (USB 2.0 §9.6.2). |
| **`SET_ADDRESS` writes `DAD` immediately** | The host sends the status stage to **address 0**, and the device uses the new address only after that. The DWC2 core handles this switch-over itself, but only if `DAD` is already written when the status stage runs. If you write it later, the device answers on the wrong address and Linux reports "device not accepting address". |
| **`SET_CONFIGURATION` activates EP1** | Until the host picks a configuration, only EP0 may be active. Endpoint 1 officially exists only after this request. |
| **HID class requests** | `usbhid` on Linux reads the report descriptor to learn the button layout, and sends `SET_IDLE`. Without the report descriptor the device enumerates but is never turned into an input device. |

### Sending reports (§9)

| Step | Why |
|---|---|
| **Interrupt IN endpoint** | Despite the name, the device **cannot** interrupt the host. The host *polls* EP1 every `bInterval` ms (1 ms here). If the TX FIFO holds a report, the core sends it. If not, the core answers NAK on its own. So "sending" a report just means loading it into the FIFO and letting the next poll pick it up. |
| **`ep1_busy`** | You can't load a second report until the host has taken the first. `XFRC` on EP1 tells you that happened. |
| **Only send on change** | HID reports describe *state*, not events. If nothing changed, NAKing is correct and saves bus bandwidth. The idle timer covers the case where the host has asked for a periodic resend. |
| **`volatile` shared flags** | The ISR changes `configured`, `suspended` and `ep1_busy` behind the main loop's back. Without `volatile`, the compiler may keep a stale copy in a register and the loop never sees the change. |
| **Pure `hid_pack`** | All the logic that doesn't touch registers can be unit-tested on the PC. Bugs left over on the board are then down to hardware and setup, which narrows the debugging. |

---

## 1. Base addresses (non-secure aliases)

| Block | Address | Source |
|---|---|---|
| `PERIPH_BASE_NS` | `0x4000_0000` | ✔ CMSIS |
| `GPIOA` | `0x4202_0000` (AHB2 + `0x00000`) | ✔ CMSIS |
| `OTG_FS` | `0x4204_0000` (AHB2 + `0x20000`) | ✔ CMSIS (matches current `OTG_FS_BASE_NS`) |
| `PWR` | `0x4602_0800` (AHB3 + `0x0800`) | ✔ CMSIS |
| `RCC` | `0x4602_0C00` (AHB3 + `0x0C00`) | ✔ CMSIS (matches current `RCC_BASE_NS`) |
| `FLASH` regs | `0x4002_2000` (AHB1 + `0x02000`) | ✔ CMSIS |

### OTG_FS register map (offsets from `OTG_FS`)

| Register | Offset | Register | Offset |
|---|---|---|---|
| `GOTGCTL` | `0x000` | `GRXSTSP` | `0x020` |
| `GAHBCFG` | `0x008` | `GRXFSIZ` | `0x024` |
| `GUSBCFG` | `0x00C` | `DIEPTXF0` | `0x028` |
| `GRSTCTL` | `0x010` | `GCCFG` | `0x038` |
| `GINTSTS` | `0x014` | `DIEPTXF1` | `0x104` |
| `GINTMSK` | `0x018` | | |
| `DCFG` | `0x800` | `DAINT` | `0x818` |
| `DCTL` | `0x804` | `DAINTMSK` | `0x81C` |
| `DSTS` | `0x808` | `PCGCCTL` | `0xE00` |
| `DIEPMSK` | `0x810` | `DOEPMSK` | `0x814` |

Per-endpoint registers, with `n` = endpoint number (all ✔ CMSIS):

```text
DIEPCTL(n)  = 0x900 + n*0x20     DOEPCTL(n)  = 0xB00 + n*0x20
DIEPINT(n)  = 0x908 + n*0x20     DOEPINT(n)  = 0xB08 + n*0x20
DIEPTSIZ(n) = 0x910 + n*0x20     DOEPTSIZ(n) = 0xB10 + n*0x20
DTXFSTS(n)  = 0x918 + n*0x20
FIFO(n)     = 0x1000 + n*0x1000   (32-bit push/pop port for endpoint n)
```

`FIFO(n)` is a single 32-bit port, not RAM. Every read pops one word and every write pushes one word. ✔ HAL (`USBx_DFIFO`).

---

## 2. Clocks — `clock_init()`

The chip boots on MSIS at 4 MHz. ST's own turnaround table (`USB_SetTurnaroundTime`, ✔ HAL) only covers **HCLK ≥ 14.2 MHz** for full speed, so HCLK must be raised first.

The simplest option is to keep MSIS and switch its range. No PLL is needed.

| MSIS range | Freq | Flash wait states at VOS range 4 | Source |
|---|---|---|---|
| 4 (reset) | 4 MHz | 0 | ✔ HAL |
| 2 | 16 MHz | 1 | ✔ HAL `RCC_SetFlashLatencyFromMSIRange` |
| 1 | 24 MHz | 2 | ✔ HAL |
| 0 | 48 MHz | not allowed in range 4 (needs range 1–3) | ✔ HAL |

Recommendation: use **24 MHz** (range 1). It sits well above the minimum, and the TRDT value is `0x8`.

```text
clock_init():
    RCC.AHB3ENR  |= PWREN                       // bit 2              ✔ CMSIS (AHB3ENR @ 0x94)

    // ⚠ RM: confirm USB OTG_FS is allowed in VOS range 4 (PWR chapter,
    //        voltage-scaling table). If not: set PWR.VOSR.VOS (bits 17:16)
    //        to range 2 or 1 and wait for VOSR.VOSRDY (bit 15) BEFORE
    //        raising the clock.

    // Raise wait states BEFORE raising the frequency
    FLASH.ACR = (FLASH.ACR & ~LATENCY) | 2      // LATENCY @ bit 0    ✔ CMSIS
    wait until (FLASH.ACR & LATENCY) == 2

    RCC.ICSCR1 = (ICSCR1 & ~MSISRANGE) | (1 << 28)   // MSISRANGE @ 28 ✔ CMSIS
               | MSIRGSEL                            // bit 23: use ICSCR1 range ✔ CMSIS
    // ⚠ RM: check whether to wait for RCC_CR.MSISRDY after the change

    update TIM2_INPUT_CLOCK_HZ in timer.c to 24'000'000
    (PSC becomes 23 for a 1 MHz counter, keeping the 1 ms tick)
```

> If you ever go **down** in frequency, change the order: lower the clock first, then the wait states.

---

## 3. `init_usb()` — power, pins, clocks

```text
usb_power_init():
    RCC.AHB3ENR |= PWREN                        // already done in clock_init
    PWR.SVMCR   |= USV                          // bit 28, SVMCR @ 0x10   ✔ CMSIS
                                                // = HAL_PWREx_EnableVddUSB ✔ HAL
    // Optional: UVMEN (bit 24) enables the VDDUSB monitor, then wait for UVMRDY ⚠ RM

usb_pins_init():
    RCC.AHB2ENR1 |= GPIOAEN                     // bit 0 (already in regs.h)
    GPIOA.MODER   : PA11, PA12 = 0b10 (alternate function)   // MODER @ 0x00 ✔ CMSIS
    GPIOA.OSPEEDR : PA11, PA12 = very high (0b11)            // OSPEEDR @ 0x08
    GPIOA.AFRH    : PA11 → AF10, PA12 → AF10                 // AFR[1] @ 0x24 ✔ CMSIS
                    (AFRH fields: pin 8..15, 4 bits each → PA11 bits 15:12, PA12 bits 19:16)
                    GPIO_AF10_USB = 0x0A                      ✔ HAL gpio_ex.h
    // ⚠ Datasheet DS13737 alternate-function table: confirm PA11 = OTG_FS_DM,
    //    PA12 = OTG_FS_DP on AF10 for your package.

usb_clocks_init():
    RCC.CR |= HSI48ON (bit 12); wait for HSI48RDY (bit 13)   ✔ CMSIS  (already written)
    RCC.CCIPR1.ICLKSEL (bits 27:26) = 0b00 → HSI48           ✔ CMSIS @ 0xE0, ✔ HAL
        (01 = PLL2Q, 10 = PLL1Q, 11 = MSIK)
    RCC.AHB2ENR1 |= OTGEN (bit 14)                            ✔ CMSIS  (already written)
```

HSI48 is not crystal-accurate. Full speed needs ±0.25%, and the U5 has a CRS peripheral that trims HSI48 against the host's SOF packets. Start without it. If you later see flaky enumeration or CRC errors in `usbmon`, enable CRS with SYNC source = USB SOF. ⚠ RM, CRS chapter.

---

## 4. Core initialisation — order matters

ST's U575 path (`USB_CoreInit`, then `USB_SetCurrentMode`, then `USB_DevInit`, ✔ HAL) runs in this order:

```text
usb_core_init():
    OTG.GUSBCFG |= PHYSEL                       // bit 6: embedded FS PHY   ✔ CMSIS/HAL

    core_soft_reset():                          // ✔ HAL USB_CoreReset
        wait (with timeout) until GRSTCTL.AHBIDL (bit 31) == 1
        GRSTCTL |= CSRST (bit 0)
        wait (with timeout) until GRSTCTL.CSRST == 0
        on timeout → return error (do not hang boot)

    OTG.GCCFG |= PWRDWN                         // bit 16: TURN ON the FS transceiver
                                                // (despite the name: 1 = powered)  ✔ HAL
                                                // ← missing from current usb.c

    force_device_mode():                        // ✔ HAL USB_SetCurrentMode
        GUSBCFG &= ~(FHMOD | FDMOD)             // bits 29, 30
        GUSBCFG |= FDMOD
        wait until GINTSTS.CMOD (bit 0) == 0    // 0 = device mode
            HAL polls every 10 ms up to a max; ≥25 ms is the spec figure
            → use deadline_t, give up after e.g. 50 ms
```

**Difference from the current `usb.c`:** it forces the mode first and then resets. Follow ST's order: PHYSEL → reset → PWRDWN → mode. You can also poll `CMOD` instead of always waiting a fixed 25 ms.

---

## 5. Device setup

```text
usb_device_init():
    // --- VBUS: Pi supplies VBUS, but is it wired to a sense pin? ---
    if NO VBUS sensing (simplest, and the default suggestion):    ✔ HAL
        GCCFG   &= ~VBDEN                       // bit 21
        GOTGCTL |= BVALOEN | BVALOVAL           // bits 6, 7: pretend VBUS is valid
    else:
        GCCFG   |= VBDEN                        // needs VBUS on PA9 ⚠ datasheet/board

    DCTL     |= SDIS                            // bit 1: stay disconnected during setup
    PCGCCTL   = 0                               // restart PHY clock

    DCFG: DSPD (bits 1:0) = 0b11 → full speed (embedded PHY)       ✔ CMSIS / HAL
          PFIVL (bits 12:11) = 0b00 → 80 %
          DAD (bits 10:4) = 0

    // --- FIFO RAM layout (units = 32-bit words) ---
    //   ⚠ RM: OTG_FS FIFO RAM total = 1.25 KB = 320 words. Confirm.
    GRXFSIZ   = 128                              // RX FIFO, shared by all OUT EPs
    DIEPTXF0  = (64 << 16) | 128                 // depth 64 words, start at 128
    DIEPTXF1  = (64 << 16) | 192                 // depth 64 words, start at 192
                                                 // total 256 ≤ 320 ✔
    (DIEPTXF format: [31:16] depth in words, [15:0] start address in words)

    flush_tx_fifo(all):  wait AHBIDL; GRSTCTL = TXFFLSH | (0x10 << 6); wait TXFFLSH==0 ✔ HAL
    flush_rx_fifo():     wait AHBIDL; GRSTCTL = RXFFLSH;               wait RXFFLSH==0

    DIEPMSK = DOEPMSK = DAINTMSK = 0
    for each endpoint n: clear DIEPCTL/DOEPCTL (SNAK, EPDIS if enabled),
                         DIEPTSIZ = DOEPTSIZ = 0,
                         DIEPINT = DOEPINT = 0xFB7F            ✔ HAL

    GINTSTS = 0xFFFFFFFF                          // clear any pending (write-1-to-clear)
    GINTMSK = USBRST | ENUMDNEM | RXFLVLM | IEPINT | OEPINT | USBSUSPM | WUIM
              // bits 12, 13, 4, 18, 19, 11, 31                 ✔ CMSIS
    GAHBCFG |= GINTMSK                            // bit 0: global interrupt enable ✔ CMSIS

    NVIC enable OTG_FS_IRQn = 73                  ✔ CMSIS
        → NVIC_ISER2 (0xE000E108) |= 1 << (73 - 64)   // bit 9
    vector table entry name: OTG_FS_IRQHandler    ✔ ST startup_stm32u575xx.s

    DCTL &= ~SDIS                                 // CONNECT: D+ pull-up on, host sees us
```

> `OTG_FS_IRQHandler` only works once you have a vector table and startup code (`usb_TODOS.md` §8). Without them, no interrupt fires and enumeration cannot happen.

---

## 6. Interrupt handler

```text
OTG_FS_IRQHandler():
    sts = GINTSTS & GINTMSK

    if sts & USBRST:         on_usb_reset();      GINTSTS = USBRST    (w1c)
    if sts & ENUMDNE:        on_enum_done();      GINTSTS = ENUMDNE
    if sts & RXFLVL:         on_rx_fifo();        (no flag clear — popping clears it)
    if sts & OEPINT:         on_out_ep_irq();
    if sts & IEPINT:         on_in_ep_irq();
    if sts & USBSUSP:        suspended = true;    GINTSTS = USBSUSP
    if sts & WKUINT:         suspended = false;   GINTSTS = WKUINT
```

### 6.1 USB reset (host pulled the bus into reset) — ✔ HAL

```text
on_usb_reset():
    DCTL &= ~RWUSIG
    flush_tx_fifo(all)
    for each endpoint n:
        DIEPINT(n) = 0xFB7F;  DIEPCTL(n) &= ~STALL
        DOEPINT(n) = 0xFB7F;  DOEPCTL(n) &= ~STALL;  DOEPCTL(n) |= SNAK
    DAINTMSK |= (1 << 16) | (1 << 0)             // OUT EP0, IN EP0
    DOEPMSK  |= STUPM (bit 3) | XFRCM (bit 0)
    DIEPMSK  |= XFRCM (bit 0) | TOM (bit 3)
    DCFG     &= ~DAD                             // address back to 0
    usb_state = DEFAULT;  configured = false
    ep0_arm_setup()
```

```text
ep0_arm_setup():                                  // ✔ HAL USB_EP0_OutStart
    DOEPTSIZ(0) = (3 << 29)                       // STUPCNT = 3 back-to-back SETUPs
                | (1 << 19)                       // PKTCNT = 1
                | 24                              // XFRSIZ = 3 * 8 bytes
    // ⚠ The U575 CMSIS header labels bits 30:29 as RXDPID, not STUPCNT.
    //    HAL writes them as STUPCNT=3. Confirm the field in RM0456 OTG_DOEPTSIZ0.
```

### 6.2 Enumeration done (reset finished, speed negotiated) — ✔ HAL

```text
on_enum_done():
    speed = (DSTS >> 1) & 0x3                     // ENUMSPD; 0b11 = full speed
    DIEPCTL(0) &= ~MPSIZ                          // MPSIZ = 0b00 → EP0 max packet 64
    DCTL |= CGINAK                                // bit 8: clear global IN NAK
    GUSBCFG.TRDT (bits 13:10) = value from HCLK:  ✔ HAL table
        14.2–15 MHz: 0xF   16–17.2: 0xD   20–21.8: 0xA
        24–27.7 MHz: 0x8   ≥32: 0x6       (full table in USB_SetTurnaroundTime)
```

### 6.3 RX FIFO — read data out of the core ✔ HAL

```text
on_rx_fifo():
    GINTMSK &= ~RXFLVLM                           // HAL masks while popping
    st     = GRXSTSP                              // POP the status word
    ep     = st & 0xF                             // EPNUM   bits 3:0
    bcnt   = (st >> 4) & 0x7FF                    // BCNT    bits 14:4
    pktsts = (st >> 17) & 0xF                     // PKTSTS  bits 20:17

    switch pktsts:
        6 (SETUP data):    read 2 words from FIFO(0) → setup_pkt[8]
        2 (OUT data):      read ceil(bcnt/4) words from FIFO(0) → buffer
        3 (OUT complete), 4 (SETUP complete):  nothing to read
    GINTMSK |= RXFLVLM
```

Do **not** handle the SETUP request here. Wait for `DOEPINT.STUP` (6.4), which means the core has finished the SETUP stage. This is the pattern HAL uses.

### 6.4 OUT endpoint interrupt

```text
on_out_ep_irq():
    pending = DAINT >> 16
    if pending & 1:                               // EP0 OUT
        i = DOEPINT(0)
        if i & STUP (bit 3):
            DOEPINT(0) = STUP
            handle_setup(setup_pkt)               // section 7
            ep0_arm_setup()                       // re-arm for the next SETUP
        if i & XFRC (bit 0):
            DOEPINT(0) = XFRC                     // OUT status ZLP received
```

### 6.5 IN endpoint interrupt

```text
on_in_ep_irq():
    pending = DAINT & 0xFFFF
    if pending & (1 << 0):  DIEPINT(0) = XFRC;  ep0_in_done()
    if pending & (1 << 1):  DIEPINT(1) = XFRC;  ep1_busy = false
```

### 6.6 Low-level EP0 helpers

```text
ep0_send(data, len):                              // len ≤ 64 for everything here
    DIEPTSIZ(0) = (1 << 19) | len                 // PKTCNT=1, XFRSIZ=len
    DIEPCTL(0) |= EPENA (31) | CNAK (26)
    for each 4-byte chunk of data (little-endian, pad last word):
        FIFO(0) = word
    if len > 0: ep0_arm_status_out()              // host will send an OUT ZLP status

ep0_send_zlp():          ep0_send(nothing, 0)     // status stage for no-data requests

ep0_arm_status_out():
    DOEPTSIZ(0) = (3 << 29) | (1 << 19) | 64
    DOEPCTL(0) |= EPENA | CNAK

ep0_stall():
    DIEPCTL(0) |= STALL (bit 21);  DOEPCTL(0) |= STALL
    (the core clears STALL by itself when the next SETUP arrives)
```

Always send `min(wLength, descriptor_length)` bytes. Linux first asks for 8 or 64 bytes of the device descriptor, then asks again for the full length.

---

## 7. Control requests (`handle_setup`)

SETUP packet layout (USB 2.0 §9.3):

```text
byte 0 bmRequestType  bit7 = direction (1 = IN), bits 6:5 = type (0 std, 1 class), bits 4:0 = recipient
byte 1 bRequest
byte 2..3 wValue   (LE)
byte 4..5 wIndex   (LE)
byte 6..7 wLength  (LE)
```

```text
handle_setup(s):
    switch (s.bmRequestType, s.bRequest):

    (0x80, GET_DESCRIPTOR=6):
        switch wValue >> 8:
            1 DEVICE  → ep0_send(dev_desc, min(18, wLength))
            2 CONFIG  → ep0_send(cfg_desc, min(34, wLength))
            3 STRING  → index = wValue & 0xFF → ep0_send(str[index]) or stall
            6 DEVICE_QUALIFIER → ep0_stall()       // correct answer for a FS-only device
            else      → ep0_stall()

    (0x00, SET_ADDRESS=5):
        DCFG.DAD = wValue & 0x7F                   // write NOW, before the status stage
        ep0_send_zlp()                             // DWC2 applies it after the status ✔ HAL
        usb_state = ADDRESS

    (0x00, SET_CONFIGURATION=9):
        if wValue == 1:  ep1_activate(); configured = true; ep0_send_zlp()
        elif wValue == 0: configured = false; ep0_send_zlp()
        else: ep0_stall()

    (0x80, GET_CONFIGURATION=8):  ep0_send([configured ? 1 : 0], 1)
    (0x80, GET_STATUS=0):         ep0_send([0, 0], 2)    // bus powered, no remote wakeup
    (0x81, GET_STATUS=0), (0x82, GET_STATUS=0): ep0_send([0, 0], 2)

    // --- HID, recipient = interface ---
    (0x81, GET_DESCRIPTOR=6):
        0x22 REPORT → ep0_send(hid_report_desc, min(29, wLength))
        0x21 HID    → ep0_send(cfg_desc + 18, 9)
    (0x21, SET_IDLE=0x0A):        idle_rate = wValue >> 8; ep0_send_zlp()  // Linux sends this
    (0xA1, GET_REPORT=0x01):      ep0_send(current_report, 2)
    (0x01, SET_INTERFACE=11):     wValue == 0 ? ep0_send_zlp() : ep0_stall()

    default: ep0_stall()
```

```text
ep1_activate():                                    // interrupt IN, endpoint 0x81
    DIEPCTL(1) = USBAEP (15)
               | (3 << 18)                         // EPTYP = interrupt
               | (1 << 22)                         // TXFNUM = 1 (uses DIEPTXF1)
               | SD0PID (28)                       // DATA0
               | SNAK (27)
               | 8                                 // MPSIZ = wMaxPacketSize
    DAINTMSK |= (1 << 1)
    ep1_busy = false
```

---

## 8. Descriptors

### 8.1 Device (18 bytes) — USB 2.0 Table 9-8

```text
12        bLength
01        bDescriptorType = DEVICE
00 02     bcdUSB 2.00
00 00 00  class/subclass/protocol = per-interface
40        bMaxPacketSize0 = 64
VV VV     idVendor   ← decision: e.g. 0x1209 (pid.codes) test PID
PP PP     idProduct
00 01     bcdDevice 1.00
01 02 03  iManufacturer, iProduct, iSerialNumber
01        bNumConfigurations
```

### 8.2 Configuration block (34 bytes total, sent as one)

```text
-- Configuration (9) --
09 02  22 00   01   01   00   80     32
len type total  nIf  cfg# iCfg attr  MaxPower = 50 × 2 mA = 100 mA
                               (0x80 = bus powered; 0xC0 if self-powered by battery)
-- Interface (9) --
09 04  00  00  01   03     00      00     00
         if# alt nEP class  sub     proto  iIf
                    =HID   =none   =none   (sub/proto 0 = not a boot device)
-- HID (9) — HID 1.11 §6.2.1 --
09 21  11 01  00       01      22         1D 00
       bcdHID country  nDesc   type=REPORT  wLength = 29
-- Endpoint (7) --
07 05  81       03          08 00      01
       EP1 IN   interrupt   wMaxPkt=8  bInterval = 1 ms (FS: in ms)
```

### 8.3 Strings

```text
str[0] = 04 03 09 04                       // language ID list: 0x0409 (en-US)
str[n] = (2 + 2*len) 03 <UTF-16LE chars>   // e.g. "Game GIRL"
```

### 8.4 HID report descriptor (29 bytes) — 12 buttons + 4 padding bits

```text
05 01     Usage Page (Generic Desktop)
09 05     Usage (Game Pad)
A1 01     Collection (Application)
05 09       Usage Page (Button)
19 01       Usage Minimum (Button 1)
29 0C       Usage Maximum (Button 12)
15 00       Logical Minimum (0)
25 01       Logical Maximum (1)
75 01       Report Size (1)
95 0C       Report Count (12)
81 02       Input (Data, Var, Abs)        ← 12 button bits
75 04       Report Size (4)
95 01       Report Count (1)
81 03       Input (Const, Var, Abs)       ← 4 padding bits
C0        End Collection
```

Check it with `hid-decode` from hid-tools rather than by eye.

Resulting report, 2 bytes, no report ID:

```text
byte 0: bit0 = Button 1 … bit7 = Button 8
byte 1: bit0 = Button 9 … bit3 = Button 12, bits 7:4 = 0
```

### 8.5 Button order is a design decision

Linux's `hid-input` maps Button *n* on a Game Pad to `BTN_GAMEPAD + (n-1)`, so the order decides the evdev names. This mapping is from memory of `drivers/hid/hid-input.c`; check it with `evtest`.

| Button # | evdev code | Current `btn_t` bit at that position |
|---|---|---|
| 1 | `BTN_SOUTH` / `BTN_A` | A ✔ |
| 2 | `BTN_EAST` / `BTN_B` | B ✔ |
| 3 | `BTN_C` | X ✗ |
| 4 | `BTN_NORTH` / `BTN_X` | Y |
| 5 | `BTN_WEST` / `BTN_Y` | Up |
| 7 / 8 | `BTN_TL` / `BTN_TR` | Left / Right |
| 11 / 12 | `BTN_SELECT` / `BTN_START` | Select / Start ✔ |
| 13 | `BTN_MODE` | — (natural spot for **Menu**) |

You have three options:

1. **Report bits = `btn_t` bits.** The packer is trivial, and you remap in the emulator.
2. **Reorder in the packer** to match the Linux names. This adds a lookup table.
3. **Send the D-pad as a hat switch or X/Y axes** instead of buttons. That's the conventional gamepad layout, and some apps and udev only treat a device as a joystick if it has axes. Check with `udevadm info /dev/input/eventX` (look for `ID_INPUT_JOYSTICK=1`).

The spec's enum (Cahier des charges §6.3) has `BTN_MENU`, but `btn_t` has no Menu button. Settle that before freezing the descriptor.

---

## 9. Sending reports (main loop, non-blocking)

```text
// pure — host-testable, no registers
hid_pack(uint16 current) -> hid_report_t:
    return { btn_low = current & 0xFF, btn_high = (current >> 8) & 0x0F }

usb_hid_send_if_changed(inputs):                  // called every main-loop pass
    if !configured or suspended:      return
    report = hid_pack(inputs.current)
    if report == last_sent and !timer_expired(&idle_deadline): return
    if ep1_busy:                      return      // previous report still in flight
    if (DTXFSTS(1) & 0xFFFF) < 1:     return      // INEPTFSAV: free words in TX FIFO ✔ CMSIS

    ep1_busy = true                               // set BEFORE enabling (ISR clears it)
    DIEPTSIZ(1) = (1 << 19) | 2                   // 1 packet, 2 bytes
    DIEPCTL(1) |= EPENA | CNAK
    FIFO(1)     = report.btn_low | (report.btn_high << 8)
    last_sent = report;  timer_start(&idle_deadline, IDLE_MS)
```

Concurrency: `configured`, `suspended` and `ep1_busy` are written in the ISR and read in the main loop, so make them `volatile`. The main loop only touches `FIFO(1)` and the ISR only touches `FIFO(0)`. Separate FIFOs don't conflict.

Unit tests for `hid_pack`, to add to `mcu/tests/`:
- every single button maps to the right bit
- no buttons pressed gives `{0, 0}`
- all buttons pressed gives `{0xFF, 0x0F}`
- bits 12–15 of `current` never leak into the padding

---

## 10. Bring-up checklist (on the Pi)

| Step | Command | Healthy result |
|---|---|---|
| Kernel saw it | `dmesg -w` while plugging in | `new full-speed USB device`, `hid-generic … Gamepad` |
| Descriptors | `lsusb -v -d VID:PID` | config, interface class 3, EP 0x81 interrupt |
| Report desc | `sudo usbhid-dump -d VID:PID` then `hid-decode` | matches §8.4 |
| Events | `evtest /dev/input/eventX` | one key code per button |
| Wire-level | `modprobe usbmon` + Wireshark on `usbmonN` | shows exactly which request failed |

Common failures, roughly in order of likelihood:

| Symptom | Usual cause |
|---|---|
| Nothing in `dmesg` | No pull-up (`SDIS` still set), `PWRDWN` not set, `USV` not set, wrong AF, or no IRQ / vector table |
| `device descriptor read/64, error -71` | EP0 FIFO or IN path wrong, wrong `TRDT`, or HCLK too low |
| `device not accepting address` | `DAD` written at the wrong time, or no status ZLP |
| Enumerates but no events | EP1 not activated, wrong `TXFNUM`, or report length ≠ descriptor |

---

## References

### Chip — STMicroelectronics

- **RM0456**, STM32U5 reference manual. Chapters: OTG_FS (USB on-the-go full-speed), RCC, PWR, GPIO, FLASH, CRS. This is the authority for everything marked ⚠ RM.
  <https://www.st.com/resource/en/reference_manual/rm0456-stm32u5-series-armbased-32bit-mcus-stmicroelectronics.pdf>
- **DS13737**, STM32U575xx datasheet. Alternate-function table (PA11/PA12 AF10) and electrical specs.
  <https://www.st.com/resource/en/datasheet/stm32u575ag.pdf>
  *(ST blocks scripted downloads, so open these in a browser.)*
- **CMSIS device header**, `stm32u575xx.h`. Every ✔ CMSIS value above came from this file.
  <https://github.com/STMicroelectronics/cmsis-device-u5/blob/master/Include/stm32u575xx.h>
- **Startup file**, `startup_stm32u575xx.s`. Vector table and the `OTG_FS_IRQHandler` name.
  <https://github.com/STMicroelectronics/cmsis-device-u5/blob/master/Source/Templates/gcc/startup_stm32u575xx.s>
- **U5 HAL / LL drivers**, the ✔ HAL source:
  - `stm32u5xx_ll_usb.c`: `USB_CoreInit`, `USB_CoreReset`, `USB_SetCurrentMode`, `USB_DevInit`, `USB_SetTurnaroundTime`, `USB_EP0_OutStart`
    <https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Src/stm32u5xx_ll_usb.c>
  - `stm32u5xx_hal_pcd.c`: `HAL_PCD_IRQHandler`, which sets the order of the reset, enum-done and RX FIFO handling
    <https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Src/stm32u5xx_hal_pcd.c>
  - `stm32u5xx_hal_pwr_ex.c`: `HAL_PWREx_EnableVddUSB` (`USV`)
    <https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Src/stm32u5xx_hal_pwr_ex.c>
  - `stm32u5xx_hal_rcc.c`: `RCC_SetFlashLatencyFromMSIRange` (wait-state table)
    <https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Src/stm32u5xx_hal_rcc.c>
  - `stm32u5xx_ll_rcc.h`: MSIS range frequencies, `ICLKSEL` encoding
    <https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Inc/stm32u5xx_ll_rcc.h>
  - `stm32u5xx_hal_gpio_ex.h`: `GPIO_AF10_USB`
    <https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Inc/stm32u5xx_hal_gpio_ex.h>
- **ST USB device middleware**, a reference for the descriptor and request flow (HID class included).
  <https://github.com/STMicroelectronics/stm32-mw-usb-device>

### Other DWC2 drivers (same Synopsys core, independent code)

- **TinyUSB**, `dcd_dwc2.c`. Clean, well-commented device-mode driver.
  <https://github.com/hathach/tinyusb/blob/master/src/portable/synopsys/dwc2/dcd_dwc2.c>
- **libopencm3**, `usb_dwc_common.c`. Small, register-level, close to how you are writing yours.
  <https://github.com/libopencm3/libopencm3/blob/master/lib/usb/usb_dwc_common.c>

### USB and HID specifications

- **USB 2.0 Specification**, chapter 9 (device framework, standard requests, descriptors).
  <https://www.usb.org/document-library/usb-20-specification>
- **HID 1.11**, class definition: HID descriptor, class requests, report descriptor items.
  <https://www.usb.org/document-library/device-class-definition-hid-111>
- **HID Usage Tables 1.6**, Generic Desktop (Game Pad 0x05) and Button page (0x09).
  <https://www.usb.org/document-library/hid-usage-tables-16>
- **USB in a NutShell** (Beyond Logic). The most readable intro to enumeration and descriptors.
  <https://www.beyondlogic.org/usbnutshell/usb1.shtml>
- **pid.codes**, free VID/PID allocation for open-source hardware (VID 0x1209).
  <https://pid.codes/>

### Linux (Pi side)

- Kernel HID introduction (report descriptors explained): <https://docs.kernel.org/hid/hidintro.html>
- Linux gamepad specification (expected button and axis layout): <https://docs.kernel.org/input/gamepad.html>
- `hid-input.c` (HID usage → evdev code mapping, §8.5): <https://github.com/torvalds/linux/blob/master/drivers/hid/hid-input.c>
- usbmon (capture USB traffic on the Pi): <https://docs.kernel.org/usb/usbmon.html>
- hid-tools (`hid-decode`, `hid-recorder`): <https://gitlab.freedesktop.org/libevdev/hid-tools>

### Arm

- Cortex-M33 Generic User Guide, NVIC (`ISER` registers):
  <https://developer.arm.com/documentation/100235/latest/>

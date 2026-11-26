# Backup domain and reset cause: reading list

Reading for #42 (reset cause, breadcrumbs, reset-loop guard). Plan in [`error_handling_TODO.md`](error_handling_TODO.md) §5, degraded modes in [`../mcu/power-and-usb-fallback.md`](../mcu/power-and-usb-fallback.md).

Sections from RM0456 Rev 7. **Bold** = must read, *(skim)* = glance only.

## 1. Reset cause: RCC (chapter 11)

| Section | Page | Why |
|---|---|---|
| **11.3.1 Power reset** | 483 | What a power-on clears. The "fresh start" case |
| **11.3.2 System reset** | 484 | Every reset source, and why `PINRSTF` is also set on internal resets (the reset pulse is driven out on NRST) |
| **11.3.3 Backup domain reset** | 485 | What clears `TAMP_BKPxR`: `BDRST`, or power-on with VDD and VBAT both off |
| **11.8.50 RCC_CSR** | 600 | The reset flags, `RMVF`, and their reset values |
| **11.8.36 RCC_APB3ENR** | 568 | `RTCAPBEN`, the bus clock needed to access TAMP |
| 11.4.14 RTC and TAMP clock *(skim)* | 500 | Confirms the APB clock is enough for the backup registers (no LSE or RTC clock) |

## 2. Backup domain write access: PWR (chapter 10)

| Section | Page | Why |
|---|---|---|
| **10.4.7 Battery backup domain** | 408 | What lives there, what powers it (VBAT), what survives a reset |
| **10.10.11 PWR_DBPR** | 458 | `DBP`, the backup domain write protection |
| 10.6.1 Brownout reset *(skim)* | 412 | Why `BORRSTF` is set on every power-on |

## 3. Backup registers: TAMP (chapter 64)

| Section | Page | Why |
|---|---|---|
| **64.4.4 TAMP register write protection** | 2645 | Links back to `DBP` |
| **64.4.5 TAMP secure protection modes** | 2645 | Non-secure access with TrustZone off |
| **64.4.6 Backup registers protection zones** | 2646 | Zones 1/2/3 and their default access. Check BKP0–2 are writable by default |
| **64.4.10 TAMP backup registers and other device secrets erase** | 2647 | What erases them on its own (tamper events, readout protection changes), so a wipe isn't mistaken for a bug |
| **64.7.18 TAMP_BKPxR** | 2684 | The registers |
| 64.7.9 TAMP_SECCFGR *(skim)* | 2671 | Reset value of `BKPRWSEC`/`BKPWSEC`. Only if writes don't stick |
| 64.7.19 TAMP register map *(skim)* | 2685 | Check the `0x100` offset of `TAMP_BKP0R` |

## 4. Watchdog: IWDG (chapter 61)

| Section | Page | Why |
|---|---|---|
| 61.4 IWDG functional description *(skim)* | 2563 | Confirm an IWDG reset is a system reset, so the backup registers survive it |

## Outside RM0456

| Document | Why |
|---|---|
| UM2861 (NUCLEO-U575ZI-Q user manual), power supply section | VBAT is tied to VDD on the Nucleo, so unplugging clears the backup registers. Check again for the PCB if VBAT gets its own supply. The "any non-IWDG reset clears the counters" rule covers it either way |
| PM0264 (Cortex-M33 programming manual), exception entry and return *(optional)* | Why saving and restoring the breadcrumb in an ISR is safe with nested interrupts: each handler restores what it saw on entry |

## Minimum

If short on time: 11.3.2, 11.3.3, 11.8.50, 11.8.36, 10.4.7, 10.10.11, 64.4.6, 64.4.10, 64.7.18.

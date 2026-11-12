# Power Control and USB Fallback (v0.1)

Status: draft v0.1, 2026-11-12. Values marked *(v0.1)* are first guesses to check on the board.

## Purpose

USB is the only data link between the STM32 and the Pi 4. This note decides:

- who controls power for the console;
- how the Pi is shut down cleanly, even when USB is down;
- what the STM32 does when USB fails.

It extends the shutdown sequence in the spec (`Game_GIRL_Pi4_STM32_Cahier_des_charges(1).md` §28), which sends the shutdown request over USB and leaves the power cutoff manual for the first prototype. Error-handling tasks are tracked in [`../todos/error_handling_TODO.md`](../todos/error_handling_TODO.md).

## Principle

**The power role never depends on USB.** The STM32 has two jobs:

| Job | Depends on USB? |
|---|---|
| Power owner: power button, battery (MAX17043), power latch, shutdown sequence | no |
| Controller: buttons and status to the Pi as a USB HID device | yes |

If USB fails, the console loses input. It must never lose the ability to power off safely.

This matches handheld consoles: the 3DS uses one always-on MCU for power and system inputs, and the Steam Deck uses an embedded controller for power plus a separate input MCU on internal USB.

## Signals

| Signal | From → To | Meaning |
|---|---|---|
| `PWR_LATCH` | STM32 GPIO → load switch | holds the battery-powered rail on for both boards |
| `PI_SHUTDOWN_REQ` | STM32 GPIO → Pi GPIO3 | falling edge: "please shut down" |
| `PI_HALTED` | Pi GPIO26 → STM32 GPIO | high: "halted, safe to cut power" |
| USB | STM32 OTG ↔ Pi USB port | input reports and status (battery %, errors) |

STM32 pin numbers are not chosen yet.

## Power latch

A soft latch:

1. Pressing the button powers the rail for a moment.
2. The STM32 boots and drives `PWR_LATCH` to keep the rail on.
3. To turn off, the STM32 releases `PWR_LATCH`.

**Requirement:** during and after an STM32 reset, the pin floats until `init_gpio()` runs again. A pull resistor on the latch line must keep the rail **on** while the pin floats. Otherwise a watchdog or USB-triggered reset cuts the Pi's power mid-write. Turning off must need an active drive from the STM32.

## Sequences

### Power on

```text
button press -> rail on -> STM32 boots, drives PWR_LATCH -> Pi boots (10-20 s) -> Pi enumerates the STM32 over USB
```

### Clean shutdown (button or low battery)

```text
STM32 pulls PI_SHUTDOWN_REQ low
  -> Pi runs a clean poweroff
  -> Pi drives PI_HALTED high
  -> STM32 releases PWR_LATCH
```

- Started by: holding the button for 2 s *(v0.1)*, or the battery service reaching critical level.
- If `PI_HALTED` doesn't go high within 30 s *(v0.1)*, the Pi is assumed hung, and the STM32 releases `PWR_LATCH` anyway.
- No USB is involved, so this also works in degraded mode.
- When USB is up, the STM32 can also send `CMD_REQUEST_SHUTDOWN` first so the Pi can save state, but the GPIO line is what the sequence relies on.

### Forced power off

Holding the button for 10 s *(v0.1)* releases `PWR_LATCH` at once. This is the last resort when the Pi is stuck.

## Pi 4 configuration

In `/boot/firmware/config.txt`:

```text
# STM32 -> Pi: falling edge on GPIO3 starts a clean poweroff
dtoverlay=gpio-shutdown,gpio_pin=3,active_low=1,gpio_pull=up

# Pi -> STM32: GPIO26 goes high once the Pi has halted
dtoverlay=gpio-poweroff,gpiopin=26
```

- GPIO3 is also I2C1's SDA. If the Pi uses I2C1, pick another pin for `gpio_pin`.
- After asserting GPIO26, `gpio-poweroff` waits for the power to disappear (`timeout_ms`) and then logs a warning. That's harmless here, because the STM32 cuts the rail.
- The Pi 4 has no power-button header, so all power control goes through the STM32 latch.

## USB failure handling

### What counts as a fault

| Situation | Fault? | Action |
|---|---|---|
| Init step times out (HSI48, core reset, device mode, FIFO flush) | yes | escalation ladder |
| Not enumerated yet (Pi booting or off) | no | wait |
| Bus suspended by the host (suspend interrupt) | no | wait |
| Host silent after enumeration, no suspend | yes | soft replug, then the escalation ladder |

VBUS is **not** a "host ready" signal: the Pi 4 powers its USB ports for its whole boot.

### Escalation ladder

1. Retry the init up to 3 times *(v0.1)*, pulsing `RCC_AHB2RSTR1.OTGRST` between attempts, with growing waits.
2. Runtime only: soft replug (`DCTL.SDIS = 1` for about 10 ms, then clear it).
3. System reset (`NVIC_SystemReset`). This is safe only because of the latch pull resistor.
4. After 3 USB-triggered resets in a row *(v0.1)*, stop resetting and stay in degraded mode. The counter is cleared after a clean run (see the reset-loop guard in the error-handling TODO).

### Degraded mode

USB is off. Everything on the power side keeps running:

- power button, including clean and forced shutdown;
- battery monitoring and the low-battery shutdown;
- status LEDs, plus a blink code for the USB fault.

The Pi gets no input and no battery percentage. It should show "controller not connected" or "battery unknown".

## Fuel gauge

The STM32 owns the MAX17043 on its own I2C bus and makes every battery safety decision locally. The Pi receives the percentage over USB for display only.

There's deliberately no backup data link (a UART or a shared I2C bus) for the battery percentage. Without USB the console has no input anyway, and a second protocol isn't worth its cost. If a low-battery warning on the Pi is needed without USB, add one more GPIO line instead.

## Wiring notes

- Both boards use 3.3 V logic and share ground. Put a series resistor of about 1 kΩ on `PI_SHUTDOWN_REQ` and `PI_HALTED` to limit current while one board is unpowered or booting.
- The Pi 4 needs up to about 3 A at 5 V. The boost converter (TPS61088 class, spec §9) must hold 5 V under load, or the Pi throttles even when the fuel gauge reads OK.
- Feeding 5 V through header pins 2/4 bypasses the Pi's input protection. Feeding through USB-C keeps it.

## Open questions

- [ ] Latch circuit: a separate load switch, or the boost converter's enable pin? Pull direction and how the button bypass is wired.
- [ ] Is the forced power-off done in STM32 firmware only, or also in hardware (an RC timer on the button) so it works when the STM32 is dead?
- [ ] STM32 pins for `PWR_LATCH`, `PI_SHUTDOWN_REQ`, `PI_HALTED`.
- [ ] Is I2C1 used on the Pi? It decides whether GPIO3 stays the shutdown pin.
- [ ] Button hold times and the halt timeout, measured against the real Pi shutdown time.
- [ ] USB fault blink code, which must not clash with the low-battery patterns in [`services/led.md`](services/led.md).

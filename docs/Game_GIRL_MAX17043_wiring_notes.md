# Game GIRL — MAX17043 Fuel Gauge Wiring Notes

## Purpose

This note explains two important checks before connecting the MAX17043 fuel-gauge breakout to the Game GIRL STM32:

1. **I²C pull-up voltage**
2. **Battery connector polarity**

The module itself is suitable for a single-cell Li-ion/LiPo battery, but cheap breakout boards can differ in how they wire the digital pull-ups and JST connector.

## I²C connection

The STM32 communicates with the MAX17043 over I²C:

```text
STM32                 MAX17043
  │                      │
SDA ─────────────────── SDA
SCL ─────────────────── SCL
GND ─────────────────── GND
```

I²C devices generally do not actively drive the line HIGH. Instead, pull-up resistors bring SDA and SCL back HIGH.

```text
            3.3 V
              │
             4.7k
              │
STM32 SDA ────┼──── MAX17043 SDA
```

For Game GIRL, we ideally want the I²C HIGH level to be **3.3 V** because the STM32 uses 3.3 V logic.

## Why pull-up voltage matters

Some inexpensive MAX17043 breakout boards may connect the I²C pull-up resistors to the battery voltage.

A single-cell LiPo varies approximately between:

```text
~3.0 V discharged
~3.7 V nominal
~4.2 V fully charged
```

If the pull-ups are tied directly to the battery:

```text
          LiPo voltage
          3.0–4.2 V
              │
             4.7k
              │
STM32 SDA ────┼──── MAX17043
```

then SDA and SCL could rise to approximately **4.2 V** when the battery is fully charged.

We do not want to feed 4.2 V into STM32 GPIO pins unless those exact pins have been verified as tolerant of that voltage.

The safer arrangement is:

```text
Battery + ─────────► MAX17043 battery measurement
                         │
3.3 V from STM32 ───────► I²C pull-ups
                         │
STM32 SDA/SCL ◄─────────► I²C
```

The battery can still reach 4.2 V; only the digital communication lines need to remain around 3.3 V.

## How to check the pull-up voltage

Before connecting SDA and SCL to the STM32, power the fuel-gauge board from the battery and use a multimeter in DC-voltage mode.

Measure:

```text
black probe → GND
red probe   → SDA
```

Then:

```text
black probe → GND
red probe   → SCL
```

Expected:

```text
~3.3 V  → good
```

If instead you see:

```text
~4.1–4.2 V
```

investigate the breakout board before connecting it to the STM32.

Possible fixes include:

- removing the onboard pull-up resistors
- adding external pull-ups to 3.3 V
- using a different breakout board
- using level shifting if necessary

## Battery JST connector polarity

A LiPo may use a 2-pin JST connector:

```text
┌────────────┐
│ red  black │
└───┬────┬───┘
    +    -
```

The problem is that not every cheap battery or module follows the same connector polarity.

Two connectors can physically fit while positive and negative are reversed.

So:

```text
plug fits
```

does **not** automatically mean:

```text
polarity is correct
```

Reverse battery polarity can damage the fuel gauge or other electronics immediately.

## How to check battery polarity

Before plugging the battery into the MAX17043 module, use a multimeter in DC-voltage mode.

Measure:

```text
red probe   → expected +
black probe → expected -
```

Interpretation:

```text
+3.7 V  → polarity correct
```

If the meter shows:

```text
-3.7 V
```

the connector polarity is reversed relative to what you expected.

Do not plug it into the board until the polarity has been corrected.

## Board markings

The MAX17043 breakout shown for Game GIRL appears to mark the battery connector with `+` and `-`.

Before connecting the battery, verify:

```text
battery red wire   → board +
battery black wire → board -
```

Do not rely only on wire colors. Confirm with a multimeter.

## Recommended Game GIRL connection

```text
LiPo battery
    │
    ├──────────────► main power system
    │                  │
    │                  ▼
    │             5 V boost converter
    │                  │
    │                  ├── Raspberry Pi 4
    │                  ├── STM32 Nucleo
    │                  ├── LCD
    │                  └── audio
    │
    └──────────────► MAX17043
                        │
                        │ I²C
                        ▼
                    STM32U575
                        │
                        ▼
                 Game GIRL software
```

The MAX17043 does **not** power the console. It only monitors the battery and reports information such as:

- battery voltage
- state of charge
- low-battery alert

## Pre-power checklist

```text
[ ] Confirm module is for 1S LiPo
[ ] Confirm battery polarity with multimeter
[ ] Confirm board + and - markings
[ ] Measure SDA voltage
[ ] Measure SCL voltage
[ ] Confirm I²C HIGH level is safe for STM32
[ ] Connect GND first
[ ] Then connect SDA/SCL
```

These checks take only a few minutes and can prevent accidental damage to the STM32 or fuel-gauge board.

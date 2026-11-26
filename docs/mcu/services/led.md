# LED Status Policy

## Purpose

`mcu/src/services/led.c` decides what the console's three status LEDs should show. Its public interface is `update_led_outputs()`, declared in `mcu/include/led.h`.

The service only makes decisions. It never touches GPIO or any register. `mcu/src/services/led_task.c` gathers the inputs each loop pass, calls the service, and passes the result to `write_GPIO_leds()` in `mcu/src/drivers/gpio.c`.

## LEDs

| LED | Colour | Nucleo | Meaning |
|---|---|---|---|
| Charging LED (`charging_led`) | green | LD1, PC7 | charger connected |
| Battery LED (`battery_led`) | red | LD3, PG2 | battery low or critical |
| Fault LED (`fault_led`) | blue | LD2, PB7 | blink code of the active fault |

All three are active high, driven through a transistor (UM2861 §7.5). PG2 is on the VDDIO2 supply, so `init_GPIO()` sets `PWR_SVMCR.IO2SV` before using it. On the Nucleo, VDDIO2 follows VDD_MCU (SB28 ON, UM2861 §7.4.5 Table 9).

## Battery Policy (green and red)

| Charger | Battery level | Green | Red |
|---|---|---|---|
| unplugged | `BATTERY_LEVEL_OK` | off | off |
| unplugged | `BATTERY_LEVEL_LOW` | off | solid on |
| unplugged | `BATTERY_LEVEL_CRITICAL` | off | blinking |
| plugged in | any | on | off |

- The green LED follows `charger_connected` only.
- The red LED is always off while charging: the green LED already tells the user the battery is being taken care of.
- When the battery is OK and unplugged, both LEDs are off. The screen shows the console is on.

Battery thresholds (35% low, 15% critical) are decided by the battery service. The LED service only receives the resulting `battery_level_t`.

There is no charger or battery driver yet: `led_task()` passes "unplugged, OK", so green and red stay off for now.

## Fault Policy (blue)

`led_task()` fills `active_fault` with `fault_first_active()`: the lowest subsystem with an active fault. A clock fault also stops USB, so it wins over a USB fault.

| Flashes | Fault codes | Meaning |
|---|---|---|
| 2 | `0x10`–`0x12` | clock stuck at 4 MHz (VOS or MSIS failed) |
| 3 | `0x20`–`0x27` | USB start-up failed after every retry |
| 4 | `0x28` (`FAULT_USB_HOST_LOST`) | host stopped answering, soft replug limit reached |
| 5 | `0x29` (`FAULT_USB_HANG`) | USB disabled by the reset guard after repeated watchdog resets |
| off | `FAULT_NONE` or any other code | healthy, or a fault with no blink code |

The fault LED has its own colour so it never clashes with the battery patterns, and needs no priority rules. It goes dark again when the module clears its fault (`fault_clear_failure()`).

After a reset the last fault can also be read with the debugger: `x/wx 0x46007D04` (`TAMP_BKPR(BKP_LAST_FAULT)`).

## Blink Timing

Both patterns are computed from the time passed in by the caller, with no state.

Critical battery (red):

```text
red on  <=>  now_ms % LED_CRITICAL_BLINK_PERIOD_MS < LED_CRITICAL_BLINK_ON_MS
```

Fault code (blue), N flashes then a pause:

```text
flashing_ms = N * LED_FAULT_SLOT_MS
t           = now_ms % (flashing_ms + LED_FAULT_PAUSE_MS)
blue on  <=>  t < flashing_ms  and  t % LED_FAULT_SLOT_MS < LED_FAULT_ON_MS
```

| Constant | Default | Meaning |
|---|---|---|
| `LED_CRITICAL_BLINK_PERIOD_MS` | 250 ms | one full on + off cycle |
| `LED_CRITICAL_BLINK_ON_MS` | 125 ms | on phase within each cycle |
| `LED_FAULT_SLOT_MS` | 500 ms | one flash: on, then off |
| `LED_FAULT_ON_MS` | 200 ms | on phase within each slot |
| `LED_FAULT_PAUSE_MS` | 1500 ms | dark gap before the code repeats |

The pause is three slots long, so the gap between codes is clearly longer than the gap between flashes. `_Static_assert`s in `led.h` reject an on time of 0 or one not shorter than its period or slot.

The same input and time always give the same output. When the 32-bit millisecond counter wraps (about 49 days), one blink phase may be cut short. This is harmless.

## Tests

`mcu/tests/test_led.c` runs on the host and links only the service, no driver. It covers:

- every LED off when idle and healthy;
- green follows the charger at every battery level;
- red solid when low, off when OK;
- critical blink boundaries (`0`, `ON_MS - 1`, `ON_MS`, `PERIOD - 1`, `PERIOD`, far future);
- red off while charging;
- exact duty cycle over one period;
- blue off for unknown codes;
- for every blink code: the number of flashes and the lit time per period;
- blue dark during the pause, then the code repeats;
- red and green unchanged by a fault;
- input not modified.

The tests use the header constants, so they follow any change to the timing.

## Out of Scope

LED brightness, PWM, and electrical validation of the PCB LEDs.

# LED Status Policy

## Purpose

`mcu/src/services/led.c` decides what the console's two status LEDs should show. Its public interface is `update_led_outputs()`, declared in `mcu/include/led.h`.

The service only makes decisions. It never touches GPIO, PWM, or any register: a future LED driver applies the result to the real pins.

## LEDs

| LED | Colour | Meaning |
|---|---|---|
| Charging LED (`charging_led`) | green | charger connected |
| Battery LED (`battery_led`) | red | battery low or critical |

## Policy

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

## Blink Timing

The critical blink is computed from the time passed in by the caller:

```text
red on  <=>  now_ms % LED_CRITICAL_BLINK_PERIOD_MS < LED_CRITICAL_BLINK_ON_MS
```

| Constant | Default | Meaning |
|---|---|---|
| `LED_CRITICAL_BLINK_PERIOD_MS` | 250 ms | one full on + off cycle |
| `LED_CRITICAL_BLINK_ON_MS` | 125 ms | on phase within each cycle |

Half the period gives an even blink; a small on time gives a short flash. A `_Static_assert` in `led.h` rejects an on time of 0 or one not shorter than the period.

The service keeps no state: the same input and time always give the same output. When the 32-bit millisecond counter wraps (about 49 days), one blink phase may be shorter, because 2^32 is not a multiple of 250. This is harmless.

## Update Order

On the target, the main loop is expected to:

1. Advance a millisecond counter on each timer tick.
2. Fill `led_input_t` from the battery service and the charger status pin.
3. Call `update_led_outputs(&input, now_ms)`.
4. Pass the result to an LED GPIO driver.

Steps 2 and 4 depend on drivers that do not exist yet.

## Tests

`mcu/tests/test_led.c` runs on the host and links only the service, no driver. It covers:

- both LEDs off when idle;
- green follows the charger at every battery level;
- red solid when low, off when OK;
- critical blink boundaries (`0`, `ON_MS - 1`, `ON_MS`, `PERIOD - 1`, `PERIOD`, far future);
- red off while charging;
- exact duty cycle over one period;
- input not modified.

The tests use the header constants, so they follow any change to the blink timing.

## Out of Scope

LED brightness, PWM, GPIO pin mapping, and electrical validation.

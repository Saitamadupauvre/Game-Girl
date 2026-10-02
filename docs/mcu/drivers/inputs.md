# Button Input Processing

## Purpose

`mcu/src/services/inputs.c` converts raw logical button samples from the GPIO driver into stable button state and one-update press/release events. Its public state is `input_state_t`, declared in `mcu/include/inputs.h`.

## Update Order

The main loop must process input in this order on every input tick:

```c
read_GPIO_input_pins(&inputs);
update_input_state(&inputs);
```

The GPIO driver writes an active-high raw mask: a set bit means that button is physically pressed. The input service does not need to know that the electrical GPIO signal is active-low.

## Debounce Method

The service retains the latest five raw masks in `debounce_history`. For each button bit independently:

- It becomes debounced pressed only when that bit is set in all five samples.
- It becomes debounced released only when that bit is clear in all five samples.
- Otherwise, its previous debounced state is retained.

This filters mechanical contact bounce without one bouncing button affecting the debounce state of another button.

At the intended 1 ms TIM2 input tick, five samples provide a nominal 5 ms debounce interval. The actual interval is `5 * input update period`; it changes if the timer configuration changes.

## State Fields

| Field | Meaning |
| --- | --- |
| `raw` | Latest GPIO sample; active-high logical pressed mask; not debounced. |
| `current` | Stable debounced pressed mask. Send this as the controller's held-button state. |
| `previous` | `current` from the preceding update. |
| `pressed` | Bits that changed from released to pressed during this update. |
| `released` | Bits that changed from pressed to released during this update. |
| `debounce_history` | Five most recent raw samples used to decide stable state. |
| `debounce_index` | Ring-buffer position for the next raw sample. |

The edge masks are calculated after debounce:

```c
pressed  = current & ~previous;
released = ~current & previous;
```

Consequently, a held button remains set in `current`, but appears in `pressed` only for the single update where its debounced press is accepted. The corresponding behavior applies to `released`.

## Startup Behavior

Initialize the state to zero before the first update:

```c
input_state_t inputs = {0};
```

This represents all buttons released. A button held during boot is reported as pressed after it has appeared in five consecutive samples. No raw bouncing value is exposed through `current`, `pressed`, or `released`.

## Sending Input

When communicating with the main console controller, send the debounced `current` mask at a regular report interval. Repeating state reports makes the system recover naturally from a dropped message.

Use `pressed` and `released` for local actions that require an edge, such as opening a menu or toggling a setting. If every edge must be delivered over an unreliable transport, add an acknowledged event queue; a state report alone cannot prove that a short press was received.

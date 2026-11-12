# Timer Architecture

## Goal

The timer system should provide a general-purpose timebase for the firmware rather than making TIM2 responsible for a specific periodic event.

The architecture is:

```text
Application modules
       │
       ▼
Software timers
       │
       ▼
 time_now_ms()
       │
       ▼
      TIM2
       │
       ▼
STM32 hardware
```

TIM2 provides a continuously increasing millisecond timebase. Software timer structs use that timebase to determine whether a requested duration has elapsed.

---

## Why use a free-running timebase?

The current approach treats TIM2 as a 1 ms event source:

```c
if (timer_event_elapsed())
{
    read_GPIO_input_pins(&inputs);
    update_input_state(&inputs);
}
```

This works, but it ties the application directly to a particular timer event.

A free-running timebase is more flexible:

- USB can request a 25 ms delay.
- Input handling can use a 1 ms or 10 ms interval.
- A display task could use a different interval.
- LED blinking could use another interval.
- All of these can use the same TIM2 hardware peripheral.

The application only needs to ask:

> Has this duration elapsed?

It does not need to know about TIM2 registers, prescalers, ARR, or UIF.

---

## Software timer structure

A simple software timer can contain a deadline:

```c
typedef struct
{
    uint32_t deadline_ms;
} timer_t;
```

The struct is very small: one `uint32_t`, so it requires 4 bytes of RAM.

For example:

```c
timer_t input_timer;
timer_t usb_timer;
```

requires only 8 bytes total.

The number of software timers is therefore not a significant memory concern on the STM32U575. The main reason to use the structs is clean separation and reusable timing logic.

---

## Timer API

A simple API could be:

```c
void timer_init(void);

uint32_t time_now_ms(void);

void timer_start(timer_t *timer, uint32_t duration_ms);

bool timer_expired(const timer_t *timer);
```

### `timer_init()`

Initializes TIM2 and starts the hardware timebase.

TIM2 should run continuously rather than generating a special 1 ms application event.

### `time_now_ms()`

Returns the current monotonic millisecond count.

Application code should use this instead of reading TIM2 directly.

### `timer_start()`

Records a future deadline:

```text
deadline = current_time + duration
```

For example:

```c
timer_start(&usb_timer, 25);
```

means:

> `usb_timer` will expire 25 ms from now.

### `timer_expired()`

Checks whether the current time has reached the timer's deadline.

Conceptually:

```text
current_time >= deadline
```

---

# Current `main.c`

The current firmware looks like:

```c
#include "gpio.h"
#include "inputs.h"
#include "timer.h"
#include "usb.h"


int main(void)
{
    init_GPIO();
    init_timer();
    init_usb();

    input_state_t inputs = {0};

    for (;;) {
        if (timer_event_elapsed()) {
            read_GPIO_input_pins(&inputs);
            update_input_state(&inputs);
        }
    }

    return 0;
}
```

The timer architecture can evolve without changing the responsibilities of the other modules.

---

# Proposed `main.c`

Define the input update period:

```c
#define INPUT_UPDATE_PERIOD_MS    1UL
```

Then create a software timer:

```c
int main(void)
{
    init_GPIO();
    init_timer();
    init_usb();

    input_state_t inputs = {0};

    timer_t input_timer = {0};

    timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);

    for (;;)
    {
        if (timer_expired(&input_timer))
        {
            read_GPIO_input_pins(&inputs);
            update_input_state(&inputs);

            timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);
        }
    }

    return 0;
}
```

The important difference is that `main.c` no longer cares that TIM2 is being used.

It only knows:

```c
timer_start(...)
timer_expired(...)
```

---

# Multiple timers

Multiple software timers can coexist on the same hardware timebase:

```c
timer_t input_timer;
timer_t usb_timer;
timer_t display_timer;
```

Conceptually:

```text
                    TIM2
                     │
                     ▼
              system_time_ms
                     │
          ┌──────────┼──────────┐
          ▼          ▼          ▼
     input_timer  usb_timer  display_timer
```

Each timer simply stores its own deadline.

For example:

```text
input_timer
    deadline = 1001 ms

usb_timer
    deadline = 1025 ms

display_timer
    deadline = 1050 ms
```

All three are driven by the same hardware clock.

---

# USB example

USB may need to wait at least 25 ms after forcing device mode.

Instead of implementing a USB-specific hardware timer, USB can create a software timer:

```c
timer_t usb_timer;

timer_start(&usb_timer, 25);
```

Then later:

```c
if (timer_expired(&usb_timer))
{
    // Continue USB initialization
}
```

USB does not need to know anything about:

```text
TIM2
TIM2_PSC
TIM2_ARR
TIM2_SR
TIM2_UIF
```

Those belong to the timer hardware layer.

---

# Separation of responsibilities

The intended architecture is:

```text
main.c
 │
 ├── input handling
 │      └── input_timer
 │
 └── USB handling
        └── usb_timer
               │
               ▼
          timer.c / timer.h
               │
               ▼
              TIM2
```

### `timer.c`

Owns:

- TIM2 initialization
- TIM2 configuration
- system timekeeping
- software timer implementation

### `input.c`

Owns:

- GPIO input interpretation
- debouncing
- logical button state

It should not configure TIM2.

### `usb.c`

Owns:

- USB peripheral initialization
- USB device state
- HID functionality

It should not configure TIM2.

### `main.c`

Owns:

- initialization order
- main application loop
- coordinating the modules

---

# Why professionals use this pattern

The important architectural principle is:

> One hardware timebase can provide timing services to many parts of the application.

This gives:

1. **Hardware abstraction**

   Application modules don't manipulate timer registers.

2. **Hardware efficiency**

   Multiple software timers can share one hardware timer.

3. **Scalability**

   Adding another timed operation doesn't require another hardware peripheral.

4. **Non-blocking behavior**

   Code can check whether a timer has expired instead of sitting inside a delay loop.

5. **Clear ownership**

   TIM2 belongs to the time service, not to USB or input handling.

6. **Portability**

   If TIM2 is eventually replaced by another hardware timer, the rest of the firmware should not need to know.

---

# Important design principle

The software timer should not itself be the clock.

The hierarchy should be:

```text
TIM2
  ↓
monotonic system time
  ↓
software timers
  ↓
application behavior
```

TIM2 answers:

> What time is it?

A software timer answers:

> Has my requested duration elapsed?

The application answers:

> What should I do now?

Keeping these responsibilities separate will make the firmware easier to extend as the Game GIRL project grows.

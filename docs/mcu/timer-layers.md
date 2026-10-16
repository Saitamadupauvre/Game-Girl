# Firmware Timer Layers

## Purpose

Firmware components need timeouts and periodic work without depending on TIM2
registers. This design keeps one hardware clock and gives each component its
own software deadline.

```text
Application modules
        |
        v
Software deadlines
        |
        v
Timer service
        |
        v
TIM2 hardware driver
```

The timer service is non-blocking. No component should spin in a delay loop
while waiting for a deadline.

## Layer Responsibilities

### TIM2 hardware driver

The driver owns RCC and TIM2 configuration. It provides a continuously running
millisecond counter:

- Enable `RCC_APB1ENR1.TIM2EN`.
- Configure TIM2 to increment once per millisecond.
- Set `TIM2_ARR` to `UINT32_MAX`.
- Start TIM2 once during boot and do not reset it for application events.
- Read `TIM2_CNT` to obtain the current millisecond value.

For a known `TIM2CLK`, choose the prescaler so that:

```text
counter frequency = TIM2CLK / (PSC + 1) = 1000 Hz
```

The current firmware's clock configuration must explicitly establish, or make
available, `TIM2CLK`; hard-coding a prescaler is valid only while that clock is
fixed and documented.

### Timer service

The service exposes time and software deadlines. It does not know whether a
deadline is used by input, USB, or another module.

```c
typedef struct {
    uint32_t deadline_ms;
} timer_t;

void timer_init(void);
uint32_t time_now_ms(void);
void timer_start(timer_t *timer, uint32_t duration_ms);
bool timer_expired(const timer_t *timer);
```

`timer_init()` configures and starts the TIM2 timebase. It must run once before
any other timer function.

`time_now_ms()` returns the current TIM2 counter value. It is a monotonically
increasing value modulo `UINT32_MAX + 1`.

`timer_start()` resets only the supplied software timer:

```c
timer->deadline_ms = time_now_ms() + duration_ms;
```

`timer_expired()` is a level check: once a deadline is reached, it returns
`true` until that software timer is restarted. It must use signed subtraction,
not `now >= deadline`, so it works across the hardware counter wrap:

```c
bool timer_expired(const timer_t *timer)
{
    return (int32_t)(time_now_ms() - timer->deadline_ms) >= 0;
}
```

Requested durations must be no greater than `INT32_MAX` milliseconds. This is
about 24.8 days and is far beyond the expected firmware timeouts.

### Application modules

Each module owns its `timer_t` instances and decides what work to perform on
expiry. Restarting a timer never changes TIM2 or another module's deadline.

```c
timer_t input_timer = {0};

timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);

for (;;) {
    if (timer_expired(&input_timer)) {
        read_GPIO_input_pins(&inputs);
        update_input_state(&inputs);
        timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);
    }

    usb_poll();
}
```

This is the intended interpretation of resetting a software timer after it
expires. It starts the next interval after the work has completed. The interval
therefore includes the small amount of work performed in the expiry branch.
For input debouncing this is acceptable as long as its required sampling period
is a minimum interval rather than an exact schedule.

If a future task needs a fixed cadence, its deadline should instead be advanced
from its previous deadline by its period. That policy should be added as a
separate API only when a caller requires it.

## Migration From The Current Driver

1. Replace `enable_hardware_timer_clocks()` with `timer_init()`.
2. Correctly enable `RCC_APB1ENR1_TIM2EN` during initialization.
3. Configure TIM2 as the free-running 1 kHz counter instead of using `UIF` as
   a one-millisecond application event.
4. Remove `timer_event_elapsed()` after its users have switched to software
   deadlines.
5. Use one `timer_t` in `main.c` for input sampling and restart it after each
   sample.
6. Let USB maintain its own timer and state machine for required delays; USB
   must not reset the shared TIM2 counter or block the main loop.

## Limits And Guarantees

- TIM2 wraps naturally after approximately 49.7 days at a 1 kHz counter rate.
- Wrap-safe deadline comparison preserves correct expiry behaviour across that
  wrap for durations up to `INT32_MAX` milliseconds.
- The timer service does not count missed periodic jobs. If the main loop is
  delayed, an expired timer runs once when the loop resumes and is then
  restarted.
- This design requires no TIM2 update interrupt: reading the 32-bit TIM2
  counter supplies the timebase directly.

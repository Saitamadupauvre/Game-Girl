# STM32U575 TIM2 Hardware Clock

## Purpose

`mcu/src/drivers/timer.c` uses the STM32U575 TIM2 peripheral as a free-running, polling-based periodic timer. `enable_hardware_timer_clocks()` configures TIM2 and `timer_event_elapsed()` reports and acknowledges each update event.

This document describes the intended TIM2 configuration, its timing, and the corrections required for the driver to operate as intended.

## Hardware path

TIM2 is an APB1 timer. Its peripheral clock must be enabled through the RCC before its registers are accessed:

```text
RCC timer clock source
        |
        v
APB1 timer clock (TIM2CLK)
        |
        v
TIM2 prescaler (PSC)
        |
        v
TIM2 counter (CNT), counting from 0 to ARR
        |
        v
Update event and UIF status flag
```

TIM2 is a 32-bit general-purpose timer. This driver uses only its basic up-counting mode; no TIM2 pins, capture/compare channels, DMA, or NVIC interrupt are configured.

## Clock and Period Calculation

The current source assumes `TIM2CLK = 4 MHz`.

```c
TIM2_PSC = 3;
TIM2_ARR = 999;
```

The prescaler divides the timer input clock by `PSC + 1`:

```text
counter clock = TIM2CLK / (PSC + 1)
              = 4,000,000 / (3 + 1)
              = 1,000,000 Hz
```

The counter therefore advances once every 1 µs. In up-counting mode, an update event occurs after `ARR + 1` counts:

```text
update period = (PSC + 1) * (ARR + 1) / TIM2CLK
              = 4 * 1000 / 4,000,000
              = 1 ms
```

`timer_event_elapsed()` consequently returns `true` at most once per **1 ms timer update**, provided it is called frequently enough to observe `UIF`.

`PSC = 3` with `ARR = 999` is not a 1 µs period. A 1 µs update period at a 4 MHz TIM2 clock would require `PSC = 3` and `ARR = 0`.

## Register Sequence

The driver intends to perform this sequence:

1. Enable the TIM2 peripheral clock in `RCC_APB1ENR1.TIM2EN`.
2. Clear `TIM2_CR1.CEN` so the counter is stopped during setup.
3. Write `TIM2_PSC = 3` and `TIM2_ARR = 999`.
4. Set `TIM2_CNT = 0`.
5. Write `TIM2_EGR.UG = 1` to generate an update event. This reloads the prescaler setting immediately.
6. Clear `TIM2_SR.UIF`, because the forced update sets the update flag.
7. Set `TIM2_CR1.CEN` to begin counting.

When `CNT` reaches the auto-reload value, the timer wraps to zero, creates an update event, and sets `TIM2_SR.UIF`. `timer_event_elapsed()` reads this flag, clears it, and returns `true`.

Clearing `UIF` acknowledges the event only. It does not reset `CNT`; the timer hardware has already wrapped the counter during the update event.

## Required Driver Corrections

Before using this driver for real hardware timing, correct the clock-enable write in `mcu/src/drivers/timer.c`:

```c
/* Current code: reads and writes back the same register value. */
RCC_APB1ENR1 |= RCC_APB1ENR1;

/* Required: set the TIM2 clock-enable bit. */
RCC_APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
```

The current line does not set bit 0 when it is clear, so TIM2 may remain clock-gated and never count.

The assumed 4 MHz `TIM2CLK` must also be established and documented by the system clock configuration. TIM2's actual input frequency depends on the selected APB1 clock and its prescaler; when the APB prescaler is not 1, STM32 timer clocking can differ from the APB peripheral clock. Recalculate `PSC` and `ARR` whenever the RCC clock tree changes.

## Polling Contract and Limitations

- `timer_event_elapsed()` is an edge coalescing poll, not an event counter. If more than one 1 ms period elapses between calls, `UIF` is still only one bit and the missed periods cannot be recovered.
- It is suitable for periodic work that runs at least once per millisecond and can tolerate polling jitter.
- It does not enable `TIM2_DIER.UIE` or a TIM2 interrupt. Use an interrupt and a software tick counter when every elapsed period must be accounted for.
- It does not configure `TIM2_CR1.URS`, `TIM2_CR1.ARPE`, or one-pulse mode, so the peripheral reset defaults apply apart from `CEN`.

## Relevant Reference Manual Sections

Source: [RM0456: STM32U575/585 reference manual](https://www.st.com/resource/en/reference_manual/rm0456-stm32u575585-armbased-32bit-mcus-stmicroelectronics.pdf).

- RCC APB1 peripheral clock enable register 1 (`RCC_APB1ENR1`), including `TIM2EN`.
- General-purpose timer TIM2 register descriptions: `TIMx_CR1`, `TIMx_SR`, `TIMx_EGR`, `TIMx_CNT`, `TIMx_PSC`, and `TIMx_ARR`.
- Section 55.4 documents the TIM2 register fields, including the update flag, event-generation, prescaler, and auto-reload behavior used here.

# GPIO Button Driver

## Purpose

`mcu/src/drivers/gpio.c` configures the STM32U575 GPIO pins connected to the Game GIRL buttons and samples their state. It provides the hardware-facing input layer between the button wiring and `input_state_t.raw`.

The driver does not debounce buttons or generate press events. Those responsibilities belong to `mcu/src/services/inputs.c`.

## Electrical Convention

Every button is configured as an input with an internal pull-up resistor:

```text
released: GPIO input reads 1
pressed:  GPIO input reads 0
```

The switch connects its pin to ground when pressed. This is an active-low electrical signal.

`read_GPIO_input_pins()` converts this into an active-high logical button mask:

```text
GPIO pin = 0  ->  logical button bit = 1  ->  pressed
GPIO pin = 1  ->  logical button bit = 0  ->  released
```

All code after the GPIO driver therefore uses `1` to mean a pressed button, independent of the active-low board wiring.

## Pin Mapping

| Port | Pin | Button |
| --- | --- | --- |
| GPIOF | PF3 | L |
| GPIOF | PF4 | R |
| GPIOF | PF5 | Start |
| GPIOF | PF10 | Select |
| GPIOF | PF11 | Y |
| GPIOF | PF13 | A |
| GPIOF | PF14 | B |
| GPIOF | PF15 | X |
| GPIOE | PE7 | Up |
| GPIOE | PE8 | Down |
| GPIOE | PE9 | Left |
| GPIOE | PE11 | Right |

The logical bits are defined by `btn_t` in `mcu/include/inputs.h`.

## Initialization

Call these functions once before reading buttons:

```c
enable_GPIO_clocks();
set_GPIO_pins();
```

`enable_GPIO_clocks()` sets `RCC_AHB2ENR1.GPIOFEN` and `RCC_AHB2ENR1.GPIOEEN`. `set_GPIO_pins()` clears the selected pins' `MODER` fields to input mode (`00`) and writes `01` to their `PUPDR` fields to select internal pull-ups.

## Sampling Contract

```c
read_GPIO_input_pins(&inputs);
```

The function reads `GPIOF_IDR` and `GPIOE_IDR` once each, then builds a 16-bit logical pressed mask in `inputs.raw`. Sampling each port once prevents different buttons on the same port from being taken from different register reads.

`raw` is an instantaneous, un-debounced state. Call `update_input_state(&inputs)` immediately after it to obtain the debounced state and edge masks.

## Relevant STM32 Registers

- `RCC_AHB2ENR1`: enables GPIOE and GPIOF peripheral clocks.
- `GPIOx_MODER`: selects input mode for each pin.
- `GPIOx_PUPDR`: selects the internal pull-up.
- `GPIOx_IDR`: returns the sampled input level.

Source: [RM0456: STM32U575/585 reference manual](https://www.st.com/resource/en/reference_manual/rm0456-stm32u575585-armbased-32bit-mcus-stmicroelectronics.pdf).

# Boardless Driver Tests

## Run

The host test does not access STM32 hardware registers. Configure and run it from the repository root:

```sh
cmake -S . -B build
cmake --build build --target gamegirl-input-tests
ctest --test-dir build --output-on-failure
```

## Coverage

`gamegirl-input-tests` verifies the logic that can run without a board:

- GPIO active-low values are translated to the expected active-high `BTN_*` bits.
- No GPIO pins asserted produces an empty raw mask.
- All configured GPIO pins asserted produces the full button mask.
- A press is accepted only after five consecutive samples.
- A bounce sequence does not create a release event.
- A release is accepted only after five consecutive samples.
- `pressed` and `released` are asserted for one update at the debounced state transition.

The test injects synthetic GPIOE/GPIOF samples into `decode_gpio_input_pins()`. The production `read_GPIO_input_pins()` still obtains these samples from `GPIOE_IDR` and `GPIOF_IDR`.

## Hardware Checks Still Required

Boardless tests cannot prove electrical behavior. When hardware is available, verify:

- Each button is connected to the documented GPIO pin and pulls the pin low when pressed.
- Internal pull-ups produce a high level when each button is released.
- The configured GPIOE and GPIOF clocks are enabled.
- TIM2 runs at the assumed input-clock frequency and produces the expected input update interval.
- No button produces repeated press/release events while held or while its contacts bounce.

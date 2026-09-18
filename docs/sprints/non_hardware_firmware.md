# Non-Hardware Firmware Sprint

## Goal

Implement STM32 firmware features that can be built and tested on a host machine before the Nucleo board and peripherals are available.

## Scope

### Input model and debounce

- [ ] Define the button bitmask for D-pad, face, shoulder, Start, Select, and Menu buttons.
- [ ] Define an `input_state_t` input representation.
- [ ] Implement a debounce state machine using sampled active-low GPIO values and timestamps.
- [ ] Unit-test press, release, bounce, and simultaneous-button cases.

Acceptance: a stable input state is accepted only after 5 to 10 ms, using simulated GPIO samples.

### USB HID gamepad data

- [ ] Define the USB HID gamepad report layout.
- [ ] Implement conversion from `input_state_t` to a HID report.
- [ ] Add the HID report descriptor.
- [ ] Unit-test report packing for every button.

Acceptance: all Game GIRL controls map to the expected HID report bits.

### USB status protocol

- [ ] Define shared command identifiers for battery, charger, rumble, LEDs, firmware version, hardware version, and shutdown.
- [ ] Define request, response, and `gamegirl_status_t` packet formats.
- [ ] Implement command parsing and response serialization.
- [ ] Unit-test valid, malformed, truncated, and unknown commands.

Acceptance: host-side tests decode every valid command and reject invalid packets safely.

### Battery telemetry logic

- [ ] Define MAX17043 register addresses and raw register conversion functions.
- [ ] Convert simulated VCELL readings to millivolts.
- [ ] Convert simulated SOC readings to battery percentages.
- [ ] Implement low-battery and critical-battery policy states.
- [ ] Unit-test conversion boundary values and battery-state transitions.

Acceptance: simulated MAX17043 register data produces correct battery status without I2C hardware.

### Power and shutdown state machine

- [ ] Implement power-button hold detection using timestamps.
- [ ] Define normal, shutdown-requested, waiting-for-host, and shutdown-complete states.
- [ ] Emit an abstract shutdown request for the Raspberry Pi.
- [ ] Unit-test short press, held press, host acknowledgement, timeout, and critical-battery shutdown.

Acceptance: a shutdown is requested only after the configured hold duration or a critical battery event.

### LED and rumble policies

- [ ] Define abstract LED states for power, charging, low battery, and debug status.
- [ ] Define abstract rumble requests and duration handling.
- [ ] Unit-test policy decisions from simulated system events.

Acceptance: services produce output commands without directly accessing GPIO or PWM registers.

## Build Infrastructure

- [ ] Add an ARM cross-compilation toolchain file.
- [ ] Add host-build targets for service unit tests.
- [ ] Add STM32 firmware build targets for ELF, BIN, and map files.
- [ ] Add placeholder OpenOCD targets for flash, reset, and erase.
- [ ] Add compiler warnings: `-Wall -Wextra -Werror`.

Acceptance: host tests and the cross-compiled firmware build complete without physical hardware.

## Hardware-Dependent Work To Defer

- GPIO electrical validation and pin mapping.
- Clock-tree validation and timer interrupt behavior.
- USB enumeration and HID recognition by Linux.
- I2C bus communication with the MAX17043.
- LED brightness, rumble motor drive, and power-button wiring.
- OpenOCD flashing, reset, and ST-LINK debugging.

## Recommended Order

1. Shared protocol and input types.
2. Debouncer and unit tests.
3. HID report generation and descriptor.
4. Battery conversion and battery policy.
5. Power/shutdown state machine.
6. Host-side test target and STM32 cross-build setup.

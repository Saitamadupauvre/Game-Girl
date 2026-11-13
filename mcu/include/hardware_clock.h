
#ifndef HARDWARE_CLOCK_H
    #define HARDWARE_CLOCK_H

    #include "fault.h"

    #include <stdint.h>

// Raises VCORE to range 3, then MSIS (the system clock) from 4 MHz to 24 MHz.
// Call before init_timer(). Returns FAULT_NONE, or the FAULT_CLOCK_* of the step that timed out:
// the chip then stays at 4 MHz, which is safe for everything except USB.
fault_t init_hardware_clock(void);

// HCLK = APB1 = TIM2 input clock in Hz: 24 MHz after a successful init_hardware_clock(), else 4 MHz.
uint32_t hardware_clock_hz(void);

#endif

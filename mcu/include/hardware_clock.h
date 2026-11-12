
#ifndef HARDWARE_CLOCK_H
    #define HARDWARE_CLOCK_H

    #include <stdbool.h>
    #include <stdint.h>

// Raises VCORE to range 3, then MSIS (the system clock) from 4 MHz to 24 MHz.
// Call before init_timer(). Returns false if a step times out: the chip then stays at 4 MHz, which
// is safe for everything except USB.
bool init_hardware_clock(void);

// HCLK = APB1 = TIM2 input clock in Hz: 24 MHz after a successful init_hardware_clock(), else 4 MHz.
uint32_t hardware_clock_hz(void);

#endif

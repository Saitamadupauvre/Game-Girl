
#include "fault.h"
#include "gpio.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer.h"
#include "usb.h"


/*
 * Non tasks are iniated here. Either they fail and use a fallback (hardware clock speed) or they work.
 * USB start up code is found in usb_task in /src/services/usb_task.c
 */

static void init_system(void)
{
    // init_timer() reads the clock speed it sets. On failure the chip stays at 4 MHz, on succes it goes up to 24Mhz
    const fault_t clock_fault = init_hardware_clock();
    if (clock_fault != FAULT_NONE) {
        fault_log(clock_fault);
    }

    init_GPIO();
    init_timer();
}

int main(void)
{
    init_system();

    for (;;) {
        input_task();
        usb_task();
    }

    return 0;
}

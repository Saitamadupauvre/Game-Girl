
#include "fault.h"
#include "gpio.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer.h"
#include "usb.h"
#include "watchdog.h"
#include "backup.h"


/*
 * Non tasks are iniated here. Either they fail and use a fallback (hardware clock speed) or they work.
 * USB start up code is found in usb_task in /src/services/usb_task.c
 */

static void init_system(void)
{
    // init_timer() reads the clock speed it sets. On failure the chip stays at 4 MHz, on succes it goes up to 24Mhz
    fault_t init_fault = init_hardware_clock();
    if (init_fault != FAULT_NONE) {
        fault_log(init_fault);
        fault_set_failure(init_fault);     // 4 MHz fallback lasts until the next reset
    }

    init_backup();
    init_GPIO();
    init_timer();

    init_fault = init_watchdog();
    if (init_fault != FAULT_NONE) {
        fault_log(init_fault);
    }
}

int main(void)
{
    init_system();

    for (;;) {
        kick_watchdog();
        input_task();
        usb_task();
    }

    return 0;
}

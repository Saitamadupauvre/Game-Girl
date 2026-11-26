
#include "fault.h"
#include "gpio.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "led.h"
#include "timer.h"
#include "usb.h"
#include "watchdog.h"
#include "backup.h"
#include "reset_guard.h"


/*
 * Non tasks are iniated here. Either they fail and use a fallback (hardware clock speed) or they work.
 * USB start up code is found in usb_task in /src/services/usb_task.c
 */

static void handle_tasks(void)
{
    breadcrumb_set(FAULT_SUBSYS_INPUT);
    input_task();

    breadcrumb_set(FAULT_SUBSYS_USB);
    if (reset_guard_enabled(FAULT_SUBSYS_USB)) {
        usb_task();
    }

    breadcrumb_set(FAULT_SUBSYS_NONE);
    led_task();     // Never disabled: a hang here counts under SYS
}

static void init_system(void)
{
    init_backup();      // First: fault_log() saves the last fault to a backup register, even a clock fault

    // init_timer() reads the clock speed it sets. On failure the chip stays at 4 MHz, on succes it goes up to 24Mhz
    fault_t init_fault = init_hardware_clock();
    if (init_fault != FAULT_NONE) {
        fault_log(init_fault);
        fault_set_failure(init_fault);     // 4 MHz fallback lasts until the next reset
    }

    init_GPIO();
    init_timer();
    init_reset_guard();

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
        reset_guard_task();
        handle_tasks();
    }

    return 0;
}

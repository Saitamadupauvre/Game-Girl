/**
  ******************************************************************************
  * @file    main.c
  * @brief   Firmware entry point: start-up, then the superloop.
  *
  *          This file provides functions to:
  *           + Initialise the drivers and services in order
  *           + Run the tasks forever, one pass at a time
  ******************************************************************************
  *
  * Start-up order: backup domain first (so even a clock fault is saved), clock, GPIO, timer,
  * reset guard, then the watchdog last. A failed init does not stop the boot: the module falls
  * back (the clock stays at 4 MHz) and the fault is logged. USB is not started here but by
  * usb_task(), which retries on its own.
  *
  * Superloop: kick the watchdog, run the reset guard, then each task. No task blocks. Before each
  * task a breadcrumb names the subsystem, so after a watchdog reset reset_guard.c knows which one
  * hung and can disable it (USB only).
  ******************************************************************************
  */

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

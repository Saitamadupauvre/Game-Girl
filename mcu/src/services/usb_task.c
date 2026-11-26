/**
  ******************************************************************************
  * @file    usb_task.c
  * @brief   USB policy: start-up retries, report sending and soft replug.
  *
  *          This file provides functions to:
  *           + Start USB with retries, then give up without stopping the main loop
  *           + Send the button report while USB runs
  *           + Detect a stalled host and soft replug
  ******************************************************************************
  *
  * State machine: STARTING -> RUNNING, or BACKOFF -> STARTING again, or FAILED. usb.c does the
  * register work; this file decides when.
  *
  * Start-up: USB needs 24 MHz, so after a clock fault it goes straight to FAILED. Otherwise up to
  * USB_START_ATTEMPTS tries of init_usb(), with hard_reset_usb() and USB_RETRY_BACKOFF_MS after
  * each failure. Every failure is logged; the last one is also set active.
  *
  * Soft replug: while the bus is active and something is pending (enumeration or a report), the
  * host must make progress within USB_HOST_STALL_MS. If not, FAULT_USB_HOST_LOST is logged and the
  * pull-up is turned off for USB_REPLUG_OFF_MS so the host enumerates again. After
  * USB_REPLUG_LIMIT replugs without a clean run, USB is reset and left FAILED. A suspended bus
  * never counts as a stall.
  *
  * Tests with usb.c faked: tests/test_usb_task.c.
  ******************************************************************************
  */

#include "usb.h"
#include "fault.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer_api.h"

#include <stdint.h>

#define USB_REQUIRED_HCLK_HZ    24000000UL  // usb.c sets TRDT for 24 MHz, and range 3 is needed anyway
#define USB_START_ATTEMPTS      3u
#define USB_RETRY_BACKOFF_MS    100UL

#define USB_HOST_STALL_MS       2000UL
#define USB_REPLUG_OFF_MS       100UL
#define USB_REPLUG_LIMIT        3u

typedef enum {
    USB_TASK_STARTING,  // Next call runs init_usb()
    USB_TASK_BACKOFF,   // An attempt failed, waiting before the next one
    USB_TASK_RUNNING,   // Core is up, reports are sent
    USB_TASK_REPLUG,    // Soft-disconnected, waiting before reconnecting
    USB_TASK_FAILED,    // Gave up, the rest of the firmware keeps running
} usb_task_state_t;

static usb_task_state_t state = USB_TASK_STARTING;
static uint8_t attempts;
static deadline_t backoff_timer;
static deadline_t stall_timer;
static uint32_t last_activity;
static uint8_t replugs;

#ifdef GAME_GIRL_HOST_TEST
void usb_task_reset_test(void)
{
    state = USB_TASK_STARTING;
    attempts = 0;
    backoff_timer = (deadline_t){0};
    stall_timer = (deadline_t){0};
    last_activity = 0;
    replugs = 0;
}
#endif

static void restart_stall_watch(void)
{
    last_activity = usb_host_activity();
    timer_start(&stall_timer, USB_HOST_STALL_MS);
}

static void usb_task_start(void)
{
    // No logging fault here, it fails in init_system()
    if (hardware_clock_hz() != USB_REQUIRED_HCLK_HZ) {
        state = USB_TASK_FAILED;
        return;
    }

    attempts++;
    const fault_t fault = init_usb();
    if (fault == FAULT_NONE) {
        fault_clear_failure(FAULT_SUBSYS_USB);
        restart_stall_watch();
        state = USB_TASK_RUNNING;
        return;
    }

    fault_log(fault);   // Every attempt is logged, so the USB counter shows how many failed
    hard_reset_usb();

    if (attempts >= USB_START_ATTEMPTS) {
        fault_set_failure(fault);
        state = USB_TASK_FAILED;
        return;
    }

    timer_start(&backoff_timer, USB_RETRY_BACKOFF_MS);
    state = USB_TASK_BACKOFF;
}

// Replug only if the bus is active, the device waits on the host, and the host stalls
static void usb_task_watch_host(void)
{
    const bool waiting = usb_waiting_for_host();

    if (!waiting || (usb_host_activity() != last_activity)) {
        if (!waiting && usb_is_configured()) {
            replugs = 0;
        }
        restart_stall_watch();
        return;
    }

    if (!timer_expired(&stall_timer)) {
        return;
    }

    fault_log(FAULT_USB_HOST_LOST);

    if (replugs >= USB_REPLUG_LIMIT) {
        hard_reset_usb();
        fault_set_failure(FAULT_USB_HOST_LOST);
        state = USB_TASK_FAILED;
        return;
    }

    replugs++;
    usb_soft_disconnect();
    timer_start(&backoff_timer, USB_REPLUG_OFF_MS);
    state = USB_TASK_REPLUG;
}

void usb_task(void)
{
    switch (state) {
    case USB_TASK_STARTING:
        usb_task_start();
        break;

    case USB_TASK_BACKOFF:
        if (timer_expired(&backoff_timer)) {
            state = USB_TASK_STARTING;
        }
        break;

    case USB_TASK_RUNNING:
        usb_hid_send_if_changed(inputs_get());
        usb_task_watch_host();
        break;

    case USB_TASK_REPLUG:
        if (timer_expired(&backoff_timer)) {
            usb_soft_connect();
            restart_stall_watch();
            state = USB_TASK_RUNNING;
        }
        break;

    case USB_TASK_FAILED:
    default:
        break;
    }
}

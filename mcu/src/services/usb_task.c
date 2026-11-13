
#include "usb.h"
#include "fault.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer_api.h"

#include <stdint.h>


#define USB_REQUIRED_HCLK_HZ    24000000UL  // usb.c sets TRDT for 24 MHz, and range 3 is needed anyway
#define USB_START_ATTEMPTS      3u
#define USB_RETRY_BACKOFF_MS    100UL

typedef enum {
    USB_TASK_STARTING,  // Next call runs init_usb()
    USB_TASK_BACKOFF,   // An attempt failed, waiting before the next one
    USB_TASK_RUNNING,   // Core is up, reports are sent
    USB_TASK_FAILED,    // Gave up, the rest of the firmware keeps running
} usb_task_state_t;

static usb_task_state_t state = USB_TASK_STARTING;
static uint8_t attempts;
static deadline_t backoff_timer;

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
        state = USB_TASK_RUNNING;
        return;
    }

    fault_log(fault);   // Every attempt is logged, so the USB counter shows how many failed
    hard_reset_usb();

    if (attempts >= USB_START_ATTEMPTS) {
        state = USB_TASK_FAILED;
        return;
    }

    timer_start(&backoff_timer, USB_RETRY_BACKOFF_MS);
    state = USB_TASK_BACKOFF;
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
        break;

    case USB_TASK_FAILED:
    default:
        break;
    }
}

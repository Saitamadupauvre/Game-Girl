
#include "usb.h"
#include "fault.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer_api.h"

#include <stdint.h>


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


#ifndef USB_H
    #define USB_H

    #include "fault.h"
    #include "inputs.h"

    #include <stdint.h>

    #define USB_CORE_TIMEOUT_MS     10UL

    #define USB_REQUIRED_HCLK_HZ    24000000UL  // usb.c sets TRDT for 24 MHz, and range 3 is needed anyway
    #define USB_START_ATTEMPTS      3u
    #define USB_RETRY_BACKOFF_MS    100UL

typedef enum {
    USB_REPLY_DATA,
    USB_REPLY_ZLP,
    USB_REPLY_STALL
} usb_reply_type_t;

typedef enum {
    USB_TASK_STARTING,  // Next call runs init_usb()
    USB_TASK_BACKOFF,   // An attempt failed, waiting before the next one
    USB_TASK_RUNNING,   // Core is up, reports are sent
    USB_TASK_FAILED,    // Gave up, the rest of the firmware keeps running
} usb_task_state_t;

typedef struct {
    usb_reply_type_t type;
    const uint8_t *data;
    uint16_t len;
} usb_reply_t;

fault_t init_usb(void);

// Non-blocking: queues a HID report on EP1 when the buttons changed.
void usb_hid_send_if_changed(const input_state_t *inputs);

void OTG_FS_IRQHandler(void);

    /*USB_TASK.C*/

// Non-blocking: brings the USB core up (retrying with backoff), then sends HID reports from inputs_get().
// Needs init_hardware_clock(), init_GPIO() and init_timer() first.
void usb_task(void);

#endif

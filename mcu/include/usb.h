
#ifndef USB_H
    #define USB_H

    #include "fault.h"
    #include "inputs.h"

    #include <stdint.h>

typedef enum {
    USB_REPLY_DATA,
    USB_REPLY_ZLP,
    USB_REPLY_STALL
} usb_reply_type_t;

typedef struct {
    usb_reply_type_t type;
    const uint8_t *data;
    uint16_t len;
} usb_reply_t;

fault_t init_usb(void);

// Puts OTG_FS back in its reset state (RCC OTGRST) and forgets the host's settings.
// Leaves the OTG IRQ masked until the next successful init_usb(). Wait before calling init_usb() again.
void hard_reset_usb(void);

// Non-blocking: queues a HID report on EP1 when the buttons changed.
void usb_hid_send_if_changed(const input_state_t *inputs);

void OTG_FS_IRQHandler(void);

    /*USB_TASK.C*/

// Non-blocking: brings the USB core up (retrying with backoff), then sends HID reports from inputs_get().
// Needs init_hardware_clock(), init_GPIO() and init_timer() first.
void usb_task(void);

#endif

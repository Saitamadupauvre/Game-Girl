
#ifndef USB_H
    #define USB_H

    #include "inputs.h"

    #include <stdint.h>

    #define USB_CORE_TIMEOUT_MS     10UL

typedef enum { USB_REPLY_DATA, USB_REPLY_ZLP, USB_REPLY_STALL} usb_reply_type_t;

typedef struct {
    usb_reply_type_t type;
    const uint8_t *data;
    uint16_t len;
} usb_reply_t;

void init_usb(void);

// Non-blocking: queues a HID report on EP1 when the buttons changed.
void usb_hid_send_if_changed(const input_state_t *inputs);

void OTG_FS_IRQHandler(void);


#endif


#ifndef HID_H
    #define HID_H

    #include "usb.h"

    #include <stdint.h>

    #define HID_SETUP_SIZE      8u
    #define HID_REPORT_SIZE     2u


// byte 0: bit n = n-th button usage in report_desc (hid.c). byte 1: low nibble = hat switch, high nibble = 0.
typedef struct {
    uint8_t buttons;
    uint8_t hat;
} hid_report_t;

// Everything the control requests change. usb.c owns one instance, hid.c only reads and writes the copy it is given.
typedef struct {
    uint8_t address;                        // set by SET_ADDRESS
    uint8_t configuration;                  // 0 or 1, set by SET_CONFIGURATION
    uint8_t idle_rate;                      // set by SET_IDLE, 4 ms units, 0 = never resend
    uint8_t last_report[HID_REPORT_SIZE];   // for GET_REPORT, written by the main loop
} hid_state_t;


// Called on USB reset: address, configuration and idle rate go back to 0.
void hid_reset(hid_state_t *state);

// Packs the debounced buttons (btn_t bits) into the 2-byte report: 8 buttons plus the D-pad as a hat switch.
// Opposite D-pad directions cancel on their axis.
hid_report_t hid_pack(uint16_t current);

/*
 * Decides the answer to an 8-byte SETUP packet and updates *state.
 *
 * DATA replies are already clipped to wLength, and point at static storage or
 * at a field of *state, so they stay valid after this returns.
 */
usb_reply_t hid_handle_setup(const uint8_t setup[HID_SETUP_SIZE], hid_state_t *state);


#endif

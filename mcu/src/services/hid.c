
#include "hid.h"
#include "inputs.h"

#include <stddef.h>
#include <stdint.h>


enum {
    // bRequest, standard
    REQ_GET_STATUS = 0x00,
    REQ_SET_ADDRESS = 0x05,
    REQ_GET_DESCRIPTOR = 0x06,
    REQ_GET_CONFIGURATION = 0x08,
    REQ_SET_CONFIGURATION = 0x09,
    REQ_SET_INTERFACE = 0x0B,

    // bRequest, HID class
    REQ_GET_REPORT = 0x01,
    REQ_SET_IDLE = 0x0A,

    // bmRequestType: direction | type | recipient
    RT_DEVICE_OUT = 0x00,
    RT_DEVICE_IN = 0x80,
    RT_INTERFACE_OUT = 0x01,
    RT_INTERFACE_IN = 0x81,
    RT_ENDPOINT_IN = 0x82,
    RT_CLASS_INTERFACE_OUT = 0x21,
    RT_CLASS_INTERFACE_IN = 0xA1,

    // wValue >> 8 of GET_DESCRIPTOR
    DESC_DEVICE = 0x01,
    DESC_CONFIGURATION = 0x02,
    DESC_STRING = 0x03,
    DESC_HID = 0x21,
    DESC_REPORT = 0x22,

    DEVICE_DESC_SIZE = 18,
    CONFIG_DESC_SIZE = 34,
    HID_DESC_SIZE = 9,
    HID_DESC_OFFSET = 18,   // after the configuration and interface descriptors
    REPORT_DESC_SIZE = 55,

    STRING_MAX_CHARS = 31,
};

// VID/PID 0x1209:0x0001 is the pid.codes test pair, fine for development.
static const uint8_t device_desc[DEVICE_DESC_SIZE] = {
    0x12, 0x01,         // bLength, bDescriptorType = DEVICE
    0x00, 0x02,         // bcdUSB 2.00
    0x00, 0x00, 0x00,   // class, subclass, protocol = per interface
    0x40,               // bMaxPacketSize0 = 64
    0x09, 0x12,         // idVendor 0x1209
    0x01, 0x00,         // idProduct 0x0001
    0x00, 0x01,         // bcdDevice 1.00
    0x01, 0x02, 0x03,   // iManufacturer, iProduct, iSerialNumber
    0x01,               // bNumConfigurations
};

// Configuration + interface + HID + endpoint, sent as one block. The HID descriptor starts at offset 18.
static const uint8_t config_desc[CONFIG_DESC_SIZE] = {
    // Configuration
    0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    // Interface 0: HID, 1 endpoint, no boot protocol
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00,
    // HID 1.11, 1 report descriptor
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, REPORT_DESC_SIZE, 0x00,
    // Endpoint 1 IN, interrupt, 8 bytes, 1 ms
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x01,
};

/*
 * Standard Linux gamepad layout, named by position (Xbox style). Linux maps Button n of a Game Pad to
 * BTN_GAMEPAD + (n - 1). The DS diamond has A on the right and B at the bottom, so:
 *   B (bottom)=1 BTN_SOUTH, A (right)=2 BTN_EAST, X (top)=4 BTN_NORTH, Y (left)=5 BTN_WEST,
 *   L=7 BTN_TL, R=8 BTN_TR, Select=11 BTN_SELECT, Start=12 BTN_START
 * Button 13 (BTN_MODE) is free for a future Home button. The D-pad is a 4-bit hat switch
 * (0 = up, clockwise, 8 = released), followed by 4 padding bits.
 */
static const uint8_t report_desc[REPORT_DESC_SIZE] = {
    0x05, 0x01,     // Usage Page (Generic Desktop)
    0x09, 0x05,     // Usage (Game Pad)
    0xA1, 0x01,     // Collection (Application)
    0x05, 0x09,     //   Usage Page (Button)
    0x09, 0x01,     //   Usage (Button 1)
    0x09, 0x02,     //   Usage (Button 2)
    0x09, 0x04,     //   Usage (Button 4)
    0x09, 0x05,     //   Usage (Button 5)
    0x09, 0x07,     //   Usage (Button 7)
    0x09, 0x08,     //   Usage (Button 8)
    0x09, 0x0B,     //   Usage (Button 11)
    0x09, 0x0C,     //   Usage (Button 12)
    0x15, 0x00,     //   Logical Minimum (0)
    0x25, 0x01,     //   Logical Maximum (1)
    0x75, 0x01,     //   Report Size (1)
    0x95, 0x08,     //   Report Count (8)
    0x81, 0x02,     //   Input (Data, Var, Abs)              <- byte 0, 8 buttons
    0x05, 0x01,     //   Usage Page (Generic Desktop)
    0x09, 0x39,     //   Usage (Hat switch)
    0x15, 0x00,     //   Logical Minimum (0)
    0x25, 0x07,     //   Logical Maximum (7), 8 is out of range = null state
    0x75, 0x04,     //   Report Size (4)
    0x95, 0x01,     //   Report Count (1)
    0x81, 0x42,     //   Input (Data, Var, Abs, Null state)  <- byte 1 low nibble
    0x75, 0x04,     //   Report Size (4)
    0x95, 0x01,     //   Report Count (1)
    0x81, 0x03,     //   Input (Const, Var, Abs)             <- byte 1 high nibble
    0xC0,           // End Collection
};

// btn_t bit behind each button usage above, in usage order: report bit n is set while button_order[n] is held.
static const uint16_t button_order[8] = {
    BTN_B, BTN_A, BTN_X, BTN_Y, BTN_L, BTN_R, BTN_SELECT, BTN_START,
};

// Hat value indexed by [vertical + 1][horizontal + 1], where up and left are -1.
static const uint8_t hat_lut[3][3] = {
    {7, 0, 1},  // up:   up-left, up, up-right
    {6, 8, 2},  // none: left, released, right
    {5, 4, 3},  // down: down-left, down, down-right
};

static const char *const strings[] = {
    NULL,           // index 0 is the language ID list
    "Game Girl",    // 1: manufacturer
    "Game Girl",    // 2: product
    "000001",       // 3: serial number
};

#define STRING_COUNT    (sizeof(strings) / sizeof(strings[0]))

static const uint8_t lang_ids[4] = {0x04, 0x03, 0x09, 0x04};    // en-US
static const uint8_t zeros[2] = {0, 0};

// Reply buffer for string descriptors: 2 header bytes + UTF-16LE characters.
static uint8_t string_buf[2 + 2 * STRING_MAX_CHARS];

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static usb_reply_t reply_data(const uint8_t *data, uint16_t len, uint16_t w_length)
{
    usb_reply_t r = {USB_REPLY_DATA, data, (len < w_length) ? len : w_length};
    return r;
}

static usb_reply_t reply_zlp(void)
{
    usb_reply_t r = {USB_REPLY_ZLP, NULL, 0};
    return r;
}

static usb_reply_t reply_stall(void)
{
    usb_reply_t r = {USB_REPLY_STALL, NULL, 0};
    return r;
}

static usb_reply_t string_reply(uint8_t index, uint16_t w_length)
{
    if (index == 0u) {
        return reply_data(lang_ids, sizeof(lang_ids), w_length);
    }

    if (index >= STRING_COUNT) {
        return reply_stall();
    }

    const char *s = strings[index];
    uint8_t n = 0;
    for (; s[n] != '\0' && n < STRING_MAX_CHARS; n++) {
        string_buf[2 + 2 * n] = (uint8_t)s[n];  // ASCII only, so the high byte is 0
        string_buf[3 + 2 * n] = 0;
    }

    string_buf[0] = (uint8_t)(2 + 2 * n);
    string_buf[1] = DESC_STRING;
    return reply_data(string_buf, string_buf[0], w_length);
}

static usb_reply_t get_descriptor(uint8_t request_type, uint16_t w_value, uint16_t w_length)
{
    const uint8_t type = (uint8_t)(w_value >> 8);
    const uint8_t index = (uint8_t)(w_value & 0xFFu);

    if (request_type == RT_DEVICE_IN) {
        switch (type) {
        case DESC_DEVICE:
            return reply_data(device_desc, sizeof(device_desc), w_length);
        case DESC_CONFIGURATION:
            return reply_data(config_desc, sizeof(config_desc), w_length);
        case DESC_STRING:
            return string_reply(index, w_length);
        default:    // includes DEVICE_QUALIFIER, the right answer for a full-speed-only device
            return reply_stall();
        }
    }

    // RT_INTERFACE_IN
    switch (type) {
    case DESC_REPORT:
        return reply_data(report_desc, sizeof(report_desc), w_length);
    case DESC_HID:
        return reply_data(&config_desc[HID_DESC_OFFSET], HID_DESC_SIZE, w_length);
    default:
        return reply_stall();
    }
}

void hid_reset(hid_state_t *state)
{
    state->address = 0;
    state->configuration = 0;
    state->idle_rate = 0;
}

hid_report_t hid_pack(uint16_t current)
{
    hid_report_t report = {0};

    for (uint8_t i = 0; i < sizeof(button_order) / sizeof(button_order[0]); i++) {
        if ((current & button_order[i]) != 0u) {
            report.buttons |= (uint8_t)(1u << i);
        }
    }

    // Opposite directions cancel
    const int horizontal = ((current & BTN_RIGHT) != 0u) - ((current & BTN_LEFT) != 0u);
    const int vertical = ((current & BTN_DOWN) != 0u) - ((current & BTN_UP) != 0u);
    report.hat = hat_lut[vertical + 1][horizontal + 1];

    return report;
}

usb_reply_t hid_handle_setup(const uint8_t setup[HID_SETUP_SIZE], hid_state_t *state)
{
    const uint8_t request_type = setup[0];
    const uint8_t request = setup[1];
    const uint16_t w_value = read_u16(&setup[2]);
    const uint16_t w_length = read_u16(&setup[6]);

    switch ((uint16_t)(request_type << 8 | request)) {
    case (RT_DEVICE_IN << 8 | REQ_GET_DESCRIPTOR):
    case (RT_INTERFACE_IN << 8 | REQ_GET_DESCRIPTOR):
        return get_descriptor(request_type, w_value, w_length);

    case (RT_DEVICE_OUT << 8 | REQ_SET_ADDRESS):
        state->address = (uint8_t)(w_value & 0x7Fu);
        return reply_zlp();

    case (RT_DEVICE_OUT << 8 | REQ_SET_CONFIGURATION):
        if (w_value > 1u) {
            return reply_stall();
        }

        state->configuration = (uint8_t)w_value;
        return reply_zlp();

    case (RT_DEVICE_IN << 8 | REQ_GET_CONFIGURATION):
        return reply_data(&state->configuration, 1, w_length);

    // Bus powered, no remote wakeup: status is always 0 for device, interface and endpoint
    case (RT_DEVICE_IN << 8 | REQ_GET_STATUS):
    case (RT_INTERFACE_IN << 8 | REQ_GET_STATUS):
    case (RT_ENDPOINT_IN << 8 | REQ_GET_STATUS):
        return reply_data(zeros, sizeof(zeros), w_length);

    case (RT_CLASS_INTERFACE_OUT << 8 | REQ_SET_IDLE):
        state->idle_rate = (uint8_t)(w_value >> 8);
        return reply_zlp();

    case (RT_CLASS_INTERFACE_IN << 8 | REQ_GET_REPORT):
        return reply_data(state->last_report, sizeof(state->last_report), w_length);

    case (RT_INTERFACE_OUT << 8 | REQ_SET_INTERFACE):
        return (w_value == 0u) ? reply_zlp() : reply_stall();

    default:
        return reply_stall();
    }
}

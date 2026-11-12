#include "hid.h"
#include "inputs.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

#define HAT_RELEASED        8u

#define RT_DEVICE_OUT      0x00u
#define RT_DEVICE_IN        0x80u
#define RT_INTERFACE_IN     0x81u
#define RT_CLASS_OUT        0x21u

#define REQ_SET_ADDRESS     0x05u
#define REQ_GET_DESCRIPTOR  0x06u
#define REQ_SET_CONFIG      0x09u
#define REQ_SET_IDLE        0x0Au

static usb_reply_t setup(hid_state_t *state, uint8_t type, uint8_t request, uint16_t value, uint16_t length)
{
    const uint8_t pkt[HID_SETUP_SIZE] = {
        type, request,
        (uint8_t)(value & 0xFFu), (uint8_t)(value >> 8),
        0, 0,
        (uint8_t)(length & 0xFFu), (uint8_t)(length >> 8),
    };

    return hid_handle_setup(pkt, state);
}

static usb_reply_t get_desc(hid_state_t *state, uint8_t type, uint8_t desc, uint8_t index, uint16_t length)
{
    return setup(state, type, REQ_GET_DESCRIPTOR, (uint16_t)(desc << 8 | index), length);
}

static int test_pack(void)
{
    // Report bit n is the n-th button usage: Button 1, 2, 4, 5, 7, 8, 11, 12
    const struct { uint16_t btn; uint8_t report_bit; } buttons[] = {
        {BTN_B, 0}, {BTN_A, 1}, {BTN_X, 2}, {BTN_Y, 3},
        {BTN_L, 4}, {BTN_R, 5}, {BTN_SELECT, 6}, {BTN_START, 7},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        const hid_report_t r = hid_pack(buttons[i].btn);
        CHECK(r.buttons == (1u << buttons[i].report_bit));
        CHECK(r.hat == HAT_RELEASED);
    }

    hid_report_t r = hid_pack(0);
    CHECK(r.buttons == 0 && r.hat == HAT_RELEASED);

    r = hid_pack(BTN_A | BTN_B | BTN_X | BTN_Y | BTN_L | BTN_R | BTN_SELECT | BTN_START);
    CHECK(r.buttons == 0xFF && r.hat == HAT_RELEASED);

    return 0;
}

static int test_pack_hat(void)
{
    const struct { uint16_t dpad; uint8_t hat; } cases[] = {
        {BTN_UP, 0}, {BTN_UP | BTN_RIGHT, 1}, {BTN_RIGHT, 2}, {BTN_DOWN | BTN_RIGHT, 3},
        {BTN_DOWN, 4}, {BTN_DOWN | BTN_LEFT, 5}, {BTN_LEFT, 6}, {BTN_UP | BTN_LEFT, 7},
        {0, HAT_RELEASED},
        // Opposite directions cancel on their axis
        {BTN_UP | BTN_DOWN, HAT_RELEASED}, {BTN_LEFT | BTN_RIGHT, HAT_RELEASED},
        {BTN_UP | BTN_DOWN | BTN_RIGHT, 2}, {BTN_LEFT | BTN_RIGHT | BTN_DOWN, 4},
        {BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT, HAT_RELEASED},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const hid_report_t r = hid_pack(cases[i].dpad);
        CHECK(r.hat == cases[i].hat);
        CHECK(r.buttons == 0);  // the D-pad never leaks into the buttons
    }

    // Unused bits of btn_t never reach the report
    const hid_report_t r = hid_pack(0xF000u);
    CHECK(r.buttons == 0 && r.hat == HAT_RELEASED);

    return 0;
}

static int test_device_descriptor(void)
{
    hid_state_t s = {0};

    usb_reply_t r = get_desc(&s, RT_DEVICE_IN, 1, 0, 8);
    CHECK(r.type == USB_REPLY_DATA && r.len == 8);

    r = get_desc(&s, RT_DEVICE_IN, 1, 0, 18);
    CHECK(r.type == USB_REPLY_DATA && r.len == 18);
    CHECK(r.data[0] == 18 && r.data[1] == 1);

    r = get_desc(&s, RT_DEVICE_IN, 1, 0, 64);
    CHECK(r.type == USB_REPLY_DATA && r.len == 18);

    r = get_desc(&s, RT_DEVICE_IN, 6, 0, 10);   // device qualifier
    CHECK(r.type == USB_REPLY_STALL);

    return 0;
}

static int test_configuration_descriptor(void)
{
    hid_state_t s = {0};

    usb_reply_t r = get_desc(&s, RT_DEVICE_IN, 2, 0, 255);
    CHECK(r.type == USB_REPLY_DATA && r.len == 34);
    CHECK((uint16_t)(r.data[2] | (r.data[3] << 8)) == 34);  // wTotalLength

    r = get_desc(&s, RT_DEVICE_IN, 2, 0, 9);
    CHECK(r.len == 9);

    return 0;
}

static int test_string_descriptors(void)
{
    hid_state_t s = {0};

    usb_reply_t r = get_desc(&s, RT_DEVICE_IN, 3, 0, 255);
    const uint8_t lang[4] = {0x04, 0x03, 0x09, 0x04};
    CHECK(r.type == USB_REPLY_DATA && r.len == 4 && memcmp(r.data, lang, 4) == 0);

    r = get_desc(&s, RT_DEVICE_IN, 3, 2, 255);
    CHECK(r.type == USB_REPLY_DATA);
    CHECK(r.len == r.data[0] && r.data[1] == 3);
    CHECK(r.len >= 4 && (r.len % 2u) == 0u);
    for (uint16_t i = 3; i < r.len; i += 2u) {
        CHECK(r.data[i] == 0);   // ASCII in UTF-16LE
    }
    CHECK(r.data[2] == 'G');

    r = get_desc(&s, RT_DEVICE_IN, 3, 9, 255);
    CHECK(r.type == USB_REPLY_STALL);

    return 0;
}

static int test_hid_descriptors(void)
{
    hid_state_t s = {0};

    usb_reply_t r = get_desc(&s, RT_INTERFACE_IN, 0x22, 0, 255);
    CHECK(r.type == USB_REPLY_DATA && r.len == 55);
    CHECK(r.data[0] == 0x05 && r.data[54] == 0xC0);

    r = get_desc(&s, RT_INTERFACE_IN, 0x21, 0, 255);
    CHECK(r.type == USB_REPLY_DATA && r.len == 9);
    CHECK(r.data[1] == 0x21);
    CHECK((uint16_t)(r.data[7] | (r.data[8] << 8)) == 55);  // wDescriptorLength

    r = get_desc(&s, RT_INTERFACE_IN, 0x23, 0, 255);
    CHECK(r.type == USB_REPLY_STALL);

    return 0;
}

static int test_state_requests(void)
{
    hid_state_t s = {0};

    usb_reply_t r = setup(&s, RT_DEVICE_OUT, REQ_SET_ADDRESS, 5, 0);
    CHECK(r.type == USB_REPLY_ZLP && s.address == 5);

    r = setup(&s, RT_DEVICE_OUT, REQ_SET_CONFIG, 1, 0);
    CHECK(r.type == USB_REPLY_ZLP && s.configuration == 1);

    r = setup(&s, RT_DEVICE_IN, 0x08, 0, 1);    // GET_CONFIGURATION
    CHECK(r.type == USB_REPLY_DATA && r.len == 1 && r.data[0] == 1);

    r = setup(&s, RT_DEVICE_OUT, REQ_SET_CONFIG, 0, 0);
    CHECK(r.type == USB_REPLY_ZLP && s.configuration == 0);

    r = setup(&s, RT_DEVICE_OUT, REQ_SET_CONFIG, 2, 0);
    CHECK(r.type == USB_REPLY_STALL && s.configuration == 0);

    r = setup(&s, RT_CLASS_OUT, REQ_SET_IDLE, 0x1200u, 0);
    CHECK(r.type == USB_REPLY_ZLP && s.idle_rate == 0x12);

    s.last_report[0] = 0xAB;
    s.last_report[1] = 0x05;
    r = setup(&s, 0xA1, 0x01, 0x0100u, 2);      // GET_REPORT
    CHECK(r.type == USB_REPLY_DATA && r.len == 2 && r.data[0] == 0xAB && r.data[1] == 0x05);

    r = setup(&s, RT_DEVICE_IN, 0x00, 0, 2);    // GET_STATUS
    CHECK(r.type == USB_REPLY_DATA && r.len == 2 && r.data[0] == 0 && r.data[1] == 0);

    r = setup(&s, 0x40, 0x77, 0, 0);            // unknown
    CHECK(r.type == USB_REPLY_STALL);

    return 0;
}

static int test_reset(void)
{
    hid_state_t s = {.address = 5, .configuration = 1, .idle_rate = 9};

    hid_reset(&s);
    CHECK(s.address == 0 && s.configuration == 0 && s.idle_rate == 0);

    return 0;
}

int main(void)
{
    return test_pack()
        || test_pack_hat()
        || test_device_descriptor()
        || test_configuration_descriptor()
        || test_string_descriptors()
        || test_hid_descriptors()
        || test_state_requests()
        || test_reset();
}

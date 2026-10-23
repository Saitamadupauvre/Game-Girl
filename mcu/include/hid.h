

#ifndef HID_H
    #define HID_H

    #include <stdint.h>


typedef struct {
    uint8_t btn_low;
    uint8_t btn_high;
} hid_report_t;


#endif

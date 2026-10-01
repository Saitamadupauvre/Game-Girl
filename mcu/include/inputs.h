
#ifndef GG_INPUTS_H
    #define GG_INPUTS_H

    #include <stdint.h>

typedef enum {
    BTN_A      = 1u << 0,
    BTN_B      = 1u << 1,
    BTN_X      = 1u << 2,
    BTN_Y      = 1u << 3,

    BTN_UP     = 1u << 4,
    BTN_DOWN   = 1u << 5,
    BTN_LEFT   = 1u << 6,
    BTN_RIGHT  = 1u << 7,

    BTN_L      = 1u << 8,
    BTN_R      = 1u << 9,

    BTN_START  = 1u << 10,
    BTN_SELECT = 1u << 11,
} btn_t;

typedef struct {
    uint16_t raw;   // pre debounce rest is post debounce
    uint16_t current;
    uint16_t previous;

    uint16_t pressed;
    uint16_t released;

    uint16_t debounce_history[5];
    uint8_t debounce_index;
} input_state_t;


void update_input_state(input_state_t *inputs);


#endif

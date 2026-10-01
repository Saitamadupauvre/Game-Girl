
#include "inputs.h"


#define INPUT_DEBOUNCE_SAMPLES 5u
#define INPUT_BUTTON_MASK      0x0FFFu

void update_input_state(input_state_t *inputs)
{
    uint16_t stable_pressed = INPUT_BUTTON_MASK;
    uint16_t stable_released = INPUT_BUTTON_MASK;

    inputs->debounce_history[inputs->debounce_index] = inputs->raw;
    inputs->debounce_index++;

    if (inputs->debounce_index == INPUT_DEBOUNCE_SAMPLES) {
        inputs->debounce_index = 0;
    }

    for (uint8_t i = 0; i < INPUT_DEBOUNCE_SAMPLES; ++i) {
        const uint16_t sample = inputs->debounce_history[i];

        stable_pressed &= sample;
        stable_released &= (uint16_t)~sample;
    }

    inputs->previous = inputs->current;
    inputs->current |= stable_pressed;
    inputs->current &= (uint16_t)~stable_released;
    inputs->pressed = inputs->current & (uint16_t)~inputs->previous;
    inputs->released = (uint16_t)~inputs->current & inputs->previous;
}

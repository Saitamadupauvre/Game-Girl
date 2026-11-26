/**
  ******************************************************************************
  * @file    inputs.c
  * @brief   Button debouncing and press/release edges.
  *
  *          This file provides functions to:
  *           + Debounce the raw button sample and update the button state
  ******************************************************************************
  *
  * update_input_state() is called once per 1 ms sample with the raw bits from gpio.c. A button
  * changes state only when it reads the same in all of the last INPUT_DEBOUNCE_SAMPLES samples.
  * It then sets current (held now), pressed and released (edges since the previous call).
  *
  * Pure, tested on the host (tests/test_inputs.c).
  ******************************************************************************
  */

#include "inputs.h"


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

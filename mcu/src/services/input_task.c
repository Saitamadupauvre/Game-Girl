/**
  ******************************************************************************
  * @file    input_task.c
  * @brief   Main-loop task that samples the buttons every 1 ms.
  *
  *          This file provides functions to:
  *           + Sample and debounce the buttons on a 1 ms deadline
  *           + Give other tasks the current button state
  ******************************************************************************
  *
  * input_task() never blocks: it returns at once until INPUT_UPDATE_PERIOD_MS has passed, then
  * reads the pins (gpio.c) and debounces them (inputs.c). usb_task.c reads the result through
  * inputs_get().
  ******************************************************************************
  */

#include "inputs.h"
#include "gpio.h"
#include "timer_api.h"

#include <stdbool.h>


static input_state_t inputs;
static deadline_t sample_timer;
static bool started;

const input_state_t *inputs_get(void)
{
    return &inputs;
}

void input_task(void)
{
    // Arms the timer based on TIM2.
    if (!started) {
        timer_start(&sample_timer, INPUT_UPDATE_PERIOD_MS);
        started = true;
        return;
    }

    if (!timer_expired(&sample_timer)) {
        return;
    }

    read_GPIO_input_pins(&inputs);
    update_input_state(&inputs);

    timer_start(&sample_timer, INPUT_UPDATE_PERIOD_MS);
}

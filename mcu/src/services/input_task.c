
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

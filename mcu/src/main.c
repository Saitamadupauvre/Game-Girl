
#include "gpio.h"
#include "inputs.h"
#include "timer.h"
#include "timer_api.h"
#include "usb.h"


int main(void)
{
    init_GPIO();
    init_timer();
    init_usb();

    input_state_t inputs = {0};

    deadline_t input_timer = {0};

    timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);

    for (;;) {
        if (timer_expired(&input_timer)) {
            read_GPIO_input_pins(&inputs);
            update_input_state(&inputs);

            timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);
        }
    }

    return 0;
}

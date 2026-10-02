
#include "gpio.h"
#include "inputs.h"
#include "timer.h"


int main(void)
{
    enable_GPIO_clocks();
    set_GPIO_pins();
    enable_hardware_timer_clocks();

    input_state_t inputs = {0};

    for (;;) {
        if (timer_event_elapsed()) {
            read_GPIO_input_pins(&inputs);
            update_input_state(&inputs);
        }
    }

    return 0;
}

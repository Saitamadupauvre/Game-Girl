
#include "gpio.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer.h"
#include "timer_api.h"
#include "usb.h"


int main(void)
{
    const bool hclock_ok = init_hardware_clock();

    init_GPIO();
    init_timer();

    if (hclock_ok) {
        init_usb();
    }

    input_state_t inputs = {0};

    deadline_t input_timer = {0};

    timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);

    for (;;) {
        if (timer_expired(&input_timer)) {
            read_GPIO_input_pins(&inputs);
            update_input_state(&inputs);
            usb_hid_send_if_changed(&inputs);

            timer_start(&input_timer, INPUT_UPDATE_PERIOD_MS);
        }
    }

    return 0;
}

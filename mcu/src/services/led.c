#include "led.h"
#include <stdbool.h>

static bool is_charging(led_output_t* output, const led_input_t *input)
{
    if (input->charger_connected) {
        output->battery_led = false;
        output->charging_led = true;
        return true;
    }
    return false;
}

static void handle_ok_battery(led_output_t* output, const led_input_t *input)
{
    if (is_charging(output, input)) return;

    output->battery_led = false;
    output->charging_led = false;
}

static void handle_low_battery(led_output_t* output, const led_input_t *input)
{
    if (is_charging(output, input)) return;

    output->battery_led = true;
    output->charging_led = false;
}

static void handle_critical_battery(led_output_t* output, const led_input_t *input, uint32_t now_ms)
{
    if (is_charging(output, input)) return;

    if (now_ms % LED_CRITICAL_BLINK_PERIOD_MS < LED_CRITICAL_BLINK_ON_MS) {
        output->battery_led = true;
        output->charging_led = false;
    } else {
        output->battery_led = false;
        output->charging_led = false;
    }
}

led_output_t update_led_outputs(const led_input_t *input, uint32_t now_ms)
{
    led_output_t output = {0};

    switch (input->battery_level) {
        case BATTERY_LEVEL_OK:
            handle_ok_battery(&output, input);
            break;
        case BATTERY_LEVEL_LOW:
            handle_low_battery(&output, input);
            break;
        case BATTERY_LEVEL_CRITICAL:
            handle_critical_battery(&output, input, now_ms);
            break;
    }
    return output;
}

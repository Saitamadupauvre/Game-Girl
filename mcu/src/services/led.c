/**
  ******************************************************************************
  * @file    led.c
  * @brief   LED policy: what the green, red and blue status LEDs show.
  *
  *          This file provides functions to:
  *           + Decide every LED's state from the system facts and the time
  ******************************************************************************
  *
  * Green: charger connected. Red: off when the battery is OK, solid when low, blinking when
  * critical, always off while charging. Blue: blink code of the active fault, off when healthy.
  *
  * Fault code: N flashes (LED_FAULT_ON_MS lit in each LED_FAULT_SLOT_MS), then LED_FAULT_PAUSE_MS
  * dark, repeated. N = 2 clock fallback, 3 USB start-up, 4 host lost, 5 USB disabled by the reset
  * guard. Faults get their own LED so they never clash with the battery patterns.
  *
  * No registers and no state: the same inputs and time always give the same outputs. led_task.c
  * applies them to the pins. Tests: tests/test_led.c. Policy: docs/mcu/services/led.md.
  ******************************************************************************
  */

#include "led.h"
#include "fault.h"

#include <stdbool.h>
#include <stdint.h>


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

// Number of flashes for a fault, 0 = not shown. Codes and their meaning: docs/mcu/services/led.md
static uint32_t fault_flash_count(fault_t sys_fault)
{
    if ((sys_fault >= FAULT_CLOCK_VOS) && (sys_fault <= FAULT_CLOCK_MSIS_RANGE)) {
        return 2u;
    }

    if ((sys_fault >= FAULT_USB_HSI48) && (sys_fault <= FAULT_USB_VDDUSB)) {
        return 3u;
    }

    if (sys_fault == FAULT_USB_HOST_LOST) {
        return 4u;
    }

    if (sys_fault == FAULT_USB_HANG) {
        return 5u;
    }

    return 0u;
}

static bool fault_led_on(fault_t sys_fault, uint32_t now_ms)
{
    const uint32_t flashes = fault_flash_count(sys_fault);
    if (flashes == 0u) {
        return false;
    }

    const uint32_t flashing_ms = flashes * LED_FAULT_SLOT_MS;
    const uint32_t t = now_ms % (flashing_ms + LED_FAULT_PAUSE_MS);

    return (t < flashing_ms) && ((t % LED_FAULT_SLOT_MS) < LED_FAULT_ON_MS);
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

    output.fault_led = fault_led_on(input->active_fault, now_ms);
    return output;
}

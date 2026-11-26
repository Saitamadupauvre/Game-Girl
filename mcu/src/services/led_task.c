/**
  ******************************************************************************
  * @file    led_task.c
  * @brief   Main-loop task that drives the status LEDs.
  *
  *          This file provides functions to:
  *           + Gather the LED inputs, apply the LED policy and drive the pins
  ******************************************************************************
  *
  * Kept apart from led.c so the policy stays pure and host-testable. Runs every loop pass: the
  * blink patterns come from time_now_ms(), and one BSRR write per LED is cheap.
  ******************************************************************************
  */

#include "led.h"
#include "fault.h"
#include "gpio.h"
#include "timer_api.h"


void led_task(void)
{
    // No charger or battery driver yet: green and red stay off until the battery service fills these in
    const led_input_t input = {
        .charger_connected = false,
        .battery_level = BATTERY_LEVEL_OK,
        .active_fault = fault_first_active(),
    };

    const led_output_t leds = update_led_outputs(&input, time_now_ms());
    write_GPIO_leds(&leds);
}

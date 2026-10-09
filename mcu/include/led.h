/**
 * @file led.h
 * @brief Hardware-independent LED status policy for the Game GIRL MCU.
 *
 * The console has two status LEDs, one green and one red:
 *  - **Charging LED (green)**: solid on while the charger is connected.
 *  - **Battery LED (red)**: off when the battery is OK, solid on when low,
 *    blinking when critical. Always off while charging: the green LED
 *    already tells the user the battery is being taken care of.
 *
 * This service only *decides* what the LEDs should show. It never touches
 * GPIO, PWM, or any register: a driver applies the result later. Time is
 * passed in by the caller, so every decision is deterministic and can be
 * unit-tested on the host.
 */

#ifndef LED_H
    #define LED_H

    #include <stdbool.h>
    #include <stdint.h>

/**
 * @brief Full blink period of the battery LED when the level is critical.
 *
 * One period = one on phase followed by one off phase.
 */
    #define LED_CRITICAL_BLINK_PERIOD_MS 250u

/**
 * @brief How long the battery LED stays on within each blink period.
 *
 * Half the period gives an even blink; a small value gives a short flash.
 * Must be greater than 0 and less than LED_CRITICAL_BLINK_PERIOD_MS.
 */
    #define LED_CRITICAL_BLINK_ON_MS 125u

_Static_assert(LED_CRITICAL_BLINK_ON_MS > 0u &&
               LED_CRITICAL_BLINK_ON_MS < LED_CRITICAL_BLINK_PERIOD_MS,
               "LED_CRITICAL_BLINK_ON_MS must be in (0, LED_CRITICAL_BLINK_PERIOD_MS)");

/**
 * @brief Battery charge level, as classified by the battery service.
 */
typedef enum {
    BATTERY_LEVEL_OK,       /**< 35% and above. */
    BATTERY_LEVEL_LOW,      /**< Below 35%. */
    BATTERY_LEVEL_CRITICAL, /**< Below 15%. */
} battery_level_t;

/**
 * @brief System facts the LED policy decides from.
 */
typedef struct {
    bool charger_connected;        /**< True while external power is plugged in. */
    battery_level_t battery_level; /**< Current battery classification. */
} led_input_t;

/**
 * @brief Desired state of each LED. True means lit.
 */
typedef struct {
    bool charging_led; /**< Green LED: charging indicator. */
    bool battery_led;  /**< Red LED: low / critical battery indicator. */
} led_output_t;

/**
 * @brief Decide what each LED should show right now.
 *
 * @param input   System facts. Must not be NULL.
 * @param now_ms  Current time in milliseconds. When it wraps (~49 days),
 *                one blink phase may be shorter; this is harmless.
 *
 * @return Desired on/off state of both LEDs.
 */
led_output_t update_led_outputs(const led_input_t *input, uint32_t now_ms);

#endif

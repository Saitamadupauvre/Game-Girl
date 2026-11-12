#include "led.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK_BOOL(actual, expected) do { \
    const bool actual_value = (actual); \
    const bool expected_value = (expected); \
    if (actual_value != expected_value) { \
        (void)fprintf(stderr, "%s:%d: %s: expected %s, got %s\n", \
            __FILE__, __LINE__, #actual, \
            expected_value ? "on" : "off", \
            actual_value ? "on" : "off"); \
        return 1; \
    } \
} while (0)

#define RUN_TEST(test) do { \
    if ((test)() != 0) { \
        return 1; \
    } \
} while (0)

#define PERIOD LED_CRITICAL_BLINK_PERIOD_MS
#define ON_MS  LED_CRITICAL_BLINK_ON_MS

static led_output_t outputs_at(bool charger_connected, battery_level_t level,
    uint32_t now_ms)
{
    const led_input_t input = {
        .charger_connected = charger_connected,
        .battery_level = level,
    };

    return update_led_outputs(&input, now_ms);
}

// Sample times spread over several blink periods and the uint32_t range.
static const uint32_t sample_times[] = {
    0u, 1u, ON_MS - 1u, ON_MS, PERIOD - 1u, PERIOD, 12345u,
    UINT32_MAX / 2u, UINT32_MAX - 1u, UINT32_MAX,
};

#define SAMPLE_COUNT (sizeof(sample_times) / sizeof(sample_times[0]))

static int test_all_off_when_idle(void)
{
    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        const led_output_t out = outputs_at(false, BATTERY_LEVEL_OK,
            sample_times[i]);

        CHECK_BOOL(out.charging_led, false);
        CHECK_BOOL(out.battery_led, false);
    }

    return 0;
}

static int test_charging_led_follows_charger(void)
{
    const battery_level_t levels[] = {
        BATTERY_LEVEL_OK, BATTERY_LEVEL_LOW, BATTERY_LEVEL_CRITICAL,
    };

    for (size_t l = 0; l < 3u; ++l) {
        for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
            CHECK_BOOL(outputs_at(true, levels[l], sample_times[i]).charging_led,
                true);
            CHECK_BOOL(outputs_at(false, levels[l], sample_times[i]).charging_led,
                false);
        }
    }

    return 0;
}

static int test_battery_led_solid_when_low(void)
{
    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_LOW,
            sample_times[i]).battery_led, true);
    }

    return 0;
}

static int test_battery_led_off_when_ok(void)
{
    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        CHECK_BOOL(outputs_at(true, BATTERY_LEVEL_OK,
            sample_times[i]).battery_led, false);
    }

    return 0;
}

static int test_battery_led_blinks_when_critical(void)
{
    // First period: on phase, then off phase.
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL, 0u).battery_led, true);
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL, ON_MS - 1u).battery_led,
        true);
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL, ON_MS).battery_led,
        false);
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL, PERIOD - 1u).battery_led,
        false);

    // Next period starts lit again.
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL, PERIOD).battery_led,
        true);
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL,
        PERIOD + ON_MS - 1u).battery_led, true);
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL,
        PERIOD + ON_MS).battery_led, false);

    // Far in the future, the pattern repeats the same way.
    const uint32_t far = PERIOD * 1000000u;

    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL, far).battery_led, true);
    CHECK_BOOL(outputs_at(false, BATTERY_LEVEL_CRITICAL,
        far + ON_MS).battery_led, false);

    return 0;
}

static int test_red_led_off_while_charging(void)
{
    const battery_level_t levels[] = {
        BATTERY_LEVEL_OK, BATTERY_LEVEL_LOW, BATTERY_LEVEL_CRITICAL,
    };

    for (size_t l = 0; l < 3u; ++l) {
        for (uint32_t t = 0; t < 3u * PERIOD; ++t) {
            CHECK_BOOL(outputs_at(true, levels[l], t).battery_led, false);
        }
    }

    return 0;
}

static int test_critical_blink_duty_cycle(void)
{
    uint32_t lit = 0;

    for (uint32_t t = 0; t < PERIOD; ++t) {
        if (outputs_at(false, BATTERY_LEVEL_CRITICAL, t).battery_led) {
            ++lit;
        }
    }

    if (lit != ON_MS) {
        (void)fprintf(stderr, "%s:%d: lit %u ms per period, expected %u\n",
            __FILE__, __LINE__, (unsigned)lit, (unsigned)ON_MS);
        return 1;
    }

    return 0;
}

static int test_input_is_not_modified(void)
{
    const led_input_t input = {
        .charger_connected = true,
        .battery_level = BATTERY_LEVEL_CRITICAL,
    };
    led_input_t copy = input;

    (void)update_led_outputs(&copy, 42u);

    CHECK_BOOL(copy.charger_connected, input.charger_connected);
    CHECK_BOOL(copy.battery_level == input.battery_level, true);

    return 0;
}

int main(void)
{
    RUN_TEST(test_all_off_when_idle);
    RUN_TEST(test_charging_led_follows_charger);
    RUN_TEST(test_battery_led_solid_when_low);
    RUN_TEST(test_battery_led_off_when_ok);
    RUN_TEST(test_battery_led_blinks_when_critical);
    RUN_TEST(test_red_led_off_while_charging);
    RUN_TEST(test_critical_blink_duty_cycle);
    RUN_TEST(test_input_is_not_modified);

    return 0;
}

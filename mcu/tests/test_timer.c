#include "timer_api.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t simulated_time_ms = 0;

uint32_t time_now_ms(void)
{
    return simulated_time_ms;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: assertion failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define CHECK_EQUAL_U32(actual, expected) do { \
    const uint32_t actual_val = (actual); \
    const uint32_t expected_val = (expected); \
    if (actual_val != expected_val) { \
        (void)fprintf(stderr, "%s:%d: expected 0x%08" PRIX32 \
            ", got 0x%08" PRIX32 "\n", __FILE__, __LINE__, \
            expected_val, actual_val); \
        return 1; \
    } \
} while (0)

int test_null_guards(void)
{
    /* Calling timer_start or timer_expired with NULL must not crash. */
    timer_start(NULL, 100);
    CHECK(timer_expired(NULL) == false);

    return 0;
}

int test_timer_normal_flow(void)
{
    simulated_time_ms = 1000;

    deadline_t t = {0};
    timer_start(&t, 50);

    CHECK_EQUAL_U32(t.deadline_ms, 1050);

    /* Before deadline */
    simulated_time_ms = 1049;
    CHECK(timer_expired(&t) == false);

    /* Exactly at deadline */
    simulated_time_ms = 1050;
    CHECK(timer_expired(&t) == true);

    /* Past deadline */
    simulated_time_ms = 1051;
    CHECK(timer_expired(&t) == true);

    /* Remains expired until restarted (level check) */
    simulated_time_ms = 2000;
    CHECK(timer_expired(&t) == true);

    /* Restart timer */
    timer_start(&t, 10);
    CHECK_EQUAL_U32(t.deadline_ms, 2010);
    CHECK(timer_expired(&t) == false);

    simulated_time_ms = 2010;
    CHECK(timer_expired(&t) == true);

    return 0;
}

int test_timer_zero_duration(void)
{
    simulated_time_ms = 500;

    deadline_t t = {0};
    timer_start(&t, 0);

    CHECK_EQUAL_U32(t.deadline_ms, 500);
    CHECK(timer_expired(&t) == true);

    return 0;
}

int test_timer_wraparound(void)
{
    /* Set time close to UINT32_MAX */
    simulated_time_ms = UINT32_MAX - 10UL;

    deadline_t t = {0};
    /* Duration of 20 ms wraps deadline to 9 ms */
    timer_start(&t, 20);

    CHECK_EQUAL_U32(t.deadline_ms, 9UL);

    /* Before wrap */
    simulated_time_ms = UINT32_MAX - 5UL;
    CHECK(timer_expired(&t) == false);

    /* At wrap point */
    simulated_time_ms = UINT32_MAX;
    CHECK(timer_expired(&t) == false);

    /* Just wrapped to 0 */
    simulated_time_ms = 0;
    CHECK(timer_expired(&t) == false);

    /* Approaching deadline */
    simulated_time_ms = 8;
    CHECK(timer_expired(&t) == false);

    /* Exactly at deadline */
    simulated_time_ms = 9;
    CHECK(timer_expired(&t) == true);

    /* Past deadline */
    simulated_time_ms = 10;
    CHECK(timer_expired(&t) == true);

    simulated_time_ms = 100;
    CHECK(timer_expired(&t) == true);

    return 0;
}

int test_multiple_timers(void)
{
    simulated_time_ms = 5000;

    deadline_t input_timer = {0};
    deadline_t usb_timer = {0};
    deadline_t display_timer = {0};

    timer_start(&input_timer, 1);
    timer_start(&usb_timer, 25);
    timer_start(&display_timer, 50);

    CHECK(timer_expired(&input_timer) == false);
    CHECK(timer_expired(&usb_timer) == false);
    CHECK(timer_expired(&display_timer) == false);

    /* Advance by 1 ms */
    simulated_time_ms = 5001;
    CHECK(timer_expired(&input_timer) == true);
    CHECK(timer_expired(&usb_timer) == false);
    CHECK(timer_expired(&display_timer) == false);

    /* Restart input timer */
    timer_start(&input_timer, 1);
    CHECK(timer_expired(&input_timer) == false);

    /* Advance to 5025 ms */
    simulated_time_ms = 5025;
    CHECK(timer_expired(&input_timer) == true);
    CHECK(timer_expired(&usb_timer) == true);
    CHECK(timer_expired(&display_timer) == false);

    /* Advance to 5050 ms */
    simulated_time_ms = 5050;
    CHECK(timer_expired(&display_timer) == true);

    return 0;
}

int main(void)
{
    if (test_null_guards() != 0) {
        return 1;
    }

    if (test_timer_normal_flow() != 0) {
        return 1;
    }

    if (test_timer_zero_duration() != 0) {
        return 1;
    }

    if (test_timer_wraparound() != 0) {
        return 1;
    }

    if (test_multiple_timers() != 0) {
        return 1;
    }

    return 0;
}

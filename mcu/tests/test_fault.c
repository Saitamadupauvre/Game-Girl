#include "fault.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: assertion failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define CHECK_FAULT(actual, expected) do { \
    const uint32_t actual_val = (uint32_t)(actual); \
    const uint32_t expected_val = (uint32_t)(expected); \
    if (actual_val != expected_val) { \
        (void)fprintf(stderr, "%s:%d: expected fault 0x%02" PRIX32 \
            ", got 0x%02" PRIX32 "\n", __FILE__, __LINE__, \
            expected_val, actual_val); \
        return 1; \
    } \
} while (0)

static int check_all_healthy(void)
{
    for (uint8_t i = 0; i < FAULT_SUBSYS_COUNT; i++) {
        CHECK_FAULT(fault_read_active((fault_subsystem_t)i), FAULT_NONE);
    }

    return 0;
}

int test_starts_healthy(void)
{
    fault_reset_test();

    return check_all_healthy();
}

int test_set_stores_cause_per_subsystem(void)
{
    fault_reset_test();

    fault_set_failure(FAULT_USB_VDDUSB);

    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_VDDUSB);
    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_CLOCK), FAULT_NONE);
    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_SYS), FAULT_NONE);

    return 0;
}

int test_set_keeps_latest_cause(void)
{
    fault_reset_test();

    fault_set_failure(FAULT_USB_HSI48);
    fault_set_failure(FAULT_USB_BSESSION);

    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_BSESSION);

    return 0;
}

int test_clear_only_touches_its_subsystem(void)
{
    fault_reset_test();

    fault_set_failure(FAULT_CLOCK_VOS);
    fault_set_failure(FAULT_USB_VDDUSB);
    fault_clear_failure(FAULT_SUBSYS_USB);

    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);
    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_CLOCK), FAULT_CLOCK_VOS);

    return 0;
}

int test_log_is_history_not_active(void)
{
    /* A retried fault is logged on every attempt but must not show as active. */
    fault_reset_test();

    fault_log(FAULT_USB_CORE_RESET);

    return check_all_healthy();
}

int test_set_none_is_ignored(void)
{
    fault_reset_test();

    fault_set_failure(FAULT_NONE);

    return check_all_healthy();
}

int test_out_of_range_is_ignored(void)
{
    /* 0xF0 has subsystem 15, past FAULT_SUBSYS_COUNT. */
    const fault_t bad = (fault_t)0xF0;

    fault_reset_test();
    fault_set_failure(FAULT_USB_VDDUSB);

    fault_set_failure(bad);
    fault_log(bad);
    fault_clear_failure((fault_subsystem_t)15);

    CHECK_FAULT(fault_read_active((fault_subsystem_t)15), FAULT_NONE);
    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_COUNT), FAULT_NONE);
    CHECK_FAULT(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_VDDUSB);

    return 0;
}

int test_reset_clears_active(void)
{
    fault_reset_test();
    fault_set_failure(FAULT_CLOCK_MSIS_RANGE);
    fault_set_failure(FAULT_USB_DEVICE_MODE);

    fault_reset_test();

    return check_all_healthy();
}

int main(void)
{
    if (test_starts_healthy() != 0) {
        return 1;
    }

    if (test_set_stores_cause_per_subsystem() != 0) {
        return 1;
    }

    if (test_set_keeps_latest_cause() != 0) {
        return 1;
    }

    if (test_clear_only_touches_its_subsystem() != 0) {
        return 1;
    }

    if (test_log_is_history_not_active() != 0) {
        return 1;
    }

    if (test_set_none_is_ignored() != 0) {
        return 1;
    }

    if (test_out_of_range_is_ignored() != 0) {
        return 1;
    }

    if (test_reset_clears_active() != 0) {
        return 1;
    }

    return 0;
}

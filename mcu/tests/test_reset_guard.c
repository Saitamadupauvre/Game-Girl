#include "stm32u575_regs.h"
#include "reset_guard.h"
#include "backup.h"
#include "fault.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: assertion failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define CHECK_HEX(actual, expected) do { \
    const uint32_t actual_val = (uint32_t)(actual); \
    const uint32_t expected_val = (uint32_t)(expected); \
    if (actual_val != expected_val) { \
        (void)fprintf(stderr, "%s:%d: expected 0x%08" PRIX32 \
            ", got 0x%08" PRIX32 "\n", __FILE__, __LINE__, \
            expected_val, actual_val); \
        return 1; \
    } \
} while (0)

// What the hardware reports: every internal reset also sets PINRSTF
#define IWDG_RESET      (RCC_CSR_IWDGRSTF | RCC_CSR_PINRSTF)
#define POWER_ON_RESET  (RCC_CSR_BORRSTF | RCC_CSR_PINRSTF)

// Packed count n for one subsystem
#define COUNT(subsys, n)    ((uint32_t)(n) << (4u * (uint32_t)(subsys)))


/* Fakes: backup.c and timer.c are not linked */

static uint32_t now_ms;
static uint32_t fake_csr;
static uint32_t fake_bkp[3];

uint32_t time_now_ms(void)
{
    return now_ms;
}

uint32_t reset_flags_read_clear(void)
{
    const uint32_t flags = fake_csr;
    fake_csr = 0u;
    return flags;
}

uint32_t backup_read(const uint32_t idx)
{
    return (idx < 3u) ? fake_bkp[idx] : 0u;
}

void backup_write(const uint32_t idx, const uint32_t value)
{
    if (idx < 3u) {
        fake_bkp[idx] = value;
    }
}

fault_subsystem_t breadcrumb_fetch(void)
{
    return (fault_subsystem_t)fake_bkp[BKP_BREADCRUMB];
}

void breadcrumb_set(fault_subsystem_t subsys)
{
    fake_bkp[BKP_BREADCRUMB] = (uint32_t)subsys;
}

// Boots with the given reset flags, breadcrumb and stored counts
static void boot(uint32_t csr, fault_subsystem_t breadcrumb, uint32_t counts)
{
    now_ms = 0u;
    fake_csr = csr;
    fake_bkp[BKP_HANG_COUNTS] = counts;
    fake_bkp[BKP_BREADCRUMB] = (uint32_t)breadcrumb;
    fault_reset_test();

    init_reset_guard();
}


/* reset_guard_decide() */

int test_power_on_clears_counts(void)
{
    CHECK_HEX(reset_guard_decide(POWER_ON_RESET, FAULT_SUBSYS_USB, COUNT(FAULT_SUBSYS_USB, 2)), 0u);

    return 0;
}

int test_pin_reset_clears_counts(void)
{
    CHECK_HEX(reset_guard_decide(RCC_CSR_PINRSTF, FAULT_SUBSYS_USB, COUNT(FAULT_SUBSYS_USB, 2)), 0u);

    return 0;
}

int test_watchdog_counts_breadcrumb(void)
{
    CHECK_HEX(reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_USB, 0u), COUNT(FAULT_SUBSYS_USB, 1));

    return 0;
}

int test_three_usb_hangs_disable_usb(void)
{
    uint32_t counts = 0u;

    for (uint32_t i = 0u; i < 2u; i++) {
        counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_USB, counts);
        CHECK(!reset_guard_is_disabled(counts, FAULT_SUBSYS_USB));
    }

    counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_USB, counts);
    CHECK(reset_guard_is_disabled(counts, FAULT_SUBSYS_USB));

    return 0;
}

int test_input_and_power_never_disabled(void)
{
    uint32_t counts = 0u;

    for (uint32_t i = 0u; i < 5u; i++) {
        counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_INPUT, counts);
        counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_POWER, counts);
    }

    CHECK_HEX(counts, COUNT(FAULT_SUBSYS_INPUT, 5) | COUNT(FAULT_SUBSYS_POWER, 5));
    CHECK(!reset_guard_is_disabled(counts, FAULT_SUBSYS_INPUT));
    CHECK(!reset_guard_is_disabled(counts, FAULT_SUBSYS_POWER));

    return 0;
}

int test_none_and_garbage_count_as_sys(void)
{
    uint32_t counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_NONE, 0u);
    counts = reset_guard_decide(IWDG_RESET, (fault_subsystem_t)0xFF, counts);

    CHECK_HEX(counts, COUNT(FAULT_SUBSYS_SYS, 2));
    CHECK(!reset_guard_is_disabled(counts, FAULT_SUBSYS_SYS));

    return 0;
}

int test_counts_are_per_subsystem(void)
{
    uint32_t counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_USB, 0u);
    counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_I2C, counts);
    counts = reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_USB, counts);

    CHECK_HEX(counts, COUNT(FAULT_SUBSYS_USB, 2) | COUNT(FAULT_SUBSYS_I2C, 1));
    CHECK(!reset_guard_is_disabled(counts, FAULT_SUBSYS_USB));

    return 0;
}

int test_count_saturates(void)
{
    const uint32_t full = COUNT(FAULT_SUBSYS_USB, 0xF);

    CHECK_HEX(reset_guard_decide(IWDG_RESET, FAULT_SUBSYS_USB, full), full);

    return 0;
}

int test_clear_keeps_disabled_counts(void)
{
    const uint32_t counts = COUNT(FAULT_SUBSYS_USB, 3) | COUNT(FAULT_SUBSYS_INPUT, 2);

    CHECK_HEX(reset_guard_clear_enabled(counts), COUNT(FAULT_SUBSYS_USB, 3));

    return 0;
}


/* init_reset_guard() and reset_guard_task() */

int test_init_stores_counts_and_resets_breadcrumb(void)
{
    boot(IWDG_RESET, FAULT_SUBSYS_USB, COUNT(FAULT_SUBSYS_USB, 1));

    CHECK_HEX(fake_bkp[BKP_HANG_COUNTS], COUNT(FAULT_SUBSYS_USB, 2));
    CHECK_HEX(fake_bkp[BKP_BREADCRUMB], FAULT_SUBSYS_NONE);
    CHECK(reset_guard_enabled(FAULT_SUBSYS_USB));
    CHECK_HEX(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);

    return 0;
}

int test_init_disables_usb_at_limit(void)
{
    boot(IWDG_RESET, FAULT_SUBSYS_USB, COUNT(FAULT_SUBSYS_USB, 2));

    CHECK(!reset_guard_enabled(FAULT_SUBSYS_USB));
    CHECK(reset_guard_enabled(FAULT_SUBSYS_INPUT));
    CHECK_HEX(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_HANG);

    return 0;
}

int test_power_on_reenables_usb(void)
{
    boot(POWER_ON_RESET, FAULT_SUBSYS_USB, COUNT(FAULT_SUBSYS_USB, 3));

    CHECK(reset_guard_enabled(FAULT_SUBSYS_USB));
    CHECK_HEX(fake_bkp[BKP_HANG_COUNTS], 0u);

    return 0;
}

int test_clean_run_clears_enabled_counts(void)
{
    boot(IWDG_RESET, FAULT_SUBSYS_INPUT, COUNT(FAULT_SUBSYS_USB, 3));   // USB stays disabled, INPUT now 1

    now_ms = 9999u;
    reset_guard_task();
    CHECK_HEX(fake_bkp[BKP_HANG_COUNTS], COUNT(FAULT_SUBSYS_USB, 3) | COUNT(FAULT_SUBSYS_INPUT, 1));

    now_ms = 10000u;
    reset_guard_task();
    CHECK_HEX(fake_bkp[BKP_HANG_COUNTS], COUNT(FAULT_SUBSYS_USB, 3));
    CHECK(!reset_guard_enabled(FAULT_SUBSYS_USB));

    return 0;
}

int test_clean_run_clears_once(void)
{
    boot(IWDG_RESET, FAULT_SUBSYS_INPUT, 0u);

    now_ms = 10000u;
    reset_guard_task();
    CHECK_HEX(fake_bkp[BKP_HANG_COUNTS], 0u);

    fake_bkp[BKP_HANG_COUNTS] = 0x1234u;    // Must not be rewritten after the first clear
    now_ms = 20000u;
    reset_guard_task();
    CHECK_HEX(fake_bkp[BKP_HANG_COUNTS], 0x1234u);

    return 0;
}


int main(void)
{
    int (*const tests[])(void) = {
        test_power_on_clears_counts,
        test_pin_reset_clears_counts,
        test_watchdog_counts_breadcrumb,
        test_three_usb_hangs_disable_usb,
        test_input_and_power_never_disabled,
        test_none_and_garbage_count_as_sys,
        test_counts_are_per_subsystem,
        test_count_saturates,
        test_clear_keeps_disabled_counts,
        test_init_stores_counts_and_resets_breadcrumb,
        test_init_disables_usb_at_limit,
        test_power_on_reenables_usb,
        test_clean_run_clears_enabled_counts,
        test_clean_run_clears_once,
    };

    for (size_t i = 0u; i < (sizeof tests / sizeof tests[0]); i++) {
        if (tests[i]() != 0) {
            return 1;
        }
    }

    return 0;
}

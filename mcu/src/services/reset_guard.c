/**
  ******************************************************************************
  * @file    reset_guard.c
  * @brief   Reset-loop guard: disables the subsystem behind repeated watchdog resets.
  *
  *          This file provides functions to:
  *           + Decide the hang counts after a reset (pure, tested on the host)
  *           + Tell whether a subsystem has been disabled
  *           + Clear the counts after a clean run
  ******************************************************************************
  *
  * A watchdog reset only helps if the hang was transient. If the same task hangs on every boot,
  * the console would reboot forever. This module stops that loop by disabling the subsystem
  * responsible, when the console can run without it.
  *
  * Before each task the main loop writes a breadcrumb (the task's fault_subsystem_t) to
  * TAMP_BKPR(BKP_BREADCRUMB). After a watchdog reset, init_reset_guard() reads it and increments
  * that subsystem's hang count in TAMP_BKPR(BKP_HANG_COUNTS). Both registers survive a system reset.
  *
  * Hang counts: one 4-bit counter per fault_subsystem_t, packed in one uint32_t
  * (subsystem n in bits 4n..4n+3).
  *
  * Rules:
  *   - Any reset other than the watchdog clears every count.
  *   - A disableable subsystem (USB, later I2C) is turned off once its count reaches GUARD_LIMIT.
  *     INPUT and POWER are never disabled.
  *   - After GUARD_CLEAN_RUN_MS without a hang, the counts of the enabled subsystems are cleared.
  *     A disabled subsystem stays off until the next non-watchdog reset.
  *
  * Tests: tests/test_reset_guard.c. Degraded modes: docs/mcu/power-and-usb-fallback.md.
  ******************************************************************************
  */

#include "stm32u575_regs.h"
#include "reset_guard.h"
#include "backup.h"
#include "fault.h"
#include "timer_api.h"

#include <stdbool.h>
#include <stdint.h>

#define GUARD_LIMIT         3u          // Watchdog resets in a row before a subsystem is disabled
#define GUARD_COUNT_MAX     0xFu        // 4-bit counter: saturates instead of spilling into the next one
#define GUARD_CLEAN_RUN_MS  10000UL     // Running this long without a hang clears the enabled counters

// Subsystems the console can run without. INPUT and POWER are never disabled. Add I2C with the gauge driver.
#define GUARD_DISABLEABLE   (1u << FAULT_SUBSYS_USB)


static uint32_t counts;
static deadline_t clean_run;
static bool cleared;


static uint32_t count_get(uint32_t packed, fault_subsystem_t subsys)
{
    return (packed >> (4u * (uint32_t)subsys)) & 0xFu;
}

static uint32_t count_set(uint32_t packed, fault_subsystem_t subsys, uint32_t n)
{
    const uint32_t shift = 4u * (uint32_t)subsys;

    return (packed & ~(0xFu << shift)) | ((n & 0xFu) << shift);
}

uint32_t reset_guard_decide(uint32_t csr_flags, fault_subsystem_t breadcrumb, uint32_t counts_in)
{
    // PINRSTF is also set on every internal reset, so only IWDGRSTF is checked
    if ((csr_flags & RCC_CSR_IWDGRSTF) == 0u) {
        return 0u;      // Power-on, pin, software or brownout: fresh start
    }

    // Hang in the loop itself, or a breadcrumb wiped by a backup-domain reset
    if ((breadcrumb == FAULT_SUBSYS_NONE) || (breadcrumb >= FAULT_SUBSYS_COUNT)) {
        breadcrumb = FAULT_SUBSYS_SYS;
    }

    uint32_t n = count_get(counts_in, breadcrumb);
    if (n < GUARD_COUNT_MAX) {
        n++;
    }

    return count_set(counts_in, breadcrumb, n);
}

bool reset_guard_is_disabled(uint32_t packed, fault_subsystem_t subsys)
{
    if (subsys >= FAULT_SUBSYS_COUNT) {
        return false;
    }

    if ((GUARD_DISABLEABLE & (1u << (uint32_t)subsys)) == 0u) {
        return false;
    }

    return count_get(packed, subsys) >= GUARD_LIMIT;
}

uint32_t reset_guard_clear_enabled(uint32_t packed)
{
    for (uint32_t subsys = 0u; subsys < (uint32_t)FAULT_SUBSYS_COUNT; subsys++) {
        if (!reset_guard_is_disabled(packed, (fault_subsystem_t)subsys)) {
            packed = count_set(packed, (fault_subsystem_t)subsys, 0u);
        }
    }

    return packed;
}


void init_reset_guard(void)
{
    const uint32_t flags = reset_flags_read_clear();

    counts = reset_guard_decide(flags, breadcrumb_fetch(), backup_read(BKP_HANG_COUNTS));
    backup_write(BKP_HANG_COUNTS, counts);
    breadcrumb_set(FAULT_SUBSYS_NONE);

    if (reset_guard_is_disabled(counts, FAULT_SUBSYS_USB)) {
        fault_log(FAULT_USB_HANG);
        fault_set_failure(FAULT_USB_HANG);
    }

    for (uint32_t subsys = 0u; subsys < (uint32_t)FAULT_SUBSYS_COUNT; subsys++) {
        if (count_get(counts, (fault_subsystem_t)subsys) >= GUARD_LIMIT) {
            fault_log(FAULT_SYS_WATCHDOG_LOOP);
            break;
        }
    }

    cleared = false;
    timer_start(&clean_run, GUARD_CLEAN_RUN_MS);
}

bool reset_guard_enabled(fault_subsystem_t subsys)
{
    return !reset_guard_is_disabled(counts, subsys);
}

void reset_guard_task(void)
{
    if (cleared || !timer_expired(&clean_run)) {
        return;
    }

    // A disabled subsystem keeps its count, so it stays off until the next non-watchdog reset
    counts = reset_guard_clear_enabled(counts);
    backup_write(BKP_HANG_COUNTS, counts);
    cleared = true;
}

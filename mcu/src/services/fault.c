/**
  ******************************************************************************
  * @file    fault.c
  * @brief   Fault history and active faults: the one place every fault_t goes.
  *
  *          This file provides functions to:
  *           + Log a fault (history)
  *           + Set and clear a subsystem's active fault
  *           + Read the active faults
  ******************************************************************************
  *
  * Codes are in fault.h (0xSN: subsystem nibble, step nibble). Each module handles its own
  * recovery and only reports here; reporters (the fault LED, later the status protocol to the Pi)
  * read only this file, never a module's own state.
  *
  * History: fault_log() keeps the last fault and a saturating count per subsystem, and saves the
  * last fault to TAMP_BKPR(BKP_LAST_FAULT) so it survives a reset. It is called on every failed
  * attempt, including ones that are retried and succeed later.
  *
  * Active state: fault_set_failure() when a module degrades, fault_clear_failure() when it
  * recovers. fault_first_active() returns the lowest subsystem first: a clock fault also stops
  * USB, so it is the real cause.
  *
  * Tests: tests/test_fault.c.
  ******************************************************************************
  */

#include "fault.h"
#include "backup.h"

#include <stdint.h>


static volatile fault_t last_fault;
static volatile uint8_t fault_count[FAULT_SUBSYS_COUNT];
static volatile fault_t active_fault[FAULT_SUBSYS_COUNT];

#ifdef GAME_GIRL_HOST_TEST
void fault_reset_test(void)
{
    last_fault = FAULT_NONE;
    for (uint8_t i = 0; i < FAULT_SUBSYS_COUNT; i++) {
        fault_count[i] = 0;
        active_fault[i] = FAULT_NONE;
    }
}
#endif

fault_t fault_read_active(fault_subsystem_t subsys)
{
    if (subsys >= FAULT_SUBSYS_COUNT) {
        return FAULT_NONE;      // No such subsystem, so nothing is failing
    }
    
    return active_fault[subsys];
}

fault_t fault_first_active(void)
{
    for (uint8_t subsys = 0; subsys < FAULT_SUBSYS_COUNT; subsys++) {
        if (active_fault[subsys] != FAULT_NONE) {
            return active_fault[subsys];
        }
    }

    return FAULT_NONE;
}

void fault_clear_failure(fault_subsystem_t subsys)
{
    if (subsys >= FAULT_SUBSYS_COUNT) {
        return;
    }

    active_fault[subsys] = FAULT_NONE;
}

void fault_set_failure(fault_t f)
{
    fault_subsystem_t subsys = fault_subsys(f);
    if (f == FAULT_NONE || subsys >= FAULT_SUBSYS_COUNT) {
        return;
    }

    active_fault[subsys] = f;
}

void fault_log(fault_t f)
{
    fault_subsystem_t system = fault_subsys(f);
    if (system >= FAULT_SUBSYS_COUNT) {
        return;
    }
    
    last_fault = f;
    if (fault_count[system] < UINT8_MAX) {
        fault_count[system]++;
    }

    backup_write(BKP_LAST_FAULT, (uint32_t)f);
}


#include "fault.h"

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
}

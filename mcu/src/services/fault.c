
#include "fault.h"


static volatile fault_t last_fault;
static volatile uint8_t fault_count[FAULT_SUBSYS_COUNT];

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

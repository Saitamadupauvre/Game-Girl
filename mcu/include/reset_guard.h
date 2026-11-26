
#ifndef RESET_GUARD_H
    #define RESET_GUARD_H

    #include "fault.h"
    #include <stdbool.h>
    #include <stdint.h>

uint32_t reset_guard_decide(uint32_t csr_flags, fault_subsystem_t breadcrumb, uint32_t counts);
bool reset_guard_is_disabled(uint32_t counts, fault_subsystem_t subsys);
uint32_t reset_guard_clear_enabled(uint32_t counts);

void init_reset_guard(void); 
bool reset_guard_enabled(fault_subsystem_t subsys);
void reset_guard_task(void);


#endif

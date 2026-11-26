
#ifndef WATCHDOG_H
    #define WATCHDOG_H

    #include "fault.h"

fault_t init_watchdog(void);
void kick_watchdog(void);

#endif
